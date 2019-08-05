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

/* Design rule:
 * 1. EPT page table is used as a p2m mapping in vTLB case.
 * 2. Only support EPT_MAX_MEM_G memory for the guest at maximum
 * 3. EPT table is preallocated at VM initilization stage.
 * 4. Doesn't support super page.
 * 5. To traverse it easily, the uppest three levels are designed as the fixed
 *    mapping.
 */

#include "../include/hax.h"
#include "include/ept.h"
#include "include/cpu.h"
#include "include/paging.h"
#include "include/vtlb.h"

static uint64_t ept_capabilities;

#define EPT_BASIC_CAPS  (ept_cap_WB | ept_cap_invept_ac)

bool ept_set_caps(uint64_t caps)
{
    if ((caps & EPT_BASIC_CAPS) != EPT_BASIC_CAPS) {
        hax_log(HAX_LOGW, "Broken host EPT support detected (caps=0x%llx)\n",
                caps);
        return 0;
    }

    // ept_cap_invept_ac implies ept_cap_invept
    if (!(caps & ept_cap_invept)) {
        hax_log(HAX_LOGW, "ept_set_caps: Assuming support for INVEPT "
                "(caps=0x%llx)\n", caps);
        caps |= ept_cap_invept;
    }

    caps &= ~EPT_UNSUPPORTED_FEATURES;
    hax_assert(!ept_capabilities || caps == ept_capabilities);
    // FIXME: This assignment is done by all logical processors simultaneously
    ept_capabilities = caps;
    return 1;
}

static bool ept_has_cap(uint64_t cap)
{
    hax_assert(ept_capabilities != 0);
    // Avoid implicit conversion from uint64_t to bool, because the latter may be
    // typedef'ed as uint8_t (see hax_types_windows.h)
    return (ept_capabilities & cap) != 0;
}

// Get the PDE entry for the specified gpa in EPT
static epte_t * ept_get_pde(struct hax_ept *ept, hax_paddr_t gpa)
{
    epte_t *e;
    uint which_g = gpa >> 30;
    // PML4 and PDPTE level needs 2 pages
    uint64_t offset = (2 + which_g) * PAGE_SIZE_4K;
    // Need Xiantao's check
    unsigned char *ept_addr = hax_page_va(ept->ept_root_page);

    hax_assert(which_g < EPT_MAX_MEM_G);

    e = (epte_t *)(ept_addr + offset) + ept_get_pde_idx(gpa);
    return e;
}

// ept_set_pte: caller can use it to setup p2m mapping for the guest.
bool ept_set_pte(hax_vm_t *hax_vm, hax_paddr_t gpa, hax_paddr_t hpa, uint emt,
                 uint mem_type, bool *is_modified)
{
    bool ret = true;
    struct hax_page *page;
    hax_paddr_t pte_ha;
    epte_t *pte;
    void *pte_base, *addr;
    struct hax_ept *ept = hax_vm->ept;
    uint which_g = gpa >> 30;
    uint perm;
    epte_t *pde = ept_get_pde(ept, gpa);

    // hax_log(HAX_LOGD, "hpa %llx gpa %llx\n", hpa, gpa);
    if (which_g >= EPT_MAX_MEM_G) {
        hax_log(HAX_LOGE, "Error: Guest's memory size is beyond %dG!\n",
                EPT_MAX_MEM_G);
        return false;
    }
    hax_mutex_lock(hax_vm->vm_lock);
    if (!epte_is_present(pde)) {
        if (mem_type == EPT_TYPE_NONE) {  // unmap
            // Don't bother allocating the PT
            goto out_unlock;
        }

        page = hax_alloc_page(0, 1);
        if (!page) {
            ret = false;
            goto out_unlock;
        }

        hax_list_add(&page->list, &ept->ept_page_list);
        addr = hax_page_va(page);
        memset(addr, 0, PAGE_SIZE_4K);
        pte_ha = hax_page_pa(page);
        // Always own full access rights
        epte_set_entry(pde, pte_ha, 7, EMT_NONE);
    }

    // Grab the PTE entry
    pte_base = hax_vmap_pfn(pde->addr);
    if (!pte_base) {
        ret = false;
        goto out_unlock;
    }
    pte = (epte_t *)pte_base + ept_get_pte_idx(gpa);
    // TODO: Just for debugging, need check QEMU for more information
    /* if (epte_is_present(pte)) {
     *     hax_log(HAX_LOGD, "Can't change the pte entry!\n");
     *     hax_mutex_unlock(hax_vm->vm_lock);
     *     hax_log(HAX_LOGD, "\npte %llx\n", pte->val);
     *     hax_vunmap_pfn(pte_base);
     *     return 0;
     * }
     */
    switch (mem_type) {
        case EPT_TYPE_NONE: {
            perm = 0;  // unmap
            break;
        }
        case EPT_TYPE_MEM: {
            perm = 7;
            break;
        }
        case EPT_TYPE_ROM: {
            perm = 5;
            break;
        }
        default: {
            hax_log(HAX_LOGE, "Unsupported mapping type 0x%x\n", mem_type);
            ret = false;
            goto out_unmap;
        }
    }
    *is_modified = epte_is_present(pte) && (epte_get_address(pte) != hpa ||
                   epte_get_perm(pte) != perm || epte_get_emt(pte) != emt);
    epte_set_entry(pte, hpa, perm, emt);

out_unmap:
    hax_vunmap_pfn(pte_base);
out_unlock:
    hax_mutex_unlock(hax_vm->vm_lock);
    return ret;
}

static bool ept_lookup(struct vcpu_t *vcpu, hax_paddr_t gpa, hax_paddr_t *hpa)
{
    epte_t *pde, *pte;
    void *pte_base;
    struct hax_ept *ept = vcpu->vm->ept;
    uint which_g = gpa >> 30;

    hax_assert(ept->ept_root_page);
    if (which_g >= EPT_MAX_MEM_G) {
        hax_log(HAX_LOGD, "ept_lookup error!\n");
        return 0;
    }

    pde = ept_get_pde(ept, gpa);

    if (!epte_is_present(pde))
        return 0;

    pte_base = hax_vmap_pfn(pde->addr);
    if (!pte_base)
        return 0;

    pte = (epte_t *)pte_base + ept_get_pte_idx(gpa);

    if (!epte_is_present(pte)) {
        hax_vunmap_pfn(pte_base);
        return 0;
    }

    *hpa = (pte->addr << 12) | (gpa & 0xfff);
    hax_vunmap_pfn(pte_base);
    return 1;
}

/*
 * Deprecated API of EPT
 * Translate a GPA to an HPA
 * @param vcpu:     current vcpu structure pointer
 * @param gpa:      guest physical address
 * @param order:    order for gpa
 * @param hpa       host physical address pointer
 */

// TODO: Do we need to consider cross-page case ??
bool ept_translate(struct vcpu_t *vcpu, hax_paddr_t gpa, uint order, hax_paddr_t *hpa)
{
    hax_assert(order == PG_ORDER_4K);
    return ept_lookup(vcpu, gpa, hpa);
}

static eptp_t ept_construct_eptp(hax_paddr_t addr)
{
    eptp_t eptp;
    eptp.val = 0;
    eptp.emt = EMT_WB;
    eptp.gaw = EPT_DEFAULT_GAW;
    eptp.asr = addr >> PG_ORDER_4K;
    return eptp;
}

bool ept_init(hax_vm_t *hax_vm)
{
    uint i;
    hax_paddr_t hpa;
    // Need Xiantao's check
    unsigned char *ept_addr;
    epte_t *e;
    struct hax_page *page;
    struct hax_ept *ept;

    if (hax_vm->ept) {
        hax_log(HAX_LOGD, "EPT: EPT has been created already!\n");
        return 0;
    }

    ept = hax_vmalloc(sizeof(struct hax_ept), 0);
    if (!ept) {
        hax_log(HAX_LOGD,
                "EPT: No enough memory for creating EPT structure!\n");
        return 0;
    }
    memset(ept, 0, sizeof(struct hax_ept));
    hax_vm->ept = ept;

    page = hax_alloc_pages(EPT_PRE_ALLOC_PG_ORDER, 0, 1);
    if (!page) {
        hax_log(HAX_LOGD, "EPT: No enough memory for creating ept table!\n");
        hax_vfree(hax_vm->ept, sizeof(struct hax_ept));
        return 0;
    }
    ept->ept_root_page = page;
    ept_addr = hax_page_va(page);
    memset(ept_addr, 0, EPT_PRE_ALLOC_PAGES * PAGE_SIZE_4K);

    // One page for building PML4 level
    ept->eptp = ept_construct_eptp(hax_pa(ept_addr));
    e = (epte_t *)ept_addr;

    // One page for building PDPTE level
    ept_addr += PAGE_SIZE_4K;
    hpa = hax_pa(ept_addr);
    epte_set_entry(e, hpa, 7, EMT_NONE);
    e = (epte_t *)ept_addr;

    // The rest pages are used to build PDE level
    for (i = 0; i < EPT_MAX_MEM_G; i++) {
        ept_addr += PAGE_SIZE_4K;
        hpa = hax_pa(ept_addr);
        epte_set_entry(e + i, hpa, 7, EMT_NONE);
    }

    hax_init_list_head(&ept->ept_page_list);

    hax_log(HAX_LOGI, "ept_init: Calling INVEPT\n");
    invept(hax_vm, EPT_INVEPT_SINGLE_CONTEXT);
    return 1;
}

// Free the whole ept structure
void ept_free (hax_vm_t *hax_vm)
{
    struct hax_page *page, *n;
    struct hax_ept *ept = hax_vm->ept;

    hax_assert(ept);

    if (!ept->ept_root_page)
        return;

    hax_log(HAX_LOGI, "ept_free: Calling INVEPT\n");
    invept(hax_vm, EPT_INVEPT_SINGLE_CONTEXT);
    hax_list_entry_for_each_safe(page, n, &ept->ept_page_list, struct hax_page,
                                 list) {
        hax_list_del(&page->list);
        hax_free_page(page);
    }

    hax_free_pages(ept->ept_root_page);
    hax_vfree(hax_vm->ept, sizeof(struct hax_ept));
    hax_vm->ept = 0;
}

struct invept_bundle {
    uint type;
    struct invept_desc *desc;
};

static void invept_smpfunc(struct invept_bundle *bundle)
{
    struct per_cpu_data *cpu_data;

    hax_smp_mb();
    cpu_data = current_cpu_data();
    cpu_data->invept_res = VMX_SUCCEED;

    cpu_vmxroot_enter();

    if (cpu_data->vmxon_res == VMX_SUCCEED) {
        cpu_data->invept_res = asm_invept(bundle->type, bundle->desc);
        cpu_vmxroot_leave();
    }
}

void invept(hax_vm_t *hax_vm, uint type)
{
    uint64_t eptp_value = vm_get_eptp(hax_vm);
    struct invept_desc desc = { eptp_value, 0 };
    struct invept_bundle bundle;
    int cpu_id;
    uint32_t res;

    if (!ept_has_cap(ept_cap_invept)) {
        hax_log(HAX_LOGW, "INVEPT was not called due to missing host support"
                " (ept_capabilities=0x%llx)\n", ept_capabilities);
        return;
    }

    switch (type) {
        case EPT_INVEPT_SINGLE_CONTEXT: {
            if (ept_has_cap(ept_cap_invept_cw))
                break;
            type = EPT_INVEPT_ALL_CONTEXT;
            // fall through
        }
        case EPT_INVEPT_ALL_CONTEXT: {
            if (ept_has_cap(ept_cap_invept_ac))
                break;
            // fall through
        }
        default: {
            hax_panic("Invalid invept type %u\n", type);
        }
    }

    bundle.type = type;
    bundle.desc = &desc;
    hax_smp_call_function(&cpu_online_map, (void (*)(void *))invept_smpfunc,
                      &bundle);

    /*
     * It is not safe to call hax_log(), etc. from invept_smpfunc(),
     * especially on macOS; instead, invept_smpfunc() writes VMX instruction
     * results in hax_cpu_data[], which are checked below.
     */
    for (cpu_id = 0; cpu_id < max_cpus; cpu_id++) {
        struct per_cpu_data *cpu_data;

        if (!cpu_is_online(cpu_id)) {
            continue;
        }
        cpu_data = hax_cpu_data[cpu_id];
        if (!cpu_data) {
            // Should never happen
            hax_log(HAX_LOGW, "invept: hax_cpu_data[%d] is NULL\n", cpu_id);
            continue;
        }

        res = (uint32_t)cpu_data->vmxon_res;
        if (res != VMX_SUCCEED) {
            hax_log(HAX_LOGE, "[Processor #%d] INVEPT was not called, because "
                    "VMXON failed (err=0x%x)\n", cpu_id, res);
        } else {
            res = (uint32_t)cpu_data->invept_res;
            if (res != VMX_SUCCEED) {
                hax_log(HAX_LOGE, "[Processor #%d] INVEPT failed (err=0x%x)\n",
                        cpu_id, res);
            }
            res = (uint32_t)cpu_data->vmxoff_res;
            if (res != VMX_SUCCEED) {
                hax_log(HAX_LOGE, "[Processor #%d] INVEPT was called, but "
                        "VMXOFF failed (err=0x%x)\n", cpu_id, res);
            }
        }
    }
}

uint64_t vcpu_get_eptp(struct vcpu_t *vcpu)
{
    struct hax_ept *ept = vcpu->vm->ept;

    if (vcpu->mmu->mmu_mode != MMU_MODE_EPT)
        return INVALID_EPTP;
    return ept->eptp.val;
}
