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

#ifndef HAX_CORE_SEGMENTS_H_
#define HAX_CORE_SEGMENTS_H_

#include "ia32.h"
// TODO: Get rid of is_compatible(), and then delete the following #include
#include "../../include/hax_interface.h"

#ifdef HAX_COMPILER_MSVC
#pragma pack(push, 1)
#endif

struct seg_desc_t {
    union {
        struct {
            uint64_t _limit0      : 16;
            uint64_t _base0       : 24;
            uint64_t _type        : 4;
            uint64_t _s           : 1;
            uint64_t _dpl         : 2;
            uint64_t _present     : 1;
            uint64_t _limit1      : 4;
            uint64_t _avl         : 1;
            uint64_t _longmode    : 1;
            uint64_t _d           : 1;
            uint64_t _granularity : 1;
            uint64_t _base1       : 8;
        } PACKED;
        uint64_t _raw;
    };
};

struct PACKED system_desc_t {
    uint16_t _limit;
    HAX_VADDR_T _base;
};

#ifdef HAX_COMPILER_MSVC
#pragma pack(pop)
#endif

typedef struct system_desc_t system_desc_t;

/*
 * This is to pass to VMCS, it should return uint64_t on long or compatible mode
 * and return uint32_t on pure 32-bit mode.
 * TODO: Fix it in 32-bit environment
 */

static inline uint64_t get_kernel_gdtr_base_4vmcs(void)
{
    system_desc_t sys_desc;

    get_kernel_gdt(&sys_desc);
    return sys_desc._base;
}

/*
 * In compatible mode, we need to return uint32_t.
 * Good luck for us is Mac has dual map for this.
 */

static inline mword get_kernel_gdtr_base(void)
{
    system_desc_t sys_desc;

    get_kernel_gdt(&sys_desc);
    return sys_desc._base;
}

static inline uint64_t get_kernel_idtr_base(void)
{
    system_desc_t sys_desc;

    get_kernel_idt(&sys_desc);
    return sys_desc._base;
}

static inline uint64_t get_kernel_ldtr_base(void)
{
    uint16_t ldt_sector = 0;
    mword gdtr_base = 0;
    uint64_t desc_base;
    struct seg_desc_t *seg_desc;

    gdtr_base = get_kernel_gdtr_base();
    ldt_sector = get_kernel_ldt();
    seg_desc = (struct seg_desc_t *)(gdtr_base) + (ldt_sector >> 3);
    desc_base = (seg_desc->_base0 + (seg_desc->_base1 << 24)) & 0xffffffff;
#ifdef HAX_ARCH_X86_64
    /* Table 3-2. TSS descriptor has 16 bytes on ia32e */
    desc_base = ((((struct seg_desc_t *)(seg_desc + 1))->_raw) << 32)
                + (desc_base & 0xffffffff);
#else
    if (is_compatible()) {
        desc_base = ((((struct seg_desc_t *)(seg_desc + 1))->_raw) << 32)
                    + (desc_base & 0xffffffff);
    }
#endif
    return desc_base;
}

static inline uint64_t get_tr_desc_base(uint16_t selector)
{
    mword gdtr_base;
    uint64_t desc_base;
    struct seg_desc_t *seg_desc;

    gdtr_base = get_kernel_gdtr_base();
    seg_desc = (struct seg_desc_t *)(gdtr_base) + (selector >> 3);
    desc_base = (seg_desc->_base0 + (seg_desc->_base1 << 24)) & 0xffffffff;
#ifdef HAX_ARCH_X86_64
    /* Table 3-2. TSS descriptor has 16 bytes on ia32e */
    desc_base = ((((struct seg_desc_t *)(seg_desc + 1))->_raw) << 32)
                + (desc_base & 0xffffffff);
#else
    if (is_compatible()) {
        desc_base = ((((struct seg_desc_t *)(seg_desc + 1))->_raw) << 32)
                    + (desc_base & 0xffffffff);
    }
#endif
    return desc_base;
}

static inline uint32_t get_kernel_fs_gs_base(uint16_t selector)
{
    mword gdtr_base;
    mword desc_base;
    struct seg_desc_t *seg_desc;

    gdtr_base = get_kernel_gdtr_base();
    seg_desc = (struct seg_desc_t *)(gdtr_base) + (selector >> 3);
    desc_base = seg_desc->_base0 + (seg_desc->_base1 << 24);
    return desc_base;
}

static inline uint64_t get_kernel_tr_base(void)
{
    uint16_t selector = get_kernel_tr_selector();
    return get_tr_desc_base(selector);
}

#endif  // HAX_CORE_SEGMENTS_H_
