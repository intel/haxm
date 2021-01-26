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

#ifndef HAX_CORE_VMX_H_
#define HAX_CORE_VMX_H_

#include "../../include/hax_types.h"

#define VMCS_NONE 0xFFFFFFFFFFFFFFFF

// Size of VMCS structure
#define IA32_VMX_VMCS_SIZE 4096

// Intel SDM Vol. 3D: Table C-1. Basic Exit Reasons
enum {
    VMX_EXIT_INT_EXCEPTION_NMI       =  0, // An SW interrupt, exception or NMI has occurred
    VMX_EXIT_EXT_INTERRUPT           =  1, // An external interrupt has occurred
    VMX_EXIT_TRIPLE_FAULT            =  2, // Triple fault occurred
    VMX_EXIT_INIT_EVENT              =  3, // INIT signal arrived
    VMX_EXIT_SIPI_EVENT              =  4, // SIPI signal arrived
    VMX_EXIT_SMI_IO_EVENT            =  5,
    VMX_EXIT_SMI_OTHER_EVENT         =  6,
    VMX_EXIT_PENDING_INTERRUPT       =  7,
    VMX_EXIT_PENDING_NMI             =  8,
    VMX_EXIT_TASK_SWITCH             =  9, // Guest attempted a task switch
    VMX_EXIT_CPUID                   = 10, // Guest executed CPUID instruction
    VMX_EXIT_GETSEC                  = 11, // Guest executed GETSEC instruction
    VMX_EXIT_HLT                     = 12, // Guest executed HLT instruction
    VMX_EXIT_INVD                    = 13, // Guest executed INVD instruction
    VMX_EXIT_INVLPG                  = 14, // Guest executed INVLPG instruction
    VMX_EXIT_RDPMC                   = 15, // Guest executed RDPMC instruction
    VMX_EXIT_RDTSC                   = 16, // Guest executed RDTSC instruction
    VMX_EXIT_RSM                     = 17, // Guest executed RSM instruction in SMM
    VMX_EXIT_VMCALL                  = 18, // Guest executed VMCALL instruction
    VMX_EXIT_VMCLEAR                 = 19, // Guest executed VMCLEAR instruction
    VMX_EXIT_VMLAUNCH                = 20, // Guest executed VMLAUNCH instruction
    VMX_EXIT_VMPTRLD                 = 21, // Guest executed VMPTRLD instruction
    VMX_EXIT_VMPTRST                 = 22, // Guest executed VMPTRST instruction
    VMX_EXIT_VMREAD                  = 23, // Guest executed VMREAD instruction
    VMX_EXIT_VMRESUME                = 24, // Guest executed VMRESUME instruction
    VMX_EXIT_VMWRITE                 = 25, // Guest executed VMWRITE instruction
    VMX_EXIT_VMXOFF                  = 26, // Guest executed VMXON instruction
    VMX_EXIT_VMXON                   = 27, // Guest executed VMXOFF instruction
    VMX_EXIT_CR_ACCESS               = 28, // Guest accessed a control register
    VMX_EXIT_DR_ACCESS               = 29, // Guest attempted access to debug register
    VMX_EXIT_IO                      = 30, // Guest attempted I/O
    VMX_EXIT_MSR_READ                = 31, // Guest attempted to read an MSR
    VMX_EXIT_MSR_WRITE               = 32, // Guest attempted to write an MSR
    VMX_EXIT_FAILED_VMENTER_GS       = 33, // VMENTER failed due to guest state
    VMX_EXIT_FAILED_VMENTER_MSR      = 34, // VMENTER failed due to MSR loading
    VMX_EXIT_MWAIT                   = 36,
    VMX_EXIT_MTF_EXIT                = 37,
    VMX_EXIT_MONITOR                 = 39,
    VMX_EXIT_PAUSE                   = 40,
    VMX_EXIT_MACHINE_CHECK           = 41,
    VMX_EXIT_TPR_BELOW_THRESHOLD     = 43,
    VMX_EXIT_APIC_ACCESS             = 44,
    VMX_EXIT_GDT_IDT_ACCESS          = 46,
    VMX_EXIT_LDT_TR_ACCESS           = 47,
    VMX_EXIT_EPT_VIOLATION           = 48,
    VMX_EXIT_EPT_MISCONFIG           = 49,
    VMX_EXIT_INVEPT                  = 50,
    VMX_EXIT_RDTSCP                  = 51,
    VMX_EXIT_VMX_TIMER_EXIT          = 52,
    VMX_EXIT_INVVPID                 = 53,
    VMX_EXIT_WBINVD                  = 54,
    VMX_EXIT_XSETBV                  = 55, // Guest executed XSETBV instruction
    VMX_EXIT_APIC_WRITE              = 56,
    VMX_EXIT_RDRAND                  = 57,
    VMX_EXIT_INVPCID                 = 58,
    VMX_EXIT_VMFUNC                  = 59,
    VMX_EXIT_ENCLS                   = 60,
    VMX_EXIT_RDSEED                  = 61,
    VMX_EXIT_XSAVES                  = 63,
    VMX_EXIT_XRSTORS                 = 64
};

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

// PIN-BASED CONTROLS
#define EXT_INTERRUPT_EXITING                  0x00000001
#define NMI_EXITING                            0x00000008
#define VIRTUAL_NMIS                           0x00000020
#define VMX_TIMER_EXITING                      0x00000040
#define PIN_CONTROLS_DEFINED                   0x00000069

// Primary CPU Exit CONTROLS
#define INTERRUPT_WINDOW_EXITING               0x00000004
#define USE_TSC_OFFSETTING                     0x00000008
#define HLT_EXITING                            0x00000080
#define INVLPG_EXITING                         0x00000200
#define MWAIT_EXITING                          0x00000400
#define RDPMC_EXITING                          0x00000800
#define RDTSC_EXITING                          0x00001000
#define CR3_LOAD_EXITING                       0x00008000
#define CR3_STORE_EXITING                      0x00010000
#define CR8_LOAD_EXITING                       0x00080000
#define CR8_STORE_EXITING                      0x00100000
#define USE_TPR_SHADOW                         0x00200000
#define NMI_WINDOW_EXITING                     0x00400000
#define DR_EXITING                             0x00800000
#define IO_EXITING                             0x01000000
#define IO_BITMAP_ACTIVE                       0x02000000
#define MONITOR_TRAP_FLAG                      0x08000000
#define MSR_BITMAP_ACTIVE                      0x10000000
#define MONITOR_EXITING                        0x20000000
#define PAUSE_EXITING                          0x40000000
#define SECONDARY_CONTROLS                     0x80000000
#define PRIMARY_CONTROLS_DEFINED               0xfbf99e8c

// Secondary CPU Exit CONTROLS
#define VIRTUALIZE_APIC_ACCESSES               0x00000001
#define ENABLE_EPT                             0x00000002
#define DESCTAB_EXITING                        0x00000004
#define ENABLE_RDTSCP                          0x00000008
#define VIRTUALIZE_X2APIC                      0x00000010
#define ENABLE_VPID                            0x00000020
#define WBINVD_EXITING                         0x00000040
#define UNRESTRICTED_GUEST                     0x00000080
#define PAUSE_LOOP_EXITING                     0x00000400
#define SECONDARY_CONTROLS_DEFINED             0x000004ff

// Exit Controls
#define EXIT_CONTROL_SAVE_DEBUG_CONTROLS       0x00000004
#define EXIT_CONTROL_HOST_ADDR_SPACE_SIZE      0x00000200
#define EXIT_CONTROL_LOAD_PERF_GLOBAL_CTRL     0x00001000
#define EXIT_CONTROL_ACKNOWLEDGE_INTERRUPT     0x00008000
#define EXIT_CONTROL_SAVE_PAT                  0x00040000
#define EXIT_CONTROL_LOAD_PAT                  0x00080000
#define EXIT_CONTROL_SAVE_EFER                 0x00100000
#define EXIT_CONTROL_LOAD_EFER                 0x00200000
#define EXIT_CONTROL_SAVE_VMX_TIMER            0x00400000
#define EXIT_CONTROLS_DEFINED                  0x007c9204

// Entry Controls
#define ENTRY_CONTROL_LOAD_DEBUG_CONTROLS      0x00000004
#define ENTRY_CONTROL_LONG_MODE_GUEST          0x00000200
#define ENTRY_CONTROL_ENTRY_TO_SMM             0x00000400
#define ENTRY_CONTROL_TEAR_DOWN_SMM_MONITOR    0x00000800
#define ENTRY_CONTROL_LOAD_PERF_GLOBAL_CTRL    0x00002000
#define ENTRY_CONTROL_LOAD_PAT                 0x00004000
#define ENTRY_CONTROL_LOAD_EFER                0x00008000
#define ENTRY_CONTROLS_DEFINED                 0x0000ee04

// Intel SDM Vol. 3C: 30.2 Conventions
typedef enum vmx_result_t {
    /* VMsucceed
     * Operation succeeded (OSZPAC flags are 0) */
    VMX_SUCCEED = 0,

    /* VMfailInvalid
     * Operation failed and VCMS pointer is invalid (CF=1) */
    VMX_FAIL_INVALID = 1,

    /* VMfailValid(ErrorNumber)
     * Operation failed and VCMS pointer is valid (ZF=1) */
    VMX_FAIL_VALID = 2,
} vmx_result_t;

// Intel SDM Vol. 3C: 30.4 VM Instruction Error Numbers
typedef enum vmx_error_t {
    VMX_ERROR_VMCALL_ROOT                =  1, // VMCALL executed in VMX root operation
    VMX_ERROR_VMCLEAR_PADDR_INVALID      =  2, // VMCLEAR with invalid physical address
    VMX_ERROR_VMCLEAR_VMXON_PTR          =  3, // VMCLEAR with VMXON pointer
    VMX_ERROR_VMLAUNCH_VMCS_UNCLEAR      =  4, // VMLAUNCH with non-clear VMCS
    VMX_ERROR_VMRESUME_VMCS_UNLAUNCHED   =  5, // VMRESUME with non-launched VMCS
    VMX_ERROR_VMRESUME_AFTER_VMXOFF      =  6, // VMRESUME after VMXOFF
    VMX_ERROR_ENTRY_CTRL_FIELDS_INVALID  =  7, // VM entry with invalid control field(s)
    VMX_ERROR_ENTRY_HOST_FIELDS_INVALID  =  8, // VM entry with invalid host-state field(s)
    VMX_ERROR_VMPTRLD_PADDR_INVALID      =  9, // VMPTRLD with invalid physical address
    VMX_ERROR_VMPTRLD_VMXON_PTR          = 10, // VMPTRLD with VMXON pointer
    VMX_ERROR_VMPTRLD_VMCSREV_INVALID    = 11, // VMPTRLD with incorrect VMCS revision identifier
    VMX_ERROR_VMREAD_VMWRITE_INVALID     = 12, // VMREAD/VMWRITE from/to unsupported VMCS component
    VMX_ERROR_VMWRITE_READONLY           = 13, // VMWRITE to read-only VMCS component
    VMX_ERROR_VMXON_ROOT                 = 15, // VMXON executed in VMX root operation
    VMX_ERROR_ENTRY_VMCS_INVALID         = 16, // VM entry with invalid executive-VMCS pointer
    VMX_ERROR_ENTRY_VMCS_UNLAUNCHED      = 17, // VM entry with non-launched executive VMCS
    VMX_ERROR_ENTRY_VMCS_NOT_VMXON       = 18, // VM entry with executive-VMCS pointer not VMXON pointer
    VMX_ERROR_VMCALL_VMCS_UNCLEAR        = 19, // VMCALL with non-clear VMCS
    VMX_ERROR_VMCALL_EXIT_INVALID        = 20, // VMCALL with invalid VM-exit control fields
    VMX_ERROR_VMCALL_MSEG_INVALID        = 22, // VMCALL with incorrect MSEG revision identifier
    VMX_ERROR_VMXOFF_SMM_DUALMONITOR     = 23, // VMXOFF under dual-monitor treatment of SMIs and SMM
    VMX_ERROR_VMCALL_SMM_INVALID         = 24, // VMCALL with invalid SMM-monitor features
    VMX_ERROR_ENTRY_EXECCTRL_INVALID     = 25, // VM entry with invalid VM-execution control fields in executive VMCS
    VMX_ERROR_ENTRY_MOV_SS               = 26, // VM entry with events blocked by MOV SS
    VMX_ERROR_INVEPT_INVALID             = 28, // Invalid operand to INVEPT/INVVPID
} vmx_error_t;

// Exit qualification 64-bit OK
union exit_qualification_t {
    uint64_t raw;
    uint64_t address;
    struct {
        uint32_t size        : 3;
        uint32_t direction   : 1;
        uint32_t string      : 1;
        uint32_t rep         : 1;
        uint32_t encoding    : 1;
        uint32_t rsv1        : 9;
        uint32_t port        : 16;
    } io;
    struct {
        uint32_t creg        : 4;
        uint32_t type        : 2;
        uint32_t rsv2        : 2;
        uint32_t gpr         : 4;
        uint32_t rsv3        : 4;
        uint32_t lmsw_source : 16;
    } cr;
    struct {
        uint32_t dreg        : 3;
        uint32_t rsv4        : 1;
        uint32_t direction   : 1;
        uint32_t rsv5        : 3;
        uint32_t gpr         : 4;
        uint32_t rsv6        : 20;
    } dr;
    struct {
        uint32_t selector    : 16;
        uint32_t rsv7        : 14;
        uint32_t source      : 2;
    } task_switch;
    struct {
        uint32_t offset      : 12;
        uint32_t access      : 3;
    } vapic;
    struct {
        uint8_t vector;
    } vapic_eoi;
    struct {
        uint32_t r           : 1;
        uint32_t w           : 1;
        uint32_t x           : 1;
        uint32_t _r          : 1;
        uint32_t _w          : 1;
        uint32_t _x          : 1;
        uint32_t res1        : 1;
        uint32_t gla1        : 1;
        uint32_t gla2        : 1;
        /*
         * According to latest IA SDM (September 2016), Table 27-7, these 3 bits
         * (11:9) are no longer reserved. They are meaningful if advanced
         * VM-exit information for EPT violations is supported by the processor,
         * which is the case with Kaby Lake.
         */
        uint32_t res2        : 3;  /* bits 11:9 */
        uint32_t nmi_block   : 1;
        uint32_t res3        : 19;
        uint32_t res4        : 32;
    } ept;
};

typedef union exit_qualification_t exit_qualification_t;

// Exit reason
union exit_reason_t {
    uint32_t raw;
    struct {
        uint32_t basic_reason : 16;
        uint32_t rsv          : 12;
        uint32_t pending_mtf  : 1;
        uint32_t vmexit_root  : 1;
        uint32_t vmexit_fail  : 1;
        uint32_t vmenter_fail : 1;
    };
};

typedef union exit_reason_t exit_reason_t;

// Instruction Information Layout (see spec: 8.6)
union instruction_info_t {
    uint32_t raw;
    struct {
        uint32_t scaling      : 2;
        uint32_t              : 1;
        uint32_t register1    : 4;
        uint32_t addrsize     : 3;
        uint32_t memreg       : 1;
        uint32_t              : 4;
        uint32_t segment      : 3;
        uint32_t indexreg     : 4;
        uint32_t indexinvalid : 1;
        uint32_t basereg      : 4;
        uint32_t baseinvalid  : 1;
        uint32_t register2    : 4;
    };
};

typedef union instruction_info_t instruction_info_t;

// 64-bit OK
union interruption_info_t {
    uint32_t raw;
    struct {
        uint32_t vector             : 8;
        uint32_t type               : 3;
        uint32_t deliver_error_code : 1;
        uint32_t nmi_unmasking      : 1;
        uint32_t reserved           : 18;
        uint32_t valid              : 1;
    };
};

enum {
    INTERRUPT   = 0,
    NMI         = 2,
    EXCEPTION   = 3,
    SWINT       = 4,
    PRIV_TRAP   = 5,
    UNPRIV_TRAP = 6,
    OTHER       = 7
};

typedef union interruption_info_t interruption_info_t;

void get_interruption_info_t(interruption_info_t *info, uint8_t v, uint8_t t);

enum {
    GAS_ACTIVE      = 0,
    GAS_HLT         = 1,
    GAS_SHUTDOWN    = 2,
    GAS_WAITFORSIPI = 3,
    GAS_CSTATE      = 4
};

// Intel SDM Vol. 3C: Table 24-3. Format of Interruptibility State
#define GUEST_INTRSTAT_STI_BLOCKING            0x00000001
#define GUEST_INTRSTAT_SS_BLOCKING             0x00000002
#define GUEST_INTRSTAT_SMI_BLOCKING            0x00000004
#define GUEST_INTRSTAT_NMI_BLOCKING            0x00000008

#ifdef HAX_COMPILER_MSVC
#pragma pack(push, 1)
#endif

// 64-bit OK
struct PACKED info_t {
    union {                          // 0: Basic Information
        uint64_t         _basic_info;
        struct {
            uint32_t     _vmcs_revision_id;
            struct {
                uint32_t _vmcs_region_length : 16;
                uint32_t _phys_limit_32bit   : 1;
                uint32_t _par_mon_supported  : 1;
                uint32_t _mem_types          : 4;
                uint32_t _reserved1          : 10;
            };
        };
    };

    union {
        uint64_t         pin_ctls;
        struct {
            uint32_t     pin_ctls_0;   // 1: Pin-Based VM-Execution Controls
            uint32_t     pin_ctls_1;
        };
    };

    union {
        uint64_t         pcpu_ctls;
        struct {
            uint32_t     pcpu_ctls_0;  // 2: Processor-Based VM-Execution Controls
            uint32_t     pcpu_ctls_1;
        };
    };

    union {
        uint64_t         exit_ctls;
        struct {
            uint32_t     exit_ctls_0;  // 3: Allowed VM-Exit Controls
            uint32_t     exit_ctls_1;
        };
    };

    union {
        uint64_t         entry_ctls;
        struct {
            uint32_t     entry_ctls_0; // 4: Allowed VM-Entry Controls
            uint32_t     entry_ctls_1;
        };
    };

    union {                          // 5: Miscellaneous Data
        uint64_t         _miscellaneous;
        struct {
            struct {
                uint32_t _tsc_comparator_len : 6;
                uint32_t _reserved2          : 2;
                uint32_t _max_sleep_state    : 8;
                uint32_t _max_cr3_targets    : 9;
                uint32_t _reserved3          : 7;
            };
            uint32_t     _mseg_revision_id;
        };
    };

    uint64_t             _cr0_fixed_0; // 6: VMX-Fixed Bits in CR0
    uint64_t             _cr0_fixed_1; // 7: VMX-Fixed Bits in CR0
    uint64_t             _cr4_fixed_0; // 8: VMX-Fixed Bits in CR4
    uint64_t             _cr4_fixed_1; // 9: VMX-Fixed Bits in CR4

    union {                          // 10: VMCS Enumeration
        uint64_t         _vmcs_enumeration;
        struct {
                uint32_t                     : 1;
                uint32_t _max_vmcs_idx       : 9;
        };
    };

    union {
        uint64_t         scpu_ctls;
        struct {
            uint32_t     scpu_ctls_0;  // 2: Processor-Based VM-Execution Controls
            uint32_t     scpu_ctls_1;
        };
    };

    uint64_t             _ept_cap;
};

#ifdef HAX_COMPILER_MSVC
#pragma pack(pop)
#endif

typedef struct PACKED info_t info_t;
// 64-bit OK
struct mseg_header_t {
    uint32_t mseg_revision_id;
    uint32_t smm_monitor_features;
    uint32_t gdtr_limit;
    uint32_t gdtr_base;
    uint32_t cs;
    uint32_t eip;
    uint32_t esp;
    uint32_t cr3;
};

union vmcs_t {
    uint32_t _revision_id;
    uint8_t _raw8[IA32_VMX_VMCS_SIZE];
};

typedef union vmcs_t vmcs_t;

enum encode_t {
    ENCODE_16 = 0x0,
    ENCODE_64 = 0x1,
    ENCODE_32 = 0x2,
    ENCODE_NATURAL = 0x3
};

typedef enum encode_t encode_t;

#define ENCODE_MASK    0x3
#define ENCODE_SHIFT    13

struct invept_desc {
    uint64_t eptp;
    uint64_t rsvd;
};

// Intel SDM Vol. 3C: Table 24-12. Format of an MSR Entry
typedef struct ALIGNED(16) vmx_msr_entry {
    uint64_t index;
    uint64_t data;
} vmx_msr_entry;

struct vcpu_state_t;
struct vcpu_t;

vmx_result_t ASMCALL asm_invept(uint type, struct invept_desc *desc);
vmx_result_t ASMCALL asm_vmclear(const hax_paddr_t *addr_in);
vmx_result_t ASMCALL asm_vmptrld(const hax_paddr_t *addr_in);
vmx_result_t ASMCALL asm_vmxon(const hax_paddr_t *addr_in);
vmx_result_t ASMCALL asm_vmxoff(void);
vmx_result_t ASMCALL asm_vmptrst(hax_paddr_t *addr_out);
vmx_result_t ASMCALL asm_vmxrun(struct vcpu_state_t *state, uint16_t launch);

mword ASMCALL vmx_get_rip(void);

mword ASMCALL asm_vmread(uint32_t component);
void ASMCALL asm_vmwrite(uint32_t component, mword val);

uint64_t vmread(struct vcpu_t *vcpu, component_index_t component);
uint64_t vmread_dump(struct vcpu_t *vcpu, unsigned enc, const char *name);
void vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                 component_index_t component, uint64_t source_val);

#define vmwrite(vcpu, x, y) vmx_vmwrite(vcpu, #x, x, y)

#define VMREAD_SEG(vcpu, seg, val)                                 \
    do {                                                           \
        ((val).selector = vmread(vcpu, GUEST_##seg##_SELECTOR),    \
         (val).base     = vmread(vcpu, GUEST_##seg##_BASE),        \
         (val).limit    = vmread(vcpu, GUEST_##seg##_LIMIT),       \
         (val).ar       = vmread(vcpu, GUEST_##seg##_AR));         \
        {                                                          \
            if ((val).null == 1)                                   \
                (val).ar = 0;                                      \
        }                                                          \
    } while (false)

#define VMREAD_DESC(vcpu, desc, val)                               \
        ((val).base  = vmread(vcpu, GUEST_##desc##_BASE),          \
         (val).limit = vmread(vcpu, GUEST_##desc##_LIMIT))

#if defined(HAX_PLATFORM_WINDOWS)
#define VMWRITE_SEG(vcpu, seg, val) {                              \
            uint32_t tmp_ar = val.ar;                              \
            if (tmp_ar == 0)                                       \
                tmp_ar = 0x10000;                                  \
            vmwrite(vcpu, GUEST_##seg##_SELECTOR, (val).selector); \
            vmwrite(vcpu, GUEST_##seg##_BASE, (val).base);         \
            vmwrite(vcpu, GUEST_##seg##_LIMIT, (val).limit);       \
            vmwrite(vcpu, GUEST_##seg##_AR, tmp_ar);               \
        }

#else
#define VMWRITE_SEG(vcpu, seg, val) ({                             \
            uint32_t tmp_ar = val.ar;                              \
            if (tmp_ar == 0)                                       \
                tmp_ar = 0x10000;                                  \
            vmwrite(vcpu, GUEST_##seg##_SELECTOR, (val).selector); \
            vmwrite(vcpu, GUEST_##seg##_BASE, (val).base);         \
            vmwrite(vcpu, GUEST_##seg##_LIMIT, (val).limit);       \
            vmwrite(vcpu, GUEST_##seg##_AR, tmp_ar);               \
        })
#endif

#define VMWRITE_DESC(vcpu, desc, val)                              \
        (vmwrite(vcpu, GUEST_##desc##_BASE, (val).base),           \
         vmwrite(vcpu, GUEST_##desc##_LIMIT, (val).limit))

void vmx_read_info(info_t *vmxinfo);

#endif  // HAX_CORE_VMX_H_
