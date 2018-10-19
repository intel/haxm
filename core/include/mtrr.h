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

#ifndef HAX_CORE_MTRR_H_
#define HAX_CORE_MTRR_H_

#define NUM_FIXED_MTRRS    11
#define NUM_VARIABLE_MTRRS 10

enum fixed_mtrr_t {
    MTRR_FIX64K_00000_MSR = 0x250,
    MTRR_FIX16K_80000_MSR = 0x258,
    MTRR_FIX16K_A0000_MSR = 0x259,
    MTRR_FIX4K_C0000_MSR  = 0x268,
    MTRR_FIX4K_C8000_MSR  = 0x269,
    MTRR_FIX4K_D0000_MSR  = 0x26A,
    MTRR_FIX4K_D8000_MSR  = 0x26B,
    MTRR_FIX4K_E0000_MSR  = 0x26C,
    MTRR_FIX4K_E8000_MSR  = 0x26D,
    MTRR_FIX4K_F0000_MSR  = 0x26E,
    MTRR_FIX4K_F8000_MSR  = 0x26F
};
typedef enum fixed_mtrr_t fixed_mtrr_t;

enum var_mtrr_t {
    MTRR_PHYS_BASE0_MSR = 0x200,
    MTRR_PHYS_MASK0_MSR = 0x201,
    MTRR_PHYS_BASE1_MSR = 0x202,
    MTRR_PHYS_MASK1_MSR = 0x203,
    MTRR_PHYS_BASE2_MSR = 0x204,
    MTRR_PHYS_MASK2_MSR = 0x205,
    MTRR_PHYS_BASE3_MSR = 0x206,
    MTRR_PHYS_MASK3_MSR = 0x207,
    MTRR_PHYS_BASE4_MSR = 0x208,
    MTRR_PHYS_MASK4_MSR = 0x209,
    MTRR_PHYS_BASE5_MSR = 0x20A,
    MTRR_PHYS_MASK5_MSR = 0x20B,
    MTRR_PHYS_BASE6_MSR = 0x20C,
    MTRR_PHYS_MASK6_MSR = 0x20D,
    MTRR_PHYS_BASE7_MSR = 0x20E,
    MTRR_PHYS_MASK7_MSR = 0x20F,
    MTRR_PHYS_BASE8_MSR = 0x210,
    MTRR_PHYS_MASK8_MSR = 0x211,
    MTRR_PHYS_BASE9_MSR = 0x212,
    MTRR_PHYS_MASK9_MSR = 0x213
};
typedef enum var_mtrr_t var_mtrr_t;

enum mtrr_msrs_t {
    MTRR_CAP_MSR      = 0x0fe,
    MTRR_DEF_TYPE_MSR = 0x2ff
};
typedef enum mtrr_msrs_t mtrr_msrs_t;

typedef uint64_t mtrr_type_t;

union mtrr_cap_t {
    uint64_t raw;
    struct {
        uint64_t num_var_range_regs : 8;
        uint64_t fix_support        : 1;
        uint64_t reserved1          : 1;
        uint64_t wc_type_support    : 1;
        uint64_t reserved2          : 53;
    };
};
typedef union mtrr_cap_t mtrr_cap_t;

union mtrr_def_type_t {
    uint64_t raw;
    struct {
        uint64_t type               : 8;
        uint64_t reserved1          : 2;
        uint64_t fixed_mtrr_enable  : 1;
        uint64_t var_mtrr_enable    : 1;
        uint64_t reserved2          : 52;
    };
};
typedef union mtrr_def_type_t mtrr_def_type_t;

union mtrr_physbase_t {
    uint64_t raw;
    struct {
        uint64_t type               : 8;
        uint64_t reserved1          : 4;
        uint64_t base               : 24;
        uint64_t reserved2          : 28;
    };
};
typedef union mtrr_physbase_t mtrr_physbase_t;

union mtrr_physmask_t {
    uint64_t raw;
    struct {
        uint64_t reserved1          : 11;
        uint64_t valid              : 1;
        uint64_t mask               : 24;
        uint64_t reserved2          : 28;
    };
};
typedef union mtrr_physmask_t mtrr_physmask_t;

struct mtrr_var_t {
    mtrr_physbase_t base;
    mtrr_physmask_t mask;
};
typedef struct mtrr_var_t mtrr_var_t;

enum mtrr_attribute_t {
    MTRR_TYPE_UNCACHEABLE     = 0,
    MTRR_TYPE_WRITE_COMBINING = 1,
    MTRR_TYPE_WRITE_THROUGH   = 4,
    MTRR_TYPE_WRITE_PROTECT   = 5,
    MTRR_TYPE_WRITE_BACK      = 6
};
typedef enum mtrr_attribute_t mtrr_attribute_t;

struct mtrr_t {
    mtrr_cap_t      mtrr_cap;
    mtrr_def_type_t mtrr_def_type;
    mtrr_type_t     mtrr_fixed64k;
    mtrr_type_t     mtrr_fixed16k[2];
    mtrr_type_t     mtrr_fixed4k[8];
    mtrr_var_t      mtrr_var[10];
};

#endif  // HAX_CORE_MTRR_H_
