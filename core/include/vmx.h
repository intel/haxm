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

#include "hax_core_interface.h"
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
    VMX_EXIT_VMCALL                  = 18,
    VMX_EXIT_VMCLEAR                 = 19,
    VMX_EXIT_VMLAUNCH                = 20,
    VMX_EXIT_VMPTRLD                 = 21,
    VMX_EXIT_VMPTRST                 = 22,
    VMX_EXIT_VMREAD                  = 23,
    VMX_EXIT_VMRESUME                = 24,
    VMX_EXIT_VMWRITE                 = 25,
    VMX_EXIT_VMXOFF                  = 26,
    VMX_EXIT_VMXON                   = 27,
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
    VMX_EXIT_XSETBV                  = 55,
    VMX_EXIT_APIC_WRITE              = 56,
    VMX_EXIT_RDRAND                  = 57,
    VMX_EXIT_INVPCID                 = 58,
    VMX_EXIT_VMFUNC                  = 59,
    VMX_EXIT_ENCLS                   = 60,
    VMX_EXIT_RDSEED                  = 61,
    VMX_EXIT_XSAVES                  = 63,
    VMX_EXIT_XRSTORS                 = 64
};

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
    uint64 raw;
    uint64 address;
    struct {
        uint32 size        : 3;
        uint32 direction   : 1;
        uint32 string      : 1;
        uint32 rep         : 1;
        uint32 encoding    : 1;
        uint32 rsv1        : 9;
        uint32 port        : 16;
    } io;
    struct {
        uint32 creg        : 4;
        uint32 type        : 2;
        uint32 rsv2        : 2;
        uint32 gpr         : 4;
        uint32 rsv3        : 4;
        uint32 lmsw_source : 16;
    } cr;
    struct {
        uint32 dreg        : 3;
        uint32 rsv4        : 1;
        uint32 direction   : 1;
        uint32 rsv5        : 3;
        uint32 gpr         : 4;
        uint32 rsv6        : 20;
    } dr;
    struct {
        uint32 selector    : 16;
        uint32 rsv7        : 14;
        uint32 source      : 2;
    } task_switch;
    struct {
        uint32 offset      : 12;
        uint32 access      : 3;
    } vapic;
    struct {
        uint8 vector;
    } vapic_eoi;
    struct {
        uint32 r           : 1;
        uint32 w           : 1;
        uint32 x           : 1;
        uint32 _r          : 1;
        uint32 _w          : 1;
        uint32 _x          : 1;
        uint32 res1        : 1;
        uint32 gla1        : 1;
        uint32 gla2        : 1;
        /*
         * According to latest IA SDM (September 2016), Table 27-7, these 3 bits
         * (11:9) are no longer reserved. They are meaningful if advanced
         * VM-exit information for EPT violations is supported by the processor,
         * which is the case with Kaby Lake.
         */
        uint32 res2        : 3;  /* bits 11:9 */
        uint32 nmi_block   : 1;
        uint32 res3        : 19;
        uint32 res4        : 32;
    } ept;
};

typedef union exit_qualification_t exit_qualification_t;

// Exit reason
union exit_reason_t {
    uint32 raw;
    struct {
        uint32 basic_reason : 16;
        uint32 rsv          : 12;
        uint32 pending_mtf  : 1;
        uint32 vmexit_root  : 1;
        uint32 vmexit_fail  : 1;
        uint32 vmenter_fail : 1;
    };
};

typedef union exit_reason_t exit_reason_t;

// Instruction Information Layout (see spec: 8.6)
union instruction_info_t {
    uint32 raw;
    struct {
        uint32 scaling      : 2;
        uint32              : 1;
        uint32 register1    : 4;
        uint32 addrsize     : 3;
        uint32 memreg       : 1;
        uint32              : 4;
        uint32 segment      : 3;
        uint32 indexreg     : 4;
        uint32 indexinvalid : 1;
        uint32 basereg      : 4;
        uint32 baseinvalid  : 1;
        uint32 register2    : 4;
    };
};

typedef union instruction_info_t instruction_info_t;

// 64-bit OK
union interruption_info_t {
    uint32 raw;
    struct {
        uint32 vector             : 8;
        uint32 type               : 3;
        uint32 deliver_error_code : 1;
        uint32 nmi_unmasking      : 1;
        uint32 reserved           : 18;
        uint32 valid              : 1;
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

void get_interruption_info_t(interruption_info_t *info, uint8 v, uint8 t);

enum {
    GAS_ACTIVE      = 0,
    GAS_HLT         = 1,
    GAS_SHUTDOWN    = 2,
    GAS_WAITFORSIPI = 3,
    GAS_CSTATE      = 4
};

#ifdef __WINNT__
#pragma pack(push, 1)
#endif

// 64-bit OK
struct PACKED info_t {
    union {                          // 0: Basic Information
        uint64         _basic_info;
        struct {
            uint32     _vmcs_revision_id;
            struct {
                uint32 _vmcs_region_length : 16;
                uint32 _phys_limit_32bit   : 1;
                uint32 _par_mon_supported  : 1;
                uint32 _mem_types          : 4;
                uint32 _reserved1          : 10;
            };
        };
    };

    union {
        uint64         pin_ctls;
        struct {
            uint32     pin_ctls_0;   // 1: Pin-Based VM-Execution Controls
            uint32     pin_ctls_1;
        };
    };

    union {
        uint64         pcpu_ctls;
        struct {
            uint32     pcpu_ctls_0;  // 2: Processor-Based VM-Execution Controls
            uint32     pcpu_ctls_1;
        };
    };

    union {
        uint64         exit_ctls;
        struct {
            uint32     exit_ctls_0;  // 3: Allowed VM-Exit Controls
            uint32     exit_ctls_1;
        };
    };

    union {
        uint64         entry_ctls;
        struct {
            uint32     entry_ctls_0; // 4: Allowed VM-Entry Controls
            uint32     entry_ctls_1;
        };
    };

    union {                          // 5: Miscellaneous Data
        uint64         _miscellaneous;
        struct {
            struct {
                uint32 _tsc_comparator_len : 6;
                uint32 _reserved2          : 2;
                uint32 _max_sleep_state    : 8;
                uint32 _max_cr3_targets    : 9;
                uint32 _reserved3          : 7;
            };
            uint32     _mseg_revision_id;
        };
    };

    uint64             _cr0_fixed_0; // 6: VMX-Fixed Bits in CR0
    uint64             _cr0_fixed_1; // 7: VMX-Fixed Bits in CR0
    uint64             _cr4_fixed_0; // 8: VMX-Fixed Bits in CR4
    uint64             _cr4_fixed_1; // 9: VMX-Fixed Bits in CR4

    union {                          // 10: VMCS Enumeration
        uint64         _vmcs_enumeration;
        struct {
                uint32                     : 1;
                uint32 _max_vmcs_idx       : 9;
        };
    };

    union {
        uint64         scpu_ctls;
        struct {
            uint32     scpu_ctls_0;  // 2: Processor-Based VM-Execution Controls
            uint32     scpu_ctls_1;
        };
    };

    uint64             _ept_cap;
};

#ifdef __WINNT__
#pragma pack(pop)
#endif

typedef struct PACKED info_t info_t;
// 64-bit OK
struct mseg_header_t {
    uint32 mseg_revision_id;
    uint32 smm_monitor_features;
    uint32 gdtr_limit;
    uint32 gdtr_base;
    uint32 cs;
    uint32 eip;
    uint32 esp;
    uint32 cr3;
};

union vmcs_t {
    uint32 _revision_id;
    uint8 _raw8[IA32_VMX_VMCS_SIZE];
};

typedef union vmcs_t vmcs_t;

struct vcpu_t;
extern void load_vmcs_common(struct vcpu_t *vcpu);
extern uint32 load_vmcs(struct vcpu_t *vcpu, preempt_flag *flags);
extern uint32 put_vmcs(struct vcpu_t *vcpu, preempt_flag *flags);
extern uint8 is_vmcs_loaded(struct vcpu_t *vcpu);
extern void hax_panic_log(struct vcpu_t *vcpu);

void hax_enable_irq(void);
void hax_disable_irq(void);

extern void hax_disable_preemption(preempt_flag *eflags);
extern void hax_enable_preemption(preempt_flag *eflags);

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
    uint64 eptp;
    uint64 rsvd;
};

vmx_result_t ASMCALL asm_invept(uint type, struct invept_desc *desc);
vmx_result_t ASMCALL asm_vmclear(const paddr_t *addr_in);
vmx_result_t ASMCALL asm_vmptrld(const paddr_t *addr_in);
vmx_result_t ASMCALL asm_vmxon(const paddr_t *addr_in);
vmx_result_t ASMCALL asm_vmxoff(void);
vmx_result_t ASMCALL asm_vmptrst(paddr_t *addr_out);
uint64 ASMCALL asm_vmxrun(struct vcpu_state_t *state, uint16 launch);

mword ASMCALL vmx_get_rip(void);

uint64 vmx_vmread(struct vcpu_t *vcpu, component_index_t component);
uint64 vmx_vmread_natural(struct vcpu_t *vcpu, component_index_t component);
uint64 vmx_vmread_64(struct vcpu_t *vcpu, component_index_t component);

void _vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                  component_index_t component, mword source_val);
void _vmx_vmwrite_natural(struct vcpu_t *vcpu, const char *name,
                          component_index_t component, uint64 source_val);
void _vmx_vmwrite_64(struct vcpu_t *vcpu, const char *name,
                     component_index_t component, uint64 source_val);

static inline uint64 __vmread_common(struct vcpu_t *vcpu,
                                     component_index_t component)
{
    uint64 value = 0;
    uint8 val = (component >> ENCODE_SHIFT) & ENCODE_MASK;

    switch(val) {
        case ENCODE_16:
        case ENCODE_32: {
            value = vmx_vmread(vcpu, component);
            break;
        }
        case ENCODE_64: {
            value = vmx_vmread_64(vcpu, component);
            break;
        }
        case ENCODE_NATURAL: {
            value = vmx_vmread_natural(vcpu, component);
            break;
        }
        default: {
            hax_error("Unsupported component %x val %x\n", component, val);
            break;
        }
    }
    return value;
}

static inline uint64 vmread(struct vcpu_t *vcpu, component_index_t component)
{
    preempt_flag flags;
    uint64 val;
    uint8 loaded = 0;

    if (!vcpu || is_vmcs_loaded(vcpu))
        loaded = 1;

    if (!loaded) {
        if (load_vmcs(vcpu, &flags)) {
            vcpu_set_panic(vcpu);
            hax_panic_log(vcpu);
            return 0;
        }
    }

    val = __vmread_common(vcpu, component);

    if (!loaded) {
        if (put_vmcs(vcpu, &flags)) {
            vcpu_set_panic(vcpu);
            hax_panic_log(vcpu);
            return 0;
        }
    }

    return val;
}

static inline uint64 vmread_dump(struct vcpu_t *vcpu, unsigned enc, char *name)
{
    uint64 val;

    switch ((enc >> 13) & 0x3) {
        case 0:
        case 2: {
            val = vmread(vcpu, enc);
            hax_warning("%04x %s: %llx\n", enc, name, val);
            break;
        }
        case 1: {
            val = vmread(vcpu, enc);
            hax_warning("%04x %s: %llx\n", enc, name, val);
            break;
        }
        case 3: {
            val = vmread(vcpu, enc);
            hax_warning("%04x %s: %llx\n", enc, name, val);
            break;
        }
        default: {
            hax_error("unsupported enc %x\n", enc);
            break;
        }
    }
    return val;
}

extern void _vmx_vmwrite_natural(struct vcpu_t *vcpu, const char *name,
                                 component_index_t component,
                                 uint64 source_val);

extern void _vmx_vmwrite_64(struct vcpu_t *vcpu, const char *name,
                            component_index_t component, uint64 source_val);

extern void _vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                         component_index_t component, mword source_val);

static inline void __vmx_vmwrite_common(struct vcpu_t *vcpu, const char *name,
                                        component_index_t component,
                                        uint64 source_val)
{
    uint8 val = (component & 0x6000) >> 13;
    switch (val) {
        case ENCODE_16:
        case ENCODE_32: {
            source_val &= 0x00000000FFFFFFFF;
            _vmx_vmwrite(vcpu, name, component, source_val);
            break;
        }
        case ENCODE_64: {
            _vmx_vmwrite_64(vcpu, name, component, source_val);
            break;
        }
        case ENCODE_NATURAL: {
            _vmx_vmwrite_natural(vcpu, name, component, source_val);
            break;
        }
        default: {
            hax_error("Unsupported component %x, val %x\n", component, val);
            break;
        }
    }
}

static inline void vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                               component_index_t component, uint64 source_val)
{
    preempt_flag flags;
    uint8 loaded = 0;

    if (!vcpu || is_vmcs_loaded(vcpu))
        loaded = 1;

    if (!loaded) {
        if (load_vmcs(vcpu, &flags)) {
            vcpu_set_panic(vcpu);
            hax_panic_log(vcpu);
            return;
        }
    }

    __vmx_vmwrite_common(vcpu, name, component, source_val);

    if (!loaded) {
        if (put_vmcs(vcpu, &flags)) {
            vcpu_set_panic(vcpu);
            hax_panic_log(vcpu);
            return;
        }
    }
}

#define vmwrite(vcpu, x, y) vmx_vmwrite(vcpu, #x, x, y)

#define VMREAD_SEG(vcpu, seg, val)                                 \
        ((val).selector = vmread(vcpu, GUEST_##seg##_SELECTOR),    \
         (val).base     = vmread(vcpu, GUEST_##seg##_BASE),        \
         (val).limit    = vmread(vcpu, GUEST_##seg##_LIMIT),       \
         (val).ar       = vmread(vcpu, GUEST_##seg##_AR));         \
        {                                                          \
            if ((val).null == 1)                                   \
                (val).ar = 0;                                      \
        }

#define VMREAD_DESC(vcpu, desc, val)                               \
        ((val).base  = vmread(vcpu, GUEST_##desc##_BASE),          \
         (val).limit = vmread(vcpu, GUEST_##desc##_LIMIT))

#if defined(__WINNT__)
#define VMWRITE_SEG(vcpu, seg, val) {                              \
            uint32_t tmp_ar = val.ar;                              \
            if (tmp_ar == 0)                                       \
                tmp_ar = 0x10000;                                  \
            vmwrite(vcpu, GUEST_##seg##_SELECTOR, (val).selector); \
            vmwrite(vcpu, GUEST_##seg##_BASE, (val).base);         \
            vmwrite(vcpu, GUEST_##seg##_LIMIT, (val).limit);       \
            vmwrite(vcpu, GUEST_##seg##_AR, tmp_ar);               \
        }

#elif defined(__MACH__)
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

extern void vmx_read_info(info_t *vmxinfo);
#endif  // HAX_CORE_VMX_H_
