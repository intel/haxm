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

#include "include/vmx.h"
#include "include/dump_vmcs.h"
#include "../include/hax.h"

extern unsigned char **vmcs_names;
extern uint32 vmcs_hash(uint32 enc);

static uint32 dump_vmcs_list[] = {
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

static int encode_type(uint32 encode)
{
    return (encode >> 13) & 0x3;
}

#define VMCS_NAME_NUMBER 256
/* including the trailing 0 */
#define VMCS_NAME_MAX_ENTRY 0x40

unsigned char *get_vmcsname_entry(int num)
{
    unsigned char *entry;
    entry = (unsigned char *)vmcs_names + num * VMCS_NAME_MAX_ENTRY;
    return entry;
}

void dump_vmcs(struct vcpu_t *vcpu)
{
    uint32 i, enc, n;
    unsigned char *name;

    uint32 *list = dump_vmcs_list;
    n = ARRAY_ELEMENTS(dump_vmcs_list);

    for (i = 0; i < n; i++) {
        enc = list[i];
        name = get_vmcsname_entry(vmcs_hash(enc));
        vmread_dump(vcpu, enc, (char *)name);
    }
}

static void setup_vmcs_name(int item, char *s)
{
    char *dest;
    int i = 0;
    int len = strlen(s) + 1;

    if (!vmcs_names)
        return;
    dest = (char *)vmcs_names + item * VMCS_NAME_MAX_ENTRY;

    if (len >= VMCS_NAME_MAX_ENTRY)
        len = VMCS_NAME_MAX_ENTRY;

    while(((*dest++ = *s++) != '\0') && (i < len))
        i++;
}

void dump_vmcs_exit(void)
{
    if (!vmcs_names)
        return;
    hax_vfree(vmcs_names, VMCS_NAME_NUMBER * VMCS_NAME_MAX_ENTRY);
}

int dump_vmcs_init(void)
{
    vmcs_names = hax_vmalloc( VMCS_NAME_NUMBER * VMCS_NAME_MAX_ENTRY, 0);

    if (!vmcs_names)
        return -ENOMEM;

    setup_vmcs_name(10, "VMX_PIN_CONTROLS");
    setup_vmcs_name(6, "VMX_PRIMARY_PROCESSOR_CONTROLS");
    setup_vmcs_name(35, "VMX_SECONDARY_PROCESSOR_CONTROLS");
    setup_vmcs_name(2, "VMX_EXCEPTION_BITMAP");
    setup_vmcs_name(18, "VMX_PAGE_FAULT_ERROR_CODE_MASK");
    setup_vmcs_name(14, "VMX_PAGE_FAULT_ERROR_CODE_MATCH");
    setup_vmcs_name(62, "VMX_EXIT_CONTROLS");
    setup_vmcs_name(87, "VMX_EXIT_MSR_STORE_COUNT");
    setup_vmcs_name(132, "VMX_EXIT_MSR_LOAD_COUNT");
    setup_vmcs_name(140, "VMX_ENTRY_CONTROLS");
    setup_vmcs_name(136, "VMX_ENTRY_MSR_LOAD_COUNT");
    setup_vmcs_name(147, "VMX_ENTRY_INTERRUPT_INFO");
    setup_vmcs_name(77, "VMX_ENTRY_EXCEPTION_ERROR_CODE");
    setup_vmcs_name(69, "VMX_ENTRY_INSTRUCTION_LENGTH");
    setup_vmcs_name(73, "VMX_TPR_THRESHOLD");
    setup_vmcs_name(51, "VMX_CR0_MASK");
    setup_vmcs_name(47, "VMX_CR4_MASK");
    setup_vmcs_name(43, "VMX_CR0_READ_SHADOW");
    setup_vmcs_name(59, "VMX_CR4_READ_SHADOW");
    setup_vmcs_name(39, "VMX_CR3_TARGET_COUNT");
    setup_vmcs_name(55, "VMX_CR3_TARGET_VAL_BASE");
    setup_vmcs_name(56, "VMX_VPID");
    setup_vmcs_name(114, "VMX_IO_BITMAP_A");
    setup_vmcs_name(110, "VMX_IO_BITMAP_B");
    setup_vmcs_name(106, "VMX_MSR_BITMAP");
    setup_vmcs_name(122, "VMX_EXIT_MSR_STORE_ADDRESS");
    setup_vmcs_name(118, "VMX_EXIT_MSR_LOAD_ADDRESS");
    setup_vmcs_name(143, "VMX_ENTRY_MSR_LOAD_ADDRESS");
    setup_vmcs_name(236, "VMX_TSC_OFFSET");
    setup_vmcs_name(244, "VMX_VAPIC_PAGE");
    setup_vmcs_name(240, "VMX_APIC_ACCESS_PAGE");
    setup_vmcs_name(173, "VMX_EPTP");
    setup_vmcs_name(48, "VMX_PREEMPTION_TIMER");
    setup_vmcs_name(50, "VMX_INSTRUCTION_ERROR_CODE");
    setup_vmcs_name(46, "VM_EXIT_INFO_REASON");
    setup_vmcs_name(42, "VM_EXIT_INFO_INTERRUPT_INFO");
    setup_vmcs_name(58, "VM_EXIT_INFO_EXCEPTION_ERROR_CODE");
    setup_vmcs_name(54, "VM_EXIT_INFO_IDT_VECTORING");
    setup_vmcs_name(79, "VM_EXIT_INFO_IDT_VECTORING_ERROR_CODE");
    setup_vmcs_name(102, "VM_EXIT_INFO_INSTRUCTION_LENGTH");
    setup_vmcs_name(127, "VM_EXIT_INFO_INSTRUCTION_INFO");
    setup_vmcs_name(115, "VM_EXIT_INFO_QUALIFICATION");
    setup_vmcs_name(111, "VM_EXIT_INFO_IO_ECX");
    setup_vmcs_name(107, "VM_EXIT_INFO_IO_ESI");
    setup_vmcs_name(123, "VM_EXIT_INFO_IO_EDI");
    setup_vmcs_name(119, "VM_EXIT_INFO_IO_EIP");
    setup_vmcs_name(144, "VM_EXIT_INFO_GUEST_LINEAR_ADDRESS");
    setup_vmcs_name(53, "VM_EXIT_INFO_GUEST_PHYSICAL_ADDRESS");
    setup_vmcs_name(148, "HOST_RIP");
    setup_vmcs_name(137, "HOST_RSP");
    setup_vmcs_name(11, "HOST_CR0");
    setup_vmcs_name(7, "HOST_CR3");
    setup_vmcs_name(3, "HOST_CR4");
    setup_vmcs_name(96, "HOST_CS_SELECTOR");
    setup_vmcs_name(108, "HOST_DS_SELECTOR");
    setup_vmcs_name(100, "HOST_ES_SELECTOR");
    setup_vmcs_name(104, "HOST_FS_SELECTOR");
    setup_vmcs_name(129, "HOST_GS_SELECTOR");
    setup_vmcs_name(92, "HOST_SS_SELECTOR");
    setup_vmcs_name(152, "HOST_TR_SELECTOR");
    setup_vmcs_name(19, "HOST_FS_BASE");
    setup_vmcs_name(15, "HOST_GS_BASE");
    setup_vmcs_name(40, "HOST_TR_BASE");
    setup_vmcs_name(63, "HOST_GDTR_BASE");
    setup_vmcs_name(88, "HOST_IDTR_BASE");
    setup_vmcs_name(41, "HOST_SYSENTER_CS");
    setup_vmcs_name(133, "HOST_SYSENTER_ESP");
    setup_vmcs_name(141, "HOST_SYSENTER_EIP");
    setup_vmcs_name(33, "GUEST_RIP");
    setup_vmcs_name(44, "GUEST_RFLAGS");
    setup_vmcs_name(71, "GUEST_RSP");
    setup_vmcs_name(8, "GUEST_CR0");
    setup_vmcs_name(4, "GUEST_CR3");
    setup_vmcs_name(0, "GUEST_CR4");
    setup_vmcs_name(74, "GUEST_ES_SELECTOR");
    setup_vmcs_name(70, "GUEST_CS_SELECTOR");
    setup_vmcs_name(66, "GUEST_SS_SELECTOR");
    setup_vmcs_name(82, "GUEST_DS_SELECTOR");
    setup_vmcs_name(78, "GUEST_FS_SELECTOR");
    setup_vmcs_name(103, "GUEST_GS_SELECTOR");
    setup_vmcs_name(126, "GUEST_LDTR_SELECTOR");
    setup_vmcs_name(151, "GUEST_TR_SELECTOR");
    setup_vmcs_name(135, "GUEST_ES_AR");
    setup_vmcs_name(146, "GUEST_CS_AR");
    setup_vmcs_name(76, "GUEST_SS_AR");
    setup_vmcs_name(68, "GUEST_DS_AR");
    setup_vmcs_name(72, "GUEST_FS_AR");
    setup_vmcs_name(34, "GUEST_GS_AR");
    setup_vmcs_name(45, "GUEST_LDTR_AR");
    setup_vmcs_name(31, "GUEST_TR_AR");
    setup_vmcs_name(16, "GUEST_ES_BASE");
    setup_vmcs_name(12, "GUEST_CS_BASE");
    setup_vmcs_name(37, "GUEST_SS_BASE");
    setup_vmcs_name(60, "GUEST_DS_BASE");
    setup_vmcs_name(85, "GUEST_FS_BASE");
    setup_vmcs_name(130, "GUEST_GS_BASE");
    setup_vmcs_name(138, "GUEST_LDTR_BASE");
    setup_vmcs_name(134, "GUEST_TR_BASE");
    setup_vmcs_name(145, "GUEST_GDTR_BASE");
    setup_vmcs_name(75, "GUEST_IDTR_BASE");
    setup_vmcs_name(9, "GUEST_ES_LIMIT");
    setup_vmcs_name(5, "GUEST_CS_LIMIT");
    setup_vmcs_name(1, "GUEST_SS_LIMIT");
    setup_vmcs_name(17, "GUEST_DS_LIMIT");
    setup_vmcs_name(13, "GUEST_FS_LIMIT");
    setup_vmcs_name(38, "GUEST_GS_LIMIT");
    setup_vmcs_name(61, "GUEST_LDTR_LIMIT");
    setup_vmcs_name(86, "GUEST_TR_LIMIT");
    setup_vmcs_name(131, "GUEST_GDTR_LIMIT");
    setup_vmcs_name(139, "GUEST_IDTR_LIMIT");
    setup_vmcs_name(28, "GUEST_VMCS_LINK_PTR");
    setup_vmcs_name(24, "GUEST_DEBUGCTL");
    setup_vmcs_name(20, "GUEST_PAT");
    setup_vmcs_name(36, "GUEST_EFER");
    setup_vmcs_name(32, "GUEST_PERF_GLOBAL_CTRL");
    setup_vmcs_name(57, "GUEST_PDPTE0");
    setup_vmcs_name(80, "GUEST_PDPTE1");
    setup_vmcs_name(105, "GUEST_PDPTE2");
    setup_vmcs_name(150, "GUEST_PDPTE3");
    setup_vmcs_name(67, "GUEST_DR7");
    setup_vmcs_name(30, "GUEST_PENDING_DBE");
    setup_vmcs_name(49, "GUEST_SYSENTER_CS");
    setup_vmcs_name(26, "GUEST_SYSENTER_ESP");
    setup_vmcs_name(22, "GUEST_SYSENTER_EIP");
    setup_vmcs_name(52, "GUEST_SMBASE");
    setup_vmcs_name(27, "GUEST_INTERRUPTIBILITY");
    setup_vmcs_name(23, "GUEST_ACTIVITY_STATE");
    return 0;
}
