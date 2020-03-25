/*
 * Copyright (c) 2009 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "include/ia32_defs.h"
#include "include/vmx.h"

#include "include/cpu.h"
#include "include/config.h"
#include "include/hax_driver.h"
#include "include/vm.h"
#include "../include/hax.h"
#include "../include/hax_release_ver.h"

/* deal with module parameter */
struct config_t config = {
    .memory_pass_through         = 0,
    .disable_ept                 = 0,
    .ept_small_pages             = 1,
    .disable_vpid                = 1,
    .disable_unrestricted_guest  = 1,
    .no_cpuid_pass_through       = 1,
    .cpuid_pass_through          = 0,
    .cpuid_no_mwait              = 0,
    .no_msr_pass_through         = 0
};

struct hax_page *io_bitmap_page_a;
struct hax_page *io_bitmap_page_b;
struct hax_page *msr_bitmap_page;

hax_cpumap_t cpu_online_map;
struct per_cpu_data **hax_cpu_data;
struct hax_t *hax;

extern hax_atomic_t vmx_cpu_num, vmx_enabled_num;
static void hax_enable_vmx(void)
{
    hax_smp_call_function(&cpu_online_map, cpu_init_vmx, NULL);
}

static void hax_disable_vmx(void)
{
    hax_smp_call_function(&cpu_online_map, cpu_exit_vmx, NULL);
}

static void free_cpu_vmxon_region(void)
{
    uint32_t cpu_id;

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!cpu_is_online(&cpu_online_map, cpu_id) || !hax_cpu_data[cpu_id])
            continue;
        if (hax_cpu_data[cpu_id]->vmxon_page) {
            hax_free_pages(hax_cpu_data[cpu_id]->vmxon_page);
            hax_cpu_data[cpu_id]->vmxon_page = NULL;
        }
    }
}

static int alloc_cpu_vmxon_region(void)
{
    uint32_t cpu_id;
    struct hax_page *page;

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!cpu_is_online(&cpu_online_map, cpu_id) || !hax_cpu_data[cpu_id])
            continue;
        page = hax_alloc_page(0, 1);
        if (!page) {
            free_cpu_vmxon_region();
            return -ENOMEM;
        }
        hax_clear_page(page);
        hax_cpu_data[cpu_id]->vmxon_page = page;
    }
    return 0;
}

void free_cpu_template_vmcs(void)
{
    uint32_t cpu_id;

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!cpu_is_online(&cpu_online_map, cpu_id) || !hax_cpu_data[cpu_id])
            continue;
        if (hax_cpu_data[cpu_id]->vmcs_page) {
            hax_free_pages(hax_cpu_data[cpu_id]->vmcs_page);
            hax_cpu_data[cpu_id]->vmcs_page = NULL;
        }
    }
}

static int alloc_cpu_template_vmcs(void)
{
    uint32_t cpu_id;
    struct hax_page *page = NULL;

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!cpu_is_online(&cpu_online_map, cpu_id) || !hax_cpu_data[cpu_id])
            continue;
        page = (struct hax_page *)hax_alloc_page(0, 1);
        if (!page) {
            free_cpu_template_vmcs();
            return -ENOMEM;
        }
        hax_clear_page(page);
        hax_cpu_data[cpu_id]->vmcs_page = page;
    }
    return 0;
}

int hax_em64t_enabled(void)
{
    return hax->em64t_enable_flag;
}

#ifdef HAX_PLATFORM_DARWIN
hax_smp_func_ret_t smp_cfunction(void *param)
#endif
#ifdef HAX_PLATFORM_LINUX
hax_smp_func_ret_t smp_cfunction(void *param)
#endif
#ifdef HAX_PLATFORM_NETBSD
hax_smp_func_ret_t smp_cfunction(void *param, void *a2 __unused)
#endif
#ifdef HAX_PLATFORM_WINDOWS
hax_smp_func_ret_t smp_cfunction(void *param)
#endif
{
    struct smp_call_parameter *p = (struct smp_call_parameter *)param;
    void (*action)(void *parap) = p->func;
    hax_cpumap_t *hax_cpus = p->cpus;

    if (cpu_is_online(hax_cpus, hax_cpu_id()))
        action(p->param);

    return (hax_smp_func_ret_t)NULL;
}

/*
 * This vcpu_data should not be accessed by anyone else at this step.
 * Return 0 if can continue, <0 for error.
 */
static int hax_vmx_enable_check(void)
{
    int vts = 0, nxs = 0, vte = 0, nxe = 0, em64s = 0, em64e = 0, finished = 0;
    int tnum = 0, error = 0;
    uint32_t cpu_id;

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        struct per_cpu_data *cpu_data;

        if (!cpu_is_online(&cpu_online_map, cpu_id))
            continue;
        cpu_data = hax_cpu_data[cpu_id];
        // This should not happen !
        if (!cpu_data)
            continue;

        if (cpu_data->cpu_features & HAX_CPUF_VALID) {
            tnum++;
            if (cpu_data->cpu_features & HAX_CPUF_SUPPORT_VT) {
                vts++;
            }
            if (cpu_data->cpu_features & HAX_CPUF_SUPPORT_NX) {
                nxs++;
            }
            if (cpu_data->cpu_features & HAX_CPUF_SUPPORT_EM64T) {
                em64s++;
            }
            if (cpu_data->cpu_features & HAX_CPUF_ENABLE_VT) {
                vte++;
            }
            if (cpu_data->cpu_features & HAX_CPUF_ENABLE_NX) {
                nxe++;
            }
            if (cpu_data->cpu_features & HAX_CPUF_ENABLE_EM64T) {
                em64e++;
            }
            if (cpu_data->cpu_features & HAX_CPUF_INITIALIZED) {
                finished++;
            }
        }
    }
    if (vts != tnum) {
        hax_log(HAX_LOGE, "VT is not supported in the system, HAXM exits, "
                "sorry!\n");
        hax_notify_host_event(HaxNoVtEvent, NULL, 0);
        return -1;
    }

    if (nxs != tnum) {
        hax_log(HAX_LOGE, "NX is not supported in the system, HAXM exits, "
                "sorry!\n");
        hax_notify_host_event(HaxNoNxEvent, NULL, 0);
        return -1;
    }
#if 0
    if (em64s != tnum) {
        hax_log(HAX_LOGE, "EM64T is not supported in the system, HAXM exits, "
                "sorry!\n");
        hax_notify_host_event(HaxNoEMT64Event, NULL, 0);
        return -1;
    }
#endif
    if (nxe != tnum) {
        hax_log(HAX_LOGE, "NX is not enabled in the system, "
                "HAXM does not function.\n");
        error = 1;
        hax_notify_host_event(HaxNxDisable, NULL, 0);
    } else {
        hax->nx_enable_flag = 1;
    }

    if (vte != tnum) {
        hax_log(HAX_LOGE, "VT is not enabled in the system, "
                "HAXM does not function.\n");
        hax_notify_host_event(HaxVtDisable, NULL, 0);
        error = 1;
    } else {
        hax->vmx_enable_flag = 1;
    }

    if (em64e == tnum) {
        hax->em64t_enable_flag = 1;
    }

    // If UG exists, we want it.
    if ((ia32_rdmsr(IA32_VMX_MISC) & IA32_VMX_MISC_UG_AVAILABLE) != 0) {
        hax->ug_enable_flag = 1;
    } else {
        hax->ug_enable_flag = 0;
    }

    if ((error == 0) && (tnum != finished)) {
        hax_log(HAX_LOGE, "Unknown reason happens to VT init, HAXM exit\n");
        hax_notify_host_event(HaxVtEnableFailure, NULL, 0);
        return -1;
    }
    return 0;
}

static int hax_vmx_init(void)
{
    int ret = -ENOMEM;

    io_bitmap_page_a = (struct hax_page *)hax_alloc_page(0, 1);
    if (!io_bitmap_page_a)
        return -ENOMEM;
    hax_set_page(io_bitmap_page_a);

    io_bitmap_page_b = (struct hax_page *)hax_alloc_page(0, 1);
    if (!io_bitmap_page_b)
        goto out_1;
    hax_set_page(io_bitmap_page_b);

    msr_bitmap_page = (struct hax_page *)hax_alloc_page(0, 1);
    if (!msr_bitmap_page)
        goto out_2;
    hax_set_page(msr_bitmap_page);

    if ((ret = alloc_cpu_vmxon_region()) < 0)
        goto out_3;

    if ((ret = alloc_cpu_template_vmcs()) < 0)
        goto out_4;

    hax_enable_vmx();

    if ((ret = hax_vmx_enable_check()) < 0)
        goto out_5;

    return 0;
out_5:
    hax_disable_vmx();
    free_cpu_template_vmcs();
out_4:
    free_cpu_vmxon_region();
out_3:
    hax_free_pages(msr_bitmap_page);
out_2:
    hax_free_pages(io_bitmap_page_b);
out_1:
    hax_free_pages(io_bitmap_page_a);
    return ret;
}

static int hax_vmx_exit(void)
{
    hax_disable_vmx();
    free_cpu_template_vmcs();
    free_cpu_vmxon_region();
    hax_free_pages(msr_bitmap_page);
    hax_free_pages(io_bitmap_page_b);
    hax_free_pages(io_bitmap_page_a);
    return 0;
}

int hax_set_memlimit(void *buf, int bufLeng, int *outLength)
{
    struct hax_set_memlimit *memlimit = buf;

    hax_mutex_lock(hax->hax_lock);
    // We can't set the limit when VM created already.
    if (!hax_list_empty(&hax->hax_vmlist)) {
        hax_mutex_unlock(hax->hax_lock);
        return -EINVAL;
    }
    if (!memlimit->enable_memlimit) {
        hax->mem_limit = 0;
        hax_log(HAX_LOGE, "disable memlimit\n");
    } else {
        hax->mem_limit = hax->mem_quota = memlimit->memory_limit << 20;
        hax_log(HAX_LOGI, "set memlimit 0x%llx\n", hax->mem_limit);
    }
    hax_mutex_unlock(hax->hax_lock);
    return 0;
}

int hax_get_capability(void *buf, int bufLeng, int *outLength)
{
    struct hax_capabilityinfo *cap;

    cap = buf;
    if (bufLeng < sizeof(struct hax_capabilityinfo))
        return -EINVAL;

    if (!hax->vmx_enable_flag || !hax->nx_enable_flag) {
        cap->wstatus = 0;
        cap->winfo = 0;
        if (!hax->vmx_enable_flag) {
            cap->winfo |= HAX_CAP_FAILREASON_VT;
        }
        if (!hax->nx_enable_flag) {
            cap->winfo |= HAX_CAP_FAILREASON_NX;
        }
    } else {
        struct per_cpu_data *cpu_data = current_cpu_data();

        cap->wstatus = HAX_CAP_STATUS_WORKING;
        // Fast MMIO supported since API version 2
        cap->winfo = HAX_CAP_FASTMMIO;
        cap->winfo |= HAX_CAP_64BIT_RAMBLOCK;
        cap->winfo |= HAX_CAP_64BIT_SETRAM;
        cap->winfo |= HAX_CAP_IMPLICIT_RAMBLOCK;
        cap->winfo |= HAX_CAP_TUNNEL_PAGE;
        cap->winfo |= HAX_CAP_RAM_PROTECTION;
        cap->winfo |= HAX_CAP_DEBUG;
        cap->winfo |= HAX_CAP_CPUID;
        if (cpu_data->vmx_info._ept_cap) {
            cap->winfo |= HAX_CAP_EPT;
        }
        if (hax->ug_enable_flag) {
            cap->winfo |= HAX_CAP_UG;
        }
    }

    if (hax->mem_limit) {
        cap->wstatus |= HAX_CAP_MEMQUOTA;
        cap->mem_quota = hax->mem_quota;
    }

    if (outLength) {
        *outLength = sizeof(struct hax_capabilityinfo);
    }
    return 0;
}

/*
 * Allows the guest to read from and/or write to the specified MSRs without
 * causing a VM exit.
 * |start| is the start MSR address, |count| the number of MSRs. Together they
 * specify a range of consecutive MSR addresses.
 * |read| and |write| determine if each MSR can be read or written freely by the
 * guest, respectively.
 */
static void set_msr_access(uint32_t start, uint32_t count, bool read, bool write)
{
    uint32_t end = start + count - 1;
    uint32_t read_base, write_base, bit;
    uint8_t *msr_bitmap = hax_page_va(msr_bitmap_page);

    hax_assert(((start ^ (start << 1)) & 0x80000000) == 0);
    hax_assert((start & 0x3fffe000) == 0);
    hax_assert(((start ^ end) & 0xffffe000) == 0);
    hax_assert(msr_bitmap);

    // See IA SDM Vol. 3C 24.6.9 for the layout of the MSR bitmaps page
    read_base = start & 0x80000000 ? 1024 : 0;
    write_base = read_base + 2048;
    for (bit = (start & 0x1fff); bit <= (end & 0x1fff); bit++) {
        // Bit clear means allowed
        if (read) {
            btr(msr_bitmap + read_base, bit);
        } else {
            bts(msr_bitmap + read_base, bit);
        }

        if (write) {
            btr(msr_bitmap + write_base, bit);
        } else {
            bts(msr_bitmap + write_base, bit);
        }
    }
}

/*
 * Probes the host CPU to determine its performance monitoring capabilities.
 */
static void hax_pmu_init(void)
{
    uint32_t cpu_id, ref_cpu_id = (uint32_t)(~0ULL);

    // Execute cpu_pmu_init() on each logical processor of the host CPU
    hax_smp_call_function(&cpu_online_map, cpu_pmu_init, NULL);

    // Find the common APM version supported by all host logical processors
    // TODO: Theoretically we should do the same for other APM parameters
    // (number of counters, etc.) as well
    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        struct per_cpu_data *cpu_data;
        uint apm_version;

        if (!cpu_is_online(&cpu_online_map, cpu_id)) {
            continue;
        }
        cpu_data = hax_cpu_data[cpu_id];
        // Should never happen
        if (!cpu_data) {
            hax_log(HAX_LOGW, "hax_pmu_init: hax_cpu_data[%d] is NULL\n",
                    cpu_id);
            continue;
        }

        apm_version = cpu_data->pmu_info.apm_version;
        if (!hax->apm_version || apm_version < hax->apm_version) {
            hax->apm_version = apm_version;
            ref_cpu_id = cpu_id;
        }
    }

    if (hax->apm_version) {
        struct cpu_pmu_info *ref_pmu_info, *pmu_info;
        uint apm_general_bitlen, apm_fixed_bitlen;

        ref_pmu_info = &hax_cpu_data[ref_cpu_id]->pmu_info;
        // IA SDM Vol. 3B 18.2 describes APM version 1 through 4, which can be
        // implemented in an incremental manner
        // TODO: Implement APM version 2
        if (hax->apm_version > 1) {
            hax->apm_version = 1;
        }
        hax->apm_general_count =
                ref_pmu_info->apm_general_count > APM_MAX_GENERAL_COUNT
                ? APM_MAX_GENERAL_COUNT : ref_pmu_info->apm_general_count;
        apm_general_bitlen = ref_pmu_info->apm_general_bitlen;
        hax->apm_general_mask = apm_general_bitlen > 63 ? (uint64_t)-1
                                : (1ULL << apm_general_bitlen) - 1;
        hax->apm_event_count =
                ref_pmu_info->apm_event_count > APM_MAX_EVENT_COUNT
                ? APM_MAX_EVENT_COUNT : ref_pmu_info->apm_event_count;
        hax->apm_event_unavailability = ref_pmu_info->apm_event_unavailability &
                                        ((1UL << hax->apm_event_count) - 1);
        hax_log(HAX_LOGI, "APM: version %u\n", hax->apm_version);
        hax_log(HAX_LOGI, "APM: %u general-purpose counters, bitmask 0x%llx\n",
                hax->apm_general_count, hax->apm_general_mask);
        hax_log(HAX_LOGI, "APM: %u events, unavailability 0x%x\n",
                hax->apm_event_count, hax->apm_event_unavailability);

        set_msr_access(IA32_PMC0, hax->apm_general_count, true, true);
        set_msr_access(IA32_PERFEVTSEL0, hax->apm_general_count, true, true);

        if (hax->apm_version > 1) {
            hax->apm_fixed_count =
                    ref_pmu_info->apm_fixed_count > APM_MAX_FIXED_COUNT
                    ? APM_MAX_FIXED_COUNT : ref_pmu_info->apm_fixed_count;
            apm_fixed_bitlen = ref_pmu_info->apm_fixed_bitlen;
            hax->apm_fixed_mask = apm_fixed_bitlen > 63 ? (uint64_t)-1
                                  : (1ULL << apm_fixed_bitlen) - 1;
            hax_log(HAX_LOGI, "APM: %u fixed-function counters, bitmask "
                    "0x%llx\n",hax->apm_fixed_count, hax->apm_fixed_mask);
        } else {
            hax->apm_fixed_count = 0;
            apm_fixed_bitlen = 0;
            hax->apm_fixed_mask = 0;
        }

        // Copy the common APM parameters to hax->apm_cpuid_0xa, so as to
        // simplify CPUID virtualization
        pmu_info = &hax->apm_cpuid_0xa;
        pmu_info->apm_version = hax->apm_version;
        pmu_info->apm_general_count = hax->apm_general_count;
        pmu_info->apm_general_bitlen = apm_general_bitlen;
        pmu_info->apm_event_count = hax->apm_event_count;
        pmu_info->apm_event_unavailability = hax->apm_event_unavailability;
        pmu_info->apm_fixed_count = hax->apm_fixed_count;
        pmu_info->apm_fixed_bitlen = apm_fixed_bitlen;
    } else {
        hax_log(HAX_LOGW, "Host CPU does not support APM\n");
        hax->apm_general_count = 0;
        hax->apm_general_mask = 0;
        hax->apm_event_count = 0;
        hax->apm_event_unavailability = 0;
        hax->apm_fixed_count = 0;
        hax->apm_fixed_mask = 0;
    }
}

int hax_module_init(void)
{
    uint32_t cpu_id;

    hax = (struct hax_t *)hax_vmalloc(sizeof(struct hax_t), HAX_MEM_NONPAGE);
    if (!hax)
        return -ENOMEM;

    hax->mem_quota = hax->mem_limit = hax_get_memory_threshold();
    hax->hax_lock = hax_mutex_alloc_init();
    if (!hax->hax_lock)
        goto out_0;

    hax_cpu_data = hax_vmalloc(cpu_online_map.cpu_num * sizeof(void *), 0);
    if (!hax_cpu_data)
        goto out_1;
    memset(hax_cpu_data, 0, cpu_online_map.cpu_num * sizeof(void *));

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!cpu_is_online(&cpu_online_map, cpu_id))
            continue;
        hax_cpu_data[cpu_id] = hax_vmalloc(sizeof(struct per_cpu_data), 0);
        if (!hax_cpu_data[cpu_id])
            goto out_2;
        memset(hax_cpu_data[cpu_id], 0, sizeof(struct per_cpu_data));

        hax_cpu_data[cpu_id]->hstate.hfxpage =
                (struct hax_page *)hax_alloc_page(0, 1);
        if (!hax_cpu_data[cpu_id]->hstate.hfxpage)
            goto out_2;
        hax_clear_page(hax_cpu_data[cpu_id]->hstate.hfxpage);
        hax_cpu_data[cpu_id]->cpu_id = cpu_id;
    }

    cpuid_host_init();
    cpuid_init_supported_features();

    if (hax_vmx_init() < 0)
        goto out_2;

    hax_pmu_init();

    hax_init_list_head(&hax->hax_vmlist);
    hax_log(HAX_LOGW, "-------- HAXM v%s Start --------\n",
            HAXM_RELEASE_VERSION_STR);

    return 0;

out_2:
    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (hax_cpu_data[cpu_id]) {
            if (hax_cpu_data[cpu_id]->hstate.hfxpage) {
                hax_free_pages(hax_cpu_data[cpu_id]->hstate.hfxpage);
            }
            hax_vfree(hax_cpu_data[cpu_id], sizeof(struct per_cpu_data));
        }
    }
    hax_vfree(hax_cpu_data, cpu_online_map.cpu_num * sizeof(void *));
out_1:
    hax_mutex_free(hax->hax_lock);
out_0:
    hax_vfree(hax, sizeof(struct hax_t));
    return -ENOMEM;
}

int hax_module_exit(void)
{
    int ret;
    uint32_t cpu_id;

    if (!hax_list_empty(&hax->hax_vmlist)) {
        hax_log(HAX_LOGE, "Still VM not be destroyed?\n");
        return -EBUSY;
    }

    ret = hax_destroy_host_interface();
    if (ret < 0)
        return ret;

    hax_vmx_exit();
    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!hax_cpu_data[cpu_id])
            continue;
        if (hax_cpu_data[cpu_id]->hstate.hfxpage) {
            hax_free_pages(hax_cpu_data[cpu_id]->hstate.hfxpage);
        }
        hax_vfree(hax_cpu_data[cpu_id], sizeof(struct per_cpu_data));
    }
    hax_vfree(hax_cpu_data, cpu_online_map.cpu_num * sizeof(void *));
    cpu_info_exit();
    hax_mutex_free(hax->hax_lock);
    hax_vfree(hax, sizeof(struct hax_t));
    hax_log(HAX_LOGW, "-------- HAXM v%s End --------\n",
            HAXM_RELEASE_VERSION_STR);

    return 0;
}
