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

#ifndef HAX_CORE_IA32_H_
#define HAX_CORE_IA32_H_

#include "types.h"
#include "segments.h"
#include "../../include/hax.h"

#define IA32_FXSAVE_SIZE 512

enum {
    CR0_PE          = (1  <<  0),
    CR0_MP          = (1  <<  1),
    CR0_EM          = (1  <<  2),
    CR0_TS          = (1  <<  3),
    CR0_ET          = (1  <<  4),
    CR0_NE          = (1  <<  5),
    CR0_WP          = (1  << 16),
    CR0_AM          = (1  << 18),
    CR0_NW          = (1  << 29),
    CR0_CD          = (1  << 30),
    CR0_PG          = (1U << 31)
};

enum {
    CR4_VME         = (1 <<  0),
    CR4_PVI         = (1 <<  1),
    CR4_TSD         = (1 <<  2),
    CR4_DE          = (1 <<  3),
    CR4_PSE         = (1 <<  4),
    CR4_PAE         = (1 <<  5),
    CR4_MCE         = (1 <<  6),
    CR4_PGE         = (1 <<  7),
    CR4_PCE         = (1 <<  8),
    CR4_OSFXSR      = (1 <<  9),
    CR4_OSXMMEXCPT  = (1 << 10),
    CR4_VMXE        = (1 << 13),
    CR4_SMXE        = (1 << 14)
};

enum {
    DR6_BD          = (1 << 13),
    DR7_GD          = (1 << 13),
    DR7_SETBITS     = (1 << 10)
};

enum {
    IA32_P5_MC_ADDR              = 0x0,
    IA32_P5_MC_TYPE              = 0x1,
    IA32_TSC                     = 0x10,
    IA32_PLATFORM_ID             = 0x17,
    IA32_APIC_BASE               = 0x1b,
    IA32_EBC_HARD_POWERON        = 0x2a,
    IA32_EBC_SOFT_POWERON        = 0x2b,
    IA32_EBC_FREQUENCY_ID        = 0x2c,
    IA32_FEATURE_CONTROL         = 0x3a,
    IA32_THERM_DIODE_OFFSET      = 0x3f,
    IA32_BIOS_UPDT_TRIG          = 0x79,
    IA32_BIOS_SIGN_ID            = 0x8b,
    IA32_SMM_MONITOR_CTL         = 0x9b,
    IA32_PMC0                    = 0xc1,
    IA32_PMC1                    = 0xc2,
    IA32_PMC2                    = 0xc3,
    IA32_PMC3                    = 0xc4,
    IA32_FSB_FREQ                = 0xcd,
    IA32_MPERF                   = 0xe7,
    IA32_APERF                   = 0xe8,
    IA32_TEMP_TARGET             = 0xee,
    IA32_MTRRCAP                 = 0xfe,
    IA32_BBL_CR_CTL3             = 0x11e,
    IA32_SYSENTER_CS             = 0x174,
    IA32_SYSENTER_ESP            = 0x175,
    IA32_SYSENTER_EIP            = 0x176,
    IA32_MCG_CAP                 = 0x179,
    IA32_MCG_STATUS              = 0x17a,
    IA32_MCG_CTL                 = 0x17b,
    IA32_PERFEVTSEL0             = 0x186,
    IA32_PERFEVTSEL1             = 0x187,
    IA32_PERFEVTSEL2             = 0x188,
    IA32_PERFEVTSEL3             = 0x189,
    IA32_PERF_CTL                = 0x199,
    IA32_MISC_ENABLE             = 0x1a0,
    IA32_DEBUGCTL                = 0x1d9,
    IA32_MTRR_PHYSBASE0          = 0x200,
    IA32_MTRR_PHYSMASK0          = 0x201,
    IA32_MTRR_PHYSBASE1          = 0x202,
    IA32_MTRR_PHYSMASK1          = 0x203,
    IA32_MTRR_PHYSBASE2          = 0x204,
    IA32_MTRR_PHYSMASK2          = 0x205,
    IA32_MTRR_PHYSBASE3          = 0x206,
    IA32_MTRR_PHYSMASK3          = 0x207,
    IA32_MTRR_PHYSBASE4          = 0x208,
    IA32_MTRR_PHYSMASK4          = 0x209,
    IA32_MTRR_PHYSBASE5          = 0x20a,
    IA32_MTRR_PHYSMASK5          = 0x20b,
    IA32_MTRR_PHYSBASE6          = 0x20c,
    IA32_MTRR_PHYSMASK6          = 0x20d,
    IA32_MTRR_PHYSBASE7          = 0x20e,
    IA32_MTRR_PHYSMASK7          = 0x20f,
    IA32_MTRR_PHYSBASE8          = 0x210,
    IA32_MTRR_PHYSMASK8          = 0x211,
    IA32_MTRR_PHYSBASE9          = 0x212,
    IA32_MTRR_PHYSMASK9          = 0x213,
    MTRRFIX64K_00000             = 0x250,
    MTRRFIX16K_80000             = 0x258,
    MTRRFIX16K_A0000             = 0x259,
    MTRRFIX4K_C0000              = 0x268,
    MTRRFIX4K_F8000              = 0x26f,
    IA32_CR_PAT                  = 0x277,
    IA32_MC0_CTL2                = 0x280,
    IA32_MC1_CTL2                = 0x281,
    IA32_MC2_CTL2                = 0x282,
    IA32_MC3_CTL2                = 0x283,
    IA32_MC4_CTL2                = 0x284,
    IA32_MC5_CTL2                = 0x285,
    IA32_MC6_CTL2                = 0x286,
    IA32_MC7_CTL2                = 0x287,
    IA32_MC8_CTL2                = 0x288,
    IA32_MTRR_DEF_TYPE           = 0x2ff,
    MSR_BPU_COUNTER0             = 0x300,
    IA32_FIXED_CTR0              = 0x309,
    IA32_FIXED_CTR1              = 0x30a,
    IA32_FIXED_CTR2              = 0x30b,
    IA32_PERF_CAPABILITIES       = 0x345,
    MSR_PEBS_MATRIX_VERT         = 0x3f2,
    IA32_FIXED_CTR_CTRL          = 0x38d,
    IA32_PERF_GLOBAL_STATUS      = 0x38e,
    IA32_PERF_GLOBAL_CTRL        = 0x38f,
    IA32_PERF_GLOBAL_OVF_CTRL    = 0x390,
    IA32_MC0_CTL                 = 0x400,
    IA32_MC0_STATUS              = 0x401,
    IA32_MC0_ADDR                = 0x402,
    IA32_MC0_MISC                = 0x403,
    IA32_CPUID_FEATURE_MASK      = 0x478,
    IA32_VMX_BASIC               = 0x480,
    IA32_VMX_PINBASED_CTLS       = 0x481,
    IA32_VMX_PROCBASED_CTLS      = 0x482,
    IA32_VMX_EXIT_CTLS           = 0x483,
    IA32_VMX_ENTRY_CTLS          = 0x484,
    IA32_VMX_MISC                = 0x485,
    IA32_VMX_CR0_FIXED0          = 0x486,
    IA32_VMX_CR0_FIXED1          = 0x487,
    IA32_VMX_CR4_FIXED0          = 0x488,
    IA32_VMX_CR4_FIXED1          = 0x489,
    IA32_VMX_VMCS_ENUM           = 0x48a,
    IA32_VMX_SECONDARY_CTLS      = 0x48b,
    IA32_VMX_EPT_VPID_CAP        = 0x48c,
    IA32_VMX_TRUE_PINBASED_CTLS  = 0x48d,
    IA32_VMX_TRUE_PROCBASED_CTLS = 0x48e,
    IA32_VMX_TRUE_EXIT_CTLS      = 0x48f,
    IA32_VMX_TRUE_ENTRY_CTLS     = 0x490,
    IA32_EFER                    = 0xc0000080,
    IA32_STAR                    = 0xc0000081,
    IA32_LSTAR                   = 0xc0000082,
    IA32_CSTAR                   = 0xc0000083,
    IA32_SF_MASK                 = 0xc0000084,
    IA32_FS_BASE                 = 0xc0000100,
    IA32_GS_BASE                 = 0xc0000101,
    IA32_KERNEL_GS_BASE          = 0xc0000102
};

// EFER bits
enum {
    IA32_EFER_SCE = (1u <<  0),
    IA32_EFER_LME = (1u <<  8),
    IA32_EFER_LMA = (1u << 10),
    IA32_EFER_XD  = (1u << 11)
};

// Feature control MSR bits
enum {
    FC_LOCKED       = 0x00001,
    FC_VMXON_INSMX  = 0x00002,
    FC_VMXON_OUTSMX = 0x00004
};

enum {
    EFLAGS_CF      = (1u <<  0),
    EFLAGS_PF      = (1u <<  2),
    EFLAGS_AF      = (1u <<  4),
    EFLAGS_ZF      = (1u <<  6),
    EFLAGS_TF      = (1u <<  8),
    EFLAGS_IF      = (1u <<  9),
    EFLAGS_DF      = (1u << 10),
    EFLAGS_OF      = (1u << 11),
    EFLAGS_IOPL    = (3u << 12),
    EFLAGS_NT      = (1u << 14),
    EFLAGS_VM      = (1u << 17),

    EFLAGS_SETBITS = (1u <<  1)
};

enum {
    EXC_DIVIDE_ERROR         = 0,
    EXC_DEBUG                = 1,
    EXC_NMI                  = 2,
    EXC_BREAK_POINT          = 3,
    EXC_OVERFLOW             = 4,
    EXC_BOUND_RANGE_EXCEEDED = 5,
    EXC_UNDEFINED_OPCODE     = 6,
    EXC_NOMATH               = 7,
    EXC_DOUBLEFAULT          = 8,
    EXC_COPROC_SEG_OVERRUN   = 9,
    EXC_INVALID_TSS          = 10,
    EXC_SEG_NOT_PRESENT      = 11,
    EXC_STACK_SEG_FAULT      = 12,
    EXC_GENERAL_PROTECTION   = 13,
    EXC_PAGEFAULT            = 14,
    EXC_MATHFAULT            = 16,
    EXC_ALIGNMENT_CHECK      = 17,
    EXC_MACHINE_CHECK        = 18,
    EXC_SIMD                 = 19
};

enum {
    feat_none               = 0U,            // 0
    feat_fpu                = 1U << 0,       // 0x1
    feat_vme                = 1U << 1,       // 0x2
    feat_de                 = 1U << 2,       // 0x4
    feat_pse                = 1U << 3,       // 0x8
    feat_tsc                = 1U << 4,       // 0x10
    feat_msr                = 1U << 5,       // 0x20
    feat_pae                = 1U << 6,       // 0x40
    feat_mce                = 1U << 7,       // 0x80
    feat_cx8                = 1U << 8,       // 0x100
    feat_apic               = 1U << 9,       // 0x200
    feat_sep                = 1U << 11,      // 0x800
    feat_mtrr               = 1U << 12,      // 0x1000
    feat_pge                = 1U << 13,      // 0x2000
    feat_mca                = 1U << 14,      // 0x4000
    feat_cmov               = 1U << 15,      // 0x8000
    feat_pat                = 1U << 16,      // 0x10000
    feat_pse36              = 1U << 17,      // 0x20000
    feat_psn                = 1U << 18,      // 0x40000
    feat_clfsh              = 1U << 19,      // 0x80000
    feat_ds                 = 1U << 21,      // 0x200000
    feat_acpi               = 1U << 22,      // 0x400000
    feat_mmx                = 1U << 23,      // 0x800000
    feat_fxsr               = 1U << 24,      // 0x1000000
    feat_sse                = 1U << 25,      // 0x2000000
    feat_sse2               = 1U << 26,      // 0x4000000
    feat_ss                 = 1U << 27,      // 0x8000000
    feat_htt                = 1U << 28,      // 0x10000000
    feat_tm                 = 1U << 29,      // 0x20000000
    feat_pbe                = 1U << 31,      // 0x80000000

    feat_sse3               = 1U << 0,       // 0x1
    feat_pclmulqdq          = 1U << 1,       // 0x2
    feat_dtes64             = 1U << 2,       // 0x4
    feat_monitor            = 1U << 3,       // 0x8
    feat_dscpl              = 1U << 4,       // 0x10
    feat_vmx                = 1U << 5,       // 0x20
    feat_smx                = 1U << 6,       // 0x40
    feat_est                = 1U << 7,       // 0x80
    feat_tm2                = 1U << 8,       // 0x100
    feat_ssse3              = 1U << 9,       // 0x200
    feat_cnxtid             = 1U << 10,      // 0x400
    feat_cmpxchg16b         = 1U << 13,      // 0x2000
    feat_xtpr_update        = 1U << 14,      // 0x4000
    feat_pdcm               = 1U << 15,      // 0x8000
    feat_dca                = 1U << 18,      // 0x40000
    feat_sse41              = 1U << 19,      // 0x80000
    feat_sse42              = 1U << 20,      // 0x100000
    feat_x2apic             = 1U << 21,      // 0x200000
    feat_movbe              = 1U << 22,      // 0x400000
    feat_popcnt             = 1U << 23,      // 0x800000
    feat_aes                = 1U << 25,      // 0x2000000
    feat_xsave              = 1U << 26,      // 0x4000000
    feat_osxsave            = 1U << 27,      // 0x8000000
    feat_hypervisor         = 1U << 31,      // 0x80000000

    feat_lahf               = 1U << 0,       // 0x1

    feat_syscall            = 1U << 11,      // 0x800
    feat_execute_disable    = 1U << 20,      // 0x10000
    feat_em64t              = 1U << 29       // 0x20000000
};

#define IA32_VMX_MISC_UG_AVAILABLE (0x0000000000000020)

#endif  // HAX_CORE_IA32_H_
