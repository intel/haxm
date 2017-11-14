/*
 * Copyright (c) 2011 Intel Corporation
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

#ifndef HAX_TYPES_H_
#define HAX_TYPES_H_

enum component_index_t {
    VMX_PIN_CONTROLS                            = 0x00004000,
    VMX_PRIMARY_PROCESSOR_CONTROLS              = 0x00004002,
    VMX_SECONDARY_PROCESSOR_CONTROLS            = 0x0000401e,
    VMX_EXCEPTION_BITMAP                        = 0x00004004,
    VMX_PAGE_FAULT_ERROR_CODE_MASK              = 0x00004006,
    VMX_PAGE_FAULT_ERROR_CODE_MATCH             = 0x00004008,
    VMX_EXIT_CONTROLS                           = 0x0000400c,
    VMX_EXIT_MSR_STORE_COUNT                    = 0x0000400e,
    VMX_EXIT_MSR_LOAD_COUNT                     = 0x00004010,
    VMX_ENTRY_CONTROLS                          = 0x00004012,
    VMX_ENTRY_MSR_LOAD_COUNT                    = 0x00004014,
    VMX_ENTRY_INTERRUPT_INFO                    = 0x00004016,
    VMX_ENTRY_EXCEPTION_ERROR_CODE              = 0x00004018,
    VMX_ENTRY_INSTRUCTION_LENGTH                = 0x0000401a,
    VMX_TPR_THRESHOLD                           = 0x0000401c,

    VMX_CR0_MASK                                = 0x00006000,
    VMX_CR4_MASK                                = 0x00006002,
    VMX_CR0_READ_SHADOW                         = 0x00006004,
    VMX_CR4_READ_SHADOW                         = 0x00006006,
    VMX_CR3_TARGET_COUNT                        = 0x0000400a,
    VMX_CR3_TARGET_VAL_BASE                     = 0x00006008, // x6008-x6206

    VMX_VPID                                    = 0x00000000,
    VMX_IO_BITMAP_A                             = 0x00002000,
    VMX_IO_BITMAP_B                             = 0x00002002,
    VMX_MSR_BITMAP                              = 0x00002004,
    VMX_EXIT_MSR_STORE_ADDRESS                  = 0x00002006,
    VMX_EXIT_MSR_LOAD_ADDRESS                   = 0x00002008,
    VMX_ENTRY_MSR_LOAD_ADDRESS                  = 0x0000200a,
    VMX_TSC_OFFSET                              = 0x00002010,
    VMX_VAPIC_PAGE                              = 0x00002012,
    VMX_APIC_ACCESS_PAGE                        = 0x00002014,
    VMX_EPTP                                    = 0x0000201a,
    VMX_PREEMPTION_TIMER                        = 0x0000482e,

    VMX_INSTRUCTION_ERROR_CODE                  = 0x00004400,

    VM_EXIT_INFO_REASON                         = 0x00004402,
    VM_EXIT_INFO_INTERRUPT_INFO                 = 0x00004404,
    VM_EXIT_INFO_EXCEPTION_ERROR_CODE           = 0x00004406,
    VM_EXIT_INFO_IDT_VECTORING                  = 0x00004408,
    VM_EXIT_INFO_IDT_VECTORING_ERROR_CODE       = 0x0000440a,
    VM_EXIT_INFO_INSTRUCTION_LENGTH             = 0x0000440c,
    VM_EXIT_INFO_INSTRUCTION_INFO               = 0x0000440e,
    VM_EXIT_INFO_QUALIFICATION                  = 0x00006400,
    VM_EXIT_INFO_IO_ECX                         = 0x00006402,
    VM_EXIT_INFO_IO_ESI                         = 0x00006404,
    VM_EXIT_INFO_IO_EDI                         = 0x00006406,
    VM_EXIT_INFO_IO_EIP                         = 0x00006408,
    VM_EXIT_INFO_GUEST_LINEAR_ADDRESS           = 0x0000640a,
    VM_EXIT_INFO_GUEST_PHYSICAL_ADDRESS         = 0x00002400,

    HOST_RIP                                    = 0x00006c16,
    HOST_RSP                                    = 0x00006c14,
    HOST_CR0                                    = 0x00006c00,
    HOST_CR3                                    = 0x00006c02,
    HOST_CR4                                    = 0x00006c04,

    HOST_CS_SELECTOR                            = 0x00000c02,
    HOST_DS_SELECTOR                            = 0x00000c06,
    HOST_ES_SELECTOR                            = 0x00000c00,
    HOST_FS_SELECTOR                            = 0x00000c08,
    HOST_GS_SELECTOR                            = 0x00000c0a,
    HOST_SS_SELECTOR                            = 0x00000c04,
    HOST_TR_SELECTOR                            = 0x00000c0c,
    HOST_FS_BASE                                = 0x00006c06,
    HOST_GS_BASE                                = 0x00006c08,
    HOST_TR_BASE                                = 0x00006c0a,
    HOST_GDTR_BASE                              = 0x00006c0c,
    HOST_IDTR_BASE                              = 0x00006c0e,

    HOST_SYSENTER_CS                            = 0x00004c00,
    HOST_SYSENTER_ESP                           = 0x00006c10,
    HOST_SYSENTER_EIP                           = 0x00006c12,

    HOST_PAT                                    = 0x00002c00,
    HOST_EFER                                   = 0x00002c02,
    HOST_PERF_GLOBAL_CTRL                       = 0x00002c04,


    GUEST_RIP                                   = 0x0000681e,
    GUEST_RFLAGS                                = 0x00006820,
    GUEST_RSP                                   = 0x0000681c,
    GUEST_CR0                                   = 0x00006800,
    GUEST_CR3                                   = 0x00006802,
    GUEST_CR4                                   = 0x00006804,

    GUEST_ES_SELECTOR                           = 0x00000800,
    GUEST_CS_SELECTOR                           = 0x00000802,
    GUEST_SS_SELECTOR                           = 0x00000804,
    GUEST_DS_SELECTOR                           = 0x00000806,
    GUEST_FS_SELECTOR                           = 0x00000808,
    GUEST_GS_SELECTOR                           = 0x0000080a,
    GUEST_LDTR_SELECTOR                         = 0x0000080c,
    GUEST_TR_SELECTOR                           = 0x0000080e,

    GUEST_ES_AR                                 = 0x00004814,
    GUEST_CS_AR                                 = 0x00004816,
    GUEST_SS_AR                                 = 0x00004818,
    GUEST_DS_AR                                 = 0x0000481a,
    GUEST_FS_AR                                 = 0x0000481c,
    GUEST_GS_AR                                 = 0x0000481e,
    GUEST_LDTR_AR                               = 0x00004820,
    GUEST_TR_AR                                 = 0x00004822,

    GUEST_ES_BASE                               = 0x00006806,
    GUEST_CS_BASE                               = 0x00006808,
    GUEST_SS_BASE                               = 0x0000680a,
    GUEST_DS_BASE                               = 0x0000680c,
    GUEST_FS_BASE                               = 0x0000680e,
    GUEST_GS_BASE                               = 0x00006810,
    GUEST_LDTR_BASE                             = 0x00006812,
    GUEST_TR_BASE                               = 0x00006814,
    GUEST_GDTR_BASE                             = 0x00006816,
    GUEST_IDTR_BASE                             = 0x00006818,

    GUEST_ES_LIMIT                              = 0x00004800,
    GUEST_CS_LIMIT                              = 0x00004802,
    GUEST_SS_LIMIT                              = 0x00004804,
    GUEST_DS_LIMIT                              = 0x00004806,
    GUEST_FS_LIMIT                              = 0x00004808,
    GUEST_GS_LIMIT                              = 0x0000480a,
    GUEST_LDTR_LIMIT                            = 0x0000480c,
    GUEST_TR_LIMIT                              = 0x0000480e,
    GUEST_GDTR_LIMIT                            = 0x00004810,
    GUEST_IDTR_LIMIT                            = 0x00004812,

    GUEST_VMCS_LINK_PTR                         = 0x00002800,
    GUEST_DEBUGCTL                              = 0x00002802,
    GUEST_PAT                                   = 0x00002804,
    GUEST_EFER                                  = 0x00002806,
    GUEST_PERF_GLOBAL_CTRL                      = 0x00002808,
    GUEST_PDPTE0                                = 0x0000280a,
    GUEST_PDPTE1                                = 0x0000280c,
    GUEST_PDPTE2                                = 0x0000280e,
    GUEST_PDPTE3                                = 0x00002810,

    GUEST_DR7                                   = 0x0000681a,
    GUEST_PENDING_DBE                           = 0x00006822,
    GUEST_SYSENTER_CS                           = 0x0000482a,
    GUEST_SYSENTER_ESP                          = 0x00006824,
    GUEST_SYSENTER_EIP                          = 0x00006826,
    GUEST_SMBASE                                = 0x00004828,
    GUEST_INTERRUPTIBILITY                      = 0x00004824,
    GUEST_ACTIVITY_STATE                        = 0x00004826,

    /* Invalid encoding */
    VMCS_NO_COMPONENT                           = 0x0000ffff
};

typedef enum component_index_t component_index_t;

struct system_desc_t;
#ifdef __MACH__
#include "darwin/hax_types_mac.h"
#endif

#ifdef __WINNT__
#include "windows/hax_types_windows.h"
#endif

/* Common typedef for all platform */
typedef uint64 hax_pa_t;
typedef uint64 hax_pfn_t;
typedef uint64 paddr_t;
typedef uint64 vaddr_t;

extern int32 hax_page_size;

typedef mword vmx_error_t;

#endif  // HAX_TYPES_H_
