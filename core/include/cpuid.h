/*
 * Copyright (c) 2018 Alexandro Sanchez Bach <alexandro@phi.nz>
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

#ifndef HAX_CORE_CPUID_H_
#define HAX_CORE_CPUID_H_

#include "../../include/hax.h"
#include "../../include/hax_types.h"

#define CPUID_REG_EAX 0
#define CPUID_REG_ECX 1
#define CPUID_REG_EDX 2
#define CPUID_REG_EBX 3

typedef union cpuid_args_t {
    struct {
        uint32_t eax;
        uint32_t ecx;
        uint32_t edx;
        uint32_t ebx;
    };
    uint32_t regs[4];
} cpuid_args_t;

typedef struct hax_cpuid_t {
    uint64_t         features_mask;
    hax_cpuid_entry  features[0];
} hax_cpuid_t;

/*
 * X86 Features
 * ============
 * Each feature is 32-bit integer key encoding the following bitfields:
 * - index        : 5;  Array index where the cached feature reside
 * - leaf_lo      : 5;  LSB (5-bits) of the leaf value (EAX)
 * - leaf_hi      : 2;  MSB (2-bits) of the leaf value (EAX)
 * - subleaf_key  : 5;  LSB (5-bits) of the sub-leaf value (ECX)
 * - subleaf_used : 1;  Sub-leaf value is specified
 * - reg          : 2;  Output register index
 * - bit          : 5;  Bit position of the feature
 *
 * This allows representing any feature obtained via:
 * - Leaf register (EAX):
 *     - leaf_hi = 0b00 (0):  00000000h-0000001Fh (Basic leaves)
 *     - leaf_hi = 0b01 (1):  40000000h-4000001Fh (Hypervisor leaves)
 *     - leaf_hi = 0b10 (2):  80000000h-8000001Fh (Extended leaves)
 * - Sub-leaf register (ECX):
 *     - subleaf_used = 1:    00000000h-0000001Fh
 */
#define FEATURE_KEY(index, leaf_lo, leaf_hi, subleaf_key, subleaf_used, reg, bit) ( \
    (((index)        & 0x1F) <<  0) | \
    (((leaf_lo)      & 0x1F) <<  5) | \
    (((leaf_hi)      & 0x03) << 10) | \
    (((subleaf_key)  & 0x1F) << 12) | \
    (((subleaf_used) & 0x01) << 17) | \
    (((reg)          & 0x03) << 18) | \
    (((bit)          & 0x1F) << 20))

#define FEATURE_KEY_LEAF(index, leaf, reg, bit) \
     FEATURE_KEY(index, leaf, (leaf >> 30), 0, 0, reg, bit)
#define FEATURE_KEY_SUBLEAF(index, leaf, subleaf, reg, bit) \
     FEATURE_KEY(index, leaf, (leaf >> 30), subleaf, 1, reg, bit)

#define FEATURE(name) \
    (1 << ((X86_FEATURE_##name >> 20) & 0x1F))

/* Intel SDM Vol. 2A: Table 3-8.
 * Information Returned by CPUID Instruction */
enum {
    /*
     * Intel SDM Vol. 2A: Table 3-10.
     * Feature Information Returned in the ECX Register
     * Features for CPUID with EAX=01h stored in ECX
     */
#define FEAT(bit) \
    FEATURE_KEY_LEAF(0, 0x01, CPUID_REG_ECX, bit)
    X86_FEATURE_SSE3          = FEAT(0),  /* 0x00000001  Streaming SIMD Extensions 3 */
    X86_FEATURE_PCLMULQDQ     = FEAT(1),  /* 0x00000002  PCLMULQDQ Instruction */
    X86_FEATURE_DTES64        = FEAT(2),  /* 0x00000004  64-bit DS Area */
    X86_FEATURE_MONITOR       = FEAT(3),  /* 0x00000008  MONITOR/MWAIT Instructions */
    X86_FEATURE_DS_CPL        = FEAT(4),  /* 0x00000010  CPL Qualified Debug Store */
    X86_FEATURE_VMX           = FEAT(5),  /* 0x00000020  Virtual Machine Extensions */
    X86_FEATURE_SMX           = FEAT(6),  /* 0x00000040  Safer Mode Extensions */
    X86_FEATURE_EIST          = FEAT(7),  /* 0x00000080  Enhanced Intel SpeedStep technology */
    X86_FEATURE_TM2           = FEAT(8),  /* 0x00000100  Thermal Monitor 2 */
    X86_FEATURE_SSSE3         = FEAT(9),  /* 0x00000200  Supplemental Streaming SIMD Extensions 3 */
    X86_FEATURE_CNXT_ID       = FEAT(10), /* 0x00000400  L1 Context ID */
    X86_FEATURE_SDBG          = FEAT(11), /* 0x00000800  Silicon Debug Interface */
    X86_FEATURE_FMA           = FEAT(12), /* 0x00001000  Fused Multiply-Add  */
    X86_FEATURE_CMPXCHG16B    = FEAT(13), /* 0x00002000  CMPXCHG16B Instruction */
    X86_FEATURE_XTPR_UPDATE   = FEAT(14), /* 0x00004000  xTPR Update Control */
    X86_FEATURE_PDCM          = FEAT(15), /* 0x00008000  Perfmon and Debug Capability */
    X86_FEATURE_PCID          = FEAT(17), /* 0x00020000  Process-context identifiers */
    X86_FEATURE_DCA           = FEAT(18), /* 0x00040000  Direct cache access for DMA writes */
    X86_FEATURE_SSE41         = FEAT(19), /* 0x00080000  Streaming SIMD Extensions 4.1 */
    X86_FEATURE_SSE42         = FEAT(20), /* 0x00100000  Streaming SIMD Extensions 4.2 */
    X86_FEATURE_X2APIC        = FEAT(21), /* 0x00200000  x2APIC support */
    X86_FEATURE_MOVBE         = FEAT(22), /* 0x00400000  MOVBE Instruction */
    X86_FEATURE_POPCNT        = FEAT(23), /* 0x00800000  POPCNT Instruction */
    X86_FEATURE_TSC_DEADLINE  = FEAT(24), /* 0x01000000  APIC supports one-shot operation using TSC deadline */
    X86_FEATURE_AESNI         = FEAT(25), /* 0x02000000  AESNI Extension */
    X86_FEATURE_XSAVE         = FEAT(26), /* 0x04000000  XSAVE/XRSTOR/XSETBV/XGETBV Instructions and XCR0 */
    X86_FEATURE_OSXSAVE       = FEAT(27), /* 0x08000000  XSAVE enabled by OS */
    X86_FEATURE_AVX           = FEAT(28), /* 0x10000000  Advanced Vector Extensions */
    X86_FEATURE_F16C          = FEAT(29), /* 0x20000000  16-bit Floating-Point Instructions */
    X86_FEATURE_RDRAND        = FEAT(30), /* 0x40000000  RDRAND Instruction */
    X86_FEATURE_HYPERVISOR    = FEAT(31), /* 0x80000000  Hypervisor Running */
#undef FEAT

    /*
     * Intel SDM Vol. 2A: Table 3-11.
     * More on Feature Information Returned in the EDX Register
     * Features for CPUID with EAX=01h stored in EDX
     */
#define FEAT(bit) \
    FEATURE_KEY_LEAF(1, 0x01, CPUID_REG_EDX, bit)
    X86_FEATURE_FPU           = FEAT(0),  /* 0x00000001  Floating Point Unit On-Chip */
    X86_FEATURE_VME           = FEAT(1),  /* 0x00000002  Virtual 8086 Mode Enhancements */
    X86_FEATURE_DE            = FEAT(2),  /* 0x00000004  Debugging Extensions */
    X86_FEATURE_PSE           = FEAT(3),  /* 0x00000008  Page Size Extension */
    X86_FEATURE_TSC           = FEAT(4),  /* 0x00000010  Time Stamp Counter */
    X86_FEATURE_MSR           = FEAT(5),  /* 0x00000020  RDMSR/WRMSR Instructions */
    X86_FEATURE_PAE           = FEAT(6),  /* 0x00000040  Physical Address Extension */
    X86_FEATURE_MCE           = FEAT(7),  /* 0x00000080  Machine Check Exception */
    X86_FEATURE_CX8           = FEAT(8),  /* 0x00000100  CMPXCHG8B Instruction */
    X86_FEATURE_APIC          = FEAT(9),  /* 0x00000200  APIC On-Chip */
    X86_FEATURE_SEP           = FEAT(11), /* 0x00000800  SYSENTER/SYSEXIT Instructions */
    X86_FEATURE_MTRR          = FEAT(12), /* 0x00001000  Memory Type Range Registers */
    X86_FEATURE_PGE           = FEAT(13), /* 0x00002000  Page Global Bit */
    X86_FEATURE_MCA           = FEAT(14), /* 0x00004000  Machine Check Architecture */
    X86_FEATURE_CMOV          = FEAT(15), /* 0x00008000  Conditional Move Instructions */
    X86_FEATURE_PAT           = FEAT(16), /* 0x00010000  Page Attribute Table */
    X86_FEATURE_PSE36         = FEAT(17), /* 0x00020000  36-Bit Page Size Extension */
    X86_FEATURE_PSN           = FEAT(18), /* 0x00040000  Processor Serial Number */
    X86_FEATURE_CLFSH         = FEAT(19), /* 0x00080000  CLFLUSH Instruction */
    X86_FEATURE_DS            = FEAT(21), /* 0x00200000  Debug Store */
    X86_FEATURE_ACPI          = FEAT(22), /* 0x00400000  Thermal Monitor and Software Controlled Clock Facilities */
    X86_FEATURE_MMX           = FEAT(23), /* 0x00800000  Intel MMX Technology */
    X86_FEATURE_FXSR          = FEAT(24), /* 0x01000000  FXSAVE and FXRSTOR Instructions */
    X86_FEATURE_SSE           = FEAT(25), /* 0x02000000  Streaming SIMD Extensions */
    X86_FEATURE_SSE2          = FEAT(26), /* 0x04000000  Streaming SIMD Extensions 2 */
    X86_FEATURE_SS            = FEAT(27), /* 0x08000000  Self Snoop */
    X86_FEATURE_HTT           = FEAT(28), /* 0x10000000  Max APIC IDs reserved field is Valid */
    X86_FEATURE_TM            = FEAT(29), /* 0x20000000  Thermal Monitor */
    X86_FEATURE_PBE           = FEAT(31), /* 0x80000000  Pending Break Enable */
#undef FEAT

    /*
     * Intel SDM Vol. 2A: Table 3-8. Information Returned by CPUID Instruction
     * Structured Extended Feature Flags Enumeration Leaf
     * Features for CPUID with EAX=07h, ECX=00h stored in ECX
     */
#define FEAT(bit) \
    FEATURE_KEY_SUBLEAF(2, 0x07, 0x00, CPUID_REG_ECX, bit)
    X86_FEATURE_PREFETCHWT1   = FEAT(0),  /* 0x00000001  PREFETCHWT1 Instruction */
    X86_FEATURE_AVX512_VBMI   = FEAT(1),  /* 0x00000002  AVX-512 Vector Bit Manipulation Instructions */
    X86_FEATURE_UMIP          = FEAT(2),  /* 0x00000004  User-Mode Instruction Prevention */
    X86_FEATURE_PKU           = FEAT(3),  /* 0x00000008  Protection Keys for User-mode pages */
    X86_FEATURE_OSPKE         = FEAT(4),  /* 0x00000010  PKU enabled by OS */
    X86_FEATURE_RDPID         = FEAT(22), /* 0x00400000  RDPID Instruction and IA32_TSC_AUX MSR */
#undef FEAT

    /*
     * Intel SDM Vol. 2A: Table 3-8. Information Returned by CPUID Instruction
     * Structured Extended Feature Flags Enumeration Leaf
     * Features for CPUID with EAX=07h, ECX=00h stored in EBX
     */
#define FEAT(bit) \
    FEATURE_KEY_SUBLEAF(3, 0x07, 0x00, CPUID_REG_EBX, bit)
    X86_FEATURE_FSGSBASE      = FEAT(0),  /* 0x00000001  RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE supported */
    X86_FEATURE_TSC_ADJUST    = FEAT(1),  /* 0x00000002  MSR IA32_TSC_ADJUST supported */
    X86_FEATURE_SGX           = FEAT(2),  /* 0x00000004  Software Guard Extensions */
    X86_FEATURE_BMI1          = FEAT(3),  /* 0x00000008  Bit Manipulation Instruction Set 1 */
    X86_FEATURE_HLE           = FEAT(4),  /* 0x00000010  Transactional Synchronization Extensions */
    X86_FEATURE_AVX2          = FEAT(5),  /* 0x00000020  Advanced Vector Extensions 2 */
    X86_FEATURE_SMEP          = FEAT(7),  /* 0x00000080  Supervisor-Mode Execution Prevention */
    X86_FEATURE_BMI2          = FEAT(8),  /* 0x00000100  Bit Manipulation Instruction Set 2 */
    X86_FEATURE_INVPCID       = FEAT(10), /* 0x00000400  INVPCID instruction */
    X86_FEATURE_RTM           = FEAT(11), /* 0x00000800  Transactional Synchronization Extensions */
    X86_FEATURE_RDT_M         = FEAT(12), /* 0x00001000  Resource Director Technology Monitoring */
    X86_FEATURE_MPX           = FEAT(14), /* 0x00004000  Memory Protection Extensions */
    X86_FEATURE_RDT_A         = FEAT(15), /* 0x00008000  Resource Director Technology Allocation */
    X86_FEATURE_AVX512F       = FEAT(16), /* 0x00010000  AVX-512 Foundation */
    X86_FEATURE_AVX512DQ      = FEAT(17), /* 0x00020000  AVX-512 Doubleword and Quadword Instructions */
    X86_FEATURE_RDSEED        = FEAT(18), /* 0x00040000  RDSEED Instruction */
    X86_FEATURE_ADX           = FEAT(19), /* 0x00080000  Multi-Precision Add-Carry Instruction Extensions */
    X86_FEATURE_SMAP          = FEAT(20), /* 0x00100000  Supervisor Mode Access Prevention */
    X86_FEATURE_AVX512_IFMA   = FEAT(21), /* 0x00200000  AVX-512 Integer Fused Multiply-Add Instructions */
    X86_FEATURE_CLFLUSHOPT    = FEAT(23), /* 0x00800000  CLFLUSHOPT Instruction */
    X86_FEATURE_CLWB          = FEAT(24), /* 0x01000000  CLWB Instruction */
    X86_FEATURE_AVX512PF      = FEAT(26), /* 0x04000000  AVX-512 Prefetch Instructions */
    X86_FEATURE_AVX512ER      = FEAT(27), /* 0x08000000  AVX-512 Exponential and Reciprocal Instructions */
    X86_FEATURE_AVX512CD      = FEAT(28), /* 0x10000000  AVX-512 Conflict Detection Instructions*/
    X86_FEATURE_SHA           = FEAT(29), /* 0x20000000  SHA Extension */
    X86_FEATURE_AVX512BW      = FEAT(30), /* 0x40000000  AVX-512 Byte and Word Instructions */
    X86_FEATURE_AVX512VL      = FEAT(31), /* 0x80000000  AVX-512 Vector Length Extensions */
#undef FEAT

    /*
     * Intel SDM Vol. 2A: Table 3-8. Information Returned by CPUID Instruction
     * Extended Function CPUID Information
     * Features for CPUID with EAX=80000001h stored in ECX
     */
#define FEAT(bit) \
    FEATURE_KEY_LEAF(4, 0x80000001, CPUID_REG_ECX, bit)
    X86_FEATURE_LAHF          = FEAT(0),  /* 0x00000001  LAHF/SAHF Instructions */
    X86_FEATURE_PREFETCHW     = FEAT(8),  /* 0x00000100  PREFETCH/PREFETCHW instructions */
#undef FEAT

    /*
     * Intel SDM Vol. 2A: Table 3-8. Information Returned by CPUID Instruction
     * Extended Function CPUID Information
     * Features for CPUID with EAX=80000001h stored in EDX
     */
#define FEAT(bit) \
    FEATURE_KEY_LEAF(5, 0x80000001, CPUID_REG_EDX, bit)
    X86_FEATURE_SYSCALL       = FEAT(11), /* 0x00000800  SYSCALL/SYSRET Instructions */
    X86_FEATURE_NX            = FEAT(20), /* 0x00100000  No-Execute Bit */
    X86_FEATURE_PDPE1GB       = FEAT(26), /* 0x04000000  Gibibyte pages */
    X86_FEATURE_RDTSCP        = FEAT(27), /* 0x08000000  RDTSCP Instruction */
    X86_FEATURE_EM64T         = FEAT(29), /* 0x20000000  Long Mode */
#undef FEAT
};

// Functions
void cpuid_query_leaf(cpuid_args_t *args, uint32_t leaf);
void cpuid_query_subleaf(cpuid_args_t *args, uint32_t leaf, uint32_t subleaf);

void cpuid_host_init(void);
bool cpuid_host_has_feature(uint32_t feature_key);
bool cpuid_host_has_feature_uncached(uint32_t feature_key);

void cpuid_init_supported_features(void);
uint32_t cpuid_guest_get_size(void);
void cpuid_guest_init(hax_cpuid_t *cpuid);
void cpuid_execute(hax_cpuid_t *cpuid, cpuid_args_t *args);
void cpuid_get_features_mask(hax_cpuid_t *cpuid, uint64_t *features_mask);
void cpuid_set_features_mask(hax_cpuid_t *cpuid, uint64_t features_mask);
void cpuid_get_guest_features(hax_cpuid_t *cpuid, hax_cpuid_entry *features);
void cpuid_set_guest_features(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info);

#endif /* HAX_CORE_CPUID_H_ */
