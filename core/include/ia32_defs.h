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

#ifndef HAX_CORE_IA32_DEFS_H_
#define HAX_CORE_IA32_DEFS_H_

#include "../../include/hax_types.h"

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
    DR7_L0          = (1 << 0),
    DR7_G0          = (1 << 1),
    DR7_L1          = (1 << 2),
    DR7_G1          = (1 << 3),
    DR7_L2          = (1 << 4),
    DR7_G2          = (1 << 5),
    DR7_L3          = (1 << 6),
    DR7_G3          = (1 << 7),
    DR7_GD          = (1 << 13),
};
#define    DR6_SETBITS          0xFFFF0FF0
#define    DR7_SETBITS          (1 << 10)
#define    HBREAK_ENABLED_MASK  (DR7_L0 | DR7_G0 | DR7_L1 | DR7_G1 | \
                                 DR7_L2 | DR7_G2 | DR7_L3 | DR7_G3)

/*
 * According to SDM Vol 3B 17.2.6, DR6/7 high 32 bits should only be set to
 * 0 in 64 bits mode. Reserved bits should be 1.
 */
static inline uint64_t fix_dr6(uint64_t val)
{
    return (val & 0xffffffff) | DR6_SETBITS;
}

static inline uint64_t fix_dr7(uint64_t val)
{
    return (val & 0xffffffff) | DR7_SETBITS;
}

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
    IA32_KERNEL_GS_BASE          = 0xc0000102,
    IA32_TSC_AUX                 = 0xc0000103
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

// Intel SDM Vol. 2A: Table 3-4. Intel 64 and IA-32 General Exceptions
enum {
    VECTOR_DE   =  0,  // Divide Error
    VECTOR_DB   =  1,  // Debug
    VECTOR_NMI  =  2,  // NMI Interrupt
    VECTOR_BP   =  3,  // Breakpoint
    VECTOR_OF   =  4,  // Overflow
    VECTOR_BR   =  5,  // BOUND Range Exceeded
    VECTOR_UD   =  6,  // Undefined Opcode
    VECTOR_NM   =  7,  // Device Not Available (No Math Coprocessor)
    VECTOR_DF   =  8,  // Double Fault
    VECTOR_TS   = 10,  // Invalid TSS
    VECTOR_NP   = 11,  // Segment Not Present
    VECTOR_SS   = 12,  // Stack Segment Fault
    VECTOR_GP   = 13,  // General Protection
    VECTOR_PF   = 14,  // Page Fault
    VECTOR_MF   = 16,  // Floating-Point Error (Math Error)
    VECTOR_AC   = 17,  // Alignment Check
    VECTOR_MC   = 18,  // Machine Check
    VECTOR_XM   = 19,  // SIMD Floating-Point Numeric Error
    VECTOR_VE   = 20   // Virtualization Exception
};

// For IA32_APIC_BASE MSR (see IASDM Vol. 3A 10.4.4)
#define APIC_BASE_BSP (1ULL << 8)
#define APIC_BASE_ENABLE  (1ULL << 11)
#define APIC_BASE_DEFAULT_ADDR  0xfee00000
#define APIC_BASE_ADDR_MASK (((1ULL << 24) - 1) << 12)
// Reserve bits 0 through 7, bits 9 and 10, and
// bits MAXPHYADDR1 through 63 , assuming MAXPHYADDR == 36.
// TODO: Use CPUID to obtain the true MAXPHYADDR
#define APIC_BASE_MASK (APIC_BASE_BSP | APIC_BASE_ENABLE | APIC_BASE_ADDR_MASK)

#define IA32_VMX_MISC_UG_AVAILABLE (0x0000000000000020)

#endif  // HAX_CORE_IA32_DEFS_H_
