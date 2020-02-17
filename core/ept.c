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

#include "include/ept.h"
#include "include/cpu.h"

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

    hax_log(HAX_LOGD, "[#%d] invept_smpfunc\n", cpu_data->cpu_id);

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
    uint32_t cpu_id, res;

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
    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        struct per_cpu_data *cpu_data;

        if (!cpu_is_online(&cpu_online_map, cpu_id)) {
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
            hax_log(HAX_LOGE, "[#%d] INVEPT was not called, because "
                    "VMXON failed (err=0x%x)\n", cpu_id, res);
        } else {
            res = (uint32_t)cpu_data->invept_res;
            if (res != VMX_SUCCEED) {
                hax_log(HAX_LOGE, "[#%d] INVEPT failed (err=0x%x)\n",
                        cpu_id, res);
            }
            res = (uint32_t)cpu_data->vmxoff_res;
            if (res != VMX_SUCCEED) {
                hax_log(HAX_LOGE, "[#%d] INVEPT was called, but "
                        "VMXOFF failed (err=0x%x)\n", cpu_id, res);
            }
        }
    }
}
