/*
 * Copyright (c) 2011 Intel Corporation
 * Copyright (c) 2018 Kryptos Logic
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

#ifndef HAX_LINUX_HAX_TYPES_LINUX_H_
#define HAX_LINUX_HAX_TYPES_LINUX_H_

#include <linux/types.h>
#define _ASM_X86_CPUFEATURES_H
#include <linux/string.h>
#undef _ASM_X86_CPUFEATURES_H
#include <linux/errno.h>

// Signed Types
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

// Unsigned Types
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef unsigned int  uint;
typedef unsigned long ulong;
typedef unsigned long ulong_t;

#if defined(__i386__)
typedef uint32_t mword;
#endif
#if defined (__x86_64__)
typedef uint64_t mword;
#endif
typedef mword HAX_VADDR_T;

#include "../hax_list.h"
struct hax_page {
    void *kva;
    struct page *page;
    uint64_t pa;
    uint32_t order;
    uint32_t flags;
    struct hax_link_list list;
};

typedef struct hax_memdesc_user {
    int nr_pages;
    struct page **pages;
} hax_memdesc_user;

typedef struct hax_kmap_user {
    void *kva;
} hax_kmap_user;

typedef struct hax_memdesc_phys {
    struct page *ppage;
} hax_memdesc_phys;

typedef struct hax_kmap_phys {
    void *kva;
} hax_kmap_phys;

typedef struct hax_spinlock hax_spinlock;

typedef uint64_t hax_cpumask_t;
typedef void hax_smp_func_ret_t;

/* Remove this later */
#define is_leaf(x)  1

typedef mword preempt_flag;
typedef void *hax_mutex;
typedef uint32_t hax_atomic_t;

/* Return the value before add */
hax_atomic_t hax_atomic_add(volatile hax_atomic_t *atom, uint32_t value);

/* Return the value before the increment */
hax_atomic_t hax_atomic_inc(volatile hax_atomic_t *atom);

/* Return the value before the decrement */
hax_atomic_t hax_atomic_dec(volatile hax_atomic_t *atom);    

void hax_smp_mb(void);

#endif  // HAX_LINUX_HAX_TYPES_LINUX_H_
