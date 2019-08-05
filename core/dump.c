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

#include "include/compiler.h"
#include "include/name.h"
#include "include/vmx.h"
#include "include/dump.h"
#include "../include/hax.h"

static uint32_t dump_vmcs_list[] = {
    VMX_PIN_CONTROLS,
    VMX_PRIMARY_PROCESSOR_CONTROLS,
    VMX_SECONDARY_PROCESSOR_CONTROLS,
    VMX_EXCEPTION_BITMAP,
    VMX_PAGE_FAULT_ERROR_CODE_MASK,
    VMX_PAGE_FAULT_ERROR_CODE_MATCH,
    VMX_EXIT_CONTROLS,
    VMX_EXIT_MSR_STORE_COUNT,
    VMX_EXIT_MSR_LOAD_COUNT,
    VMX_ENTRY_CONTROLS,
    VMX_ENTRY_MSR_LOAD_COUNT,
    VMX_ENTRY_INTERRUPT_INFO,
    VMX_ENTRY_EXCEPTION_ERROR_CODE,
    VMX_ENTRY_INSTRUCTION_LENGTH,
    VMX_TPR_THRESHOLD,

    VMX_CR0_MASK,
    VMX_CR4_MASK,
    VMX_CR0_READ_SHADOW,
    VMX_CR4_READ_SHADOW,
    VMX_CR3_TARGET_COUNT,
    VMX_CR3_TARGET_VAL_BASE, // x6008-x6206

    VMX_VPID,
    VMX_IO_BITMAP_A,
    VMX_IO_BITMAP_B,
    VMX_MSR_BITMAP,
    VMX_EXIT_MSR_STORE_ADDRESS,
    VMX_EXIT_MSR_LOAD_ADDRESS,
    VMX_ENTRY_MSR_LOAD_ADDRESS,
    VMX_TSC_OFFSET,
    VMX_VAPIC_PAGE,
    VMX_APIC_ACCESS_PAGE,
    VMX_EPTP,
    VMX_PREEMPTION_TIMER,

    VMX_INSTRUCTION_ERROR_CODE,

    VM_EXIT_INFO_REASON,
    VM_EXIT_INFO_INTERRUPT_INFO,
    VM_EXIT_INFO_EXCEPTION_ERROR_CODE,
    VM_EXIT_INFO_IDT_VECTORING,
    VM_EXIT_INFO_IDT_VECTORING_ERROR_CODE,
    VM_EXIT_INFO_INSTRUCTION_LENGTH,
    VM_EXIT_INFO_INSTRUCTION_INFO,
    VM_EXIT_INFO_QUALIFICATION,
    VM_EXIT_INFO_IO_ECX,
    VM_EXIT_INFO_IO_ESI,
    VM_EXIT_INFO_IO_EDI,
    VM_EXIT_INFO_IO_EIP,
    VM_EXIT_INFO_GUEST_LINEAR_ADDRESS,
    VM_EXIT_INFO_GUEST_PHYSICAL_ADDRESS,

    HOST_RIP,
    HOST_RSP,
    HOST_CR0,
    HOST_CR3,
    HOST_CR4,

    HOST_CS_SELECTOR,
    HOST_DS_SELECTOR,
    HOST_ES_SELECTOR,
    HOST_FS_SELECTOR,
    HOST_GS_SELECTOR,
    HOST_SS_SELECTOR,
    HOST_TR_SELECTOR,
    HOST_FS_BASE,
    HOST_GS_BASE,
    HOST_TR_BASE,
    HOST_GDTR_BASE,
    HOST_IDTR_BASE,

    HOST_SYSENTER_CS,
    HOST_SYSENTER_ESP,
    HOST_SYSENTER_EIP,

    GUEST_RIP,
    GUEST_RFLAGS,
    GUEST_RSP,
    GUEST_CR0,
    GUEST_CR3,
    GUEST_CR4,

    GUEST_ES_SELECTOR,
    GUEST_CS_SELECTOR,
    GUEST_SS_SELECTOR,
    GUEST_DS_SELECTOR,
    GUEST_FS_SELECTOR,
    GUEST_GS_SELECTOR,
    GUEST_LDTR_SELECTOR,
    GUEST_TR_SELECTOR,

    GUEST_ES_AR,
    GUEST_CS_AR,
    GUEST_SS_AR,
    GUEST_DS_AR,
    GUEST_FS_AR,
    GUEST_GS_AR,
    GUEST_LDTR_AR,
    GUEST_TR_AR,

    GUEST_ES_BASE,
    GUEST_CS_BASE,
    GUEST_SS_BASE,
    GUEST_DS_BASE,
    GUEST_FS_BASE,
    GUEST_GS_BASE,
    GUEST_LDTR_BASE,
    GUEST_TR_BASE,
    GUEST_GDTR_BASE,
    GUEST_IDTR_BASE,

    GUEST_ES_LIMIT,
    GUEST_CS_LIMIT,
    GUEST_SS_LIMIT,
    GUEST_DS_LIMIT,
    GUEST_FS_LIMIT,
    GUEST_GS_LIMIT,
    GUEST_LDTR_LIMIT,
    GUEST_TR_LIMIT,
    GUEST_GDTR_LIMIT,
    GUEST_IDTR_LIMIT,

    GUEST_VMCS_LINK_PTR,
    GUEST_DEBUGCTL,
    GUEST_PAT,
    GUEST_EFER,
    GUEST_PERF_GLOBAL_CTRL,
    GUEST_PDPTE0,
    GUEST_PDPTE1,
    GUEST_PDPTE2,
    GUEST_PDPTE3,

    GUEST_DR7,
    GUEST_PENDING_DBE,
    GUEST_SYSENTER_CS,
    GUEST_SYSENTER_ESP,
    GUEST_SYSENTER_EIP,
    GUEST_SMBASE,
    GUEST_INTERRUPTIBILITY,
    GUEST_ACTIVITY_STATE,
};

void dump_vmcs(struct vcpu_t *vcpu)
{
    uint32_t i, enc, n;
    const char *name;

    uint32_t *list = dump_vmcs_list;
    n = ARRAY_ELEMENTS(dump_vmcs_list);

    for (i = 0; i < n; i++) {
        enc = list[i];
        name = name_vmcs_component(enc);
        vmread_dump(vcpu, enc, name);
    }
}

void dump_vmx_info(struct info_t *info)
{
    hax_log(HAX_LOGI, "VMCS Rev %d\n", info->_vmcs_revision_id);

    hax_log(HAX_LOGI, "VMX basic info       : 0x%016llX\n",
            info->_basic_info);
    hax_log(HAX_LOGI, "VMX misc info        : 0x%016llX\n",
            info->_miscellaneous);
    hax_log(HAX_LOGI, "VMX revision control : %u\n",
            info->_vmcs_revision_id);
    hax_log(HAX_LOGI, "VMX exit ctls        : 0x%X, 0x%X\n",
            info->exit_ctls_0, info->exit_ctls_1);
    hax_log(HAX_LOGI, "VMX entry ctls       : 0x%X, 0x%X\n",
            info->entry_ctls_0, info->entry_ctls_1);
    hax_log(HAX_LOGI, "VMX pin ctls         : 0x%X, 0x%X\n",
            info->pin_ctls_0, info->pin_ctls_1);
    hax_log(HAX_LOGI, "VMX cpu prim ctrls   : 0x%X, 0x%X\n",
            info->pcpu_ctls_0, info->pcpu_ctls_1);
    hax_log(HAX_LOGI, "VMX cpu sec ctrl     : 0x%X, 0x%X\n",
            info->scpu_ctls_0, info->scpu_ctls_1);
    hax_log(HAX_LOGI, "VMX fixed CR0 bits   : 0x%llX, %llX\n",
            info->_cr0_fixed_0, info->_cr0_fixed_1);
    hax_log(HAX_LOGI, "VMX fixed CR4 bits   : 0x%llX, %llX\n",
            info->_cr4_fixed_0, info->_cr4_fixed_1);
    hax_log(HAX_LOGI, "VMX EPT/VPID caps    : 0x%016llX\n",
            info->_ept_cap);
}

/*Remove this function. It only for debug*/
/*void dump_cs_ds(uint16_t cs, uint16_t ds)
{
    struct system_desc_t desc;
    struct seg_desc_t *seg_desc;

    get_kernel_gdt(&desc);

    seg_desc = (struct seg_desc_t *)((mword)desc._base) + (cs >> 3);

    hax_log(HAX_LOGD, "\nsel: %x\n", cs >> 3);
    hax_log(HAX_LOGD, "type: %x\n", seg_desc->_type);
    hax_log(HAX_LOGD, "s: %x\n", seg_desc->_s);
    hax_log(HAX_LOGD, "present: %x\n", seg_desc->_present);
    hax_log(HAX_LOGD, "avl: %x\n", seg_desc->_avl);
    hax_log(HAX_LOGD, "long: %x\n", seg_desc->_longmode);
    hax_log(HAX_LOGD, "d/b: %x\n", seg_desc->_d);
    hax_log(HAX_LOGD, "g: %x\n", seg_desc->_granularity);
    hax_log(HAX_LOGD, "base0: %x\n", seg_desc->_base0);
    hax_log(HAX_LOGD, "limit: %x\n", seg_desc->_limit0);
    hax_log(HAX_LOGD, "dpl: %x\n", seg_desc->_limit0);

    hax_log(HAX_LOGD, "raw: %llx\n", seg_desc->_raw);
    seg_desc = (struct seg_desc_t *)((mword)desc._base) + (ds >> 3);

    hax_log(HAX_LOGD, "\nsel: %x\n", ds >> 3);
    hax_log(HAX_LOGD, "type: %x\n", seg_desc->_type);
    hax_log(HAX_LOGD, "s: %x\n", seg_desc->_s);
    hax_log(HAX_LOGD, "present: %x\n", seg_desc->_present);
    hax_log(HAX_LOGD, "avl: %x\n", seg_desc->_avl);
    hax_log(HAX_LOGD, "long: %x\n", seg_desc->_longmode);
    hax_log(HAX_LOGD, "d/b: %x\n", seg_desc->_d);
    hax_log(HAX_LOGD, "g: %x\n", seg_desc->_granularity);
    hax_log(HAX_LOGD, "base0: %x\n", seg_desc->_base0);
    hax_log(HAX_LOGD, "limit: %x\n", seg_desc->_limit0);
    hax_log(HAX_LOGD, "dpl: %x\n", seg_desc->_dpl);
    hax_log(HAX_LOGD, "raw: %llx\n", seg_desc->_raw);
}*/
