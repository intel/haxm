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

#ifndef HAX_DARWIN_HAX_TYPES_MAC_H_
#define HAX_DARWIN_HAX_TYPES_MAC_H_

#define CONFIG_KERNEL_HAX

#ifndef CONFIG_KERNEL_HAX
#include <mach/mach_types.h>
typedef uint64_t hax_va_t;
typedef uint32_t hax_size_t;

#else
#include <libkern/OSAtomic.h>
#include <IOKit/IOLib.h>
#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
#include <sys/ioccom.h>
#include <sys/errno.h>
#include <kern/locks.h>
#include <libkern/OSBase.h>

#include "../hax_list.h"
typedef uint64_t hax_va_t;
typedef uint32_t hax_size_t;

/* Spinlock releated definition */
typedef lck_spin_t hax_spinlock;
typedef lck_mtx_t* hax_mutex;
typedef lck_rw_t hax_rw_lock;

typedef SInt32 hax_atomic_t;

// Unsigned Types
typedef unsigned long       ulong;

/* Return the value before add */
static hax_atomic_t hax_atomic_add(hax_atomic_t *address, SInt32 amount)
{
    return OSAddAtomic(amount, address);
}

/* Return the value before the increment */
static hax_atomic_t hax_atomic_inc(hax_atomic_t *address)
{
    return OSIncrementAtomic(address);
}

/* Return the value before the decrement */
static hax_atomic_t hax_atomic_dec(hax_atomic_t *address)
{
    return OSDecrementAtomic(address);
}

/*
 * According to kernel programming, the Atomic function is barrier
 * Although we can write a hax_smp_mb from scrach, this simple one can resolve our
 * issue
 */
static inline void hax_smp_mb(void)
{
    SInt32 atom;
    OSAddAtomic(1, &atom);
}

struct IOBufferMemoryDescriptor;
struct IOMemoryMap;
struct IOMemoryDescriptor;
#ifdef __cplusplus
extern "C" {
#endif

struct hax_page {
    /* XXX TBD combine the md and bmd */
    struct IOMemoryDescriptor *md;
    struct IOBufferMemoryDescriptor *bmd;
    struct IOMemoryMap *map;
    uint8_t flags;
    int order;
    void *kva;
    uint64_t pa;
    struct hax_link_list list;
};

typedef struct hax_memdesc_user {
    struct IOMemoryDescriptor *md;
} hax_memdesc_user;

typedef struct hax_kmap_user {
    struct IOMemoryMap *mm;
} hax_kmap_user;

typedef struct hax_memdesc_phys {
    struct IOBufferMemoryDescriptor *bmd;
} hax_memdesc_phys;

typedef struct hax_kmap_phys {
    struct IOMemoryMap *mm;
} hax_kmap_phys;

#ifdef __cplusplus
}
#endif

typedef ulong mword;
typedef mword preempt_flag;
typedef uint64_t hax_cpumask_t;
typedef void hax_smp_func_ret_t;
typedef uint64_t HAX_VADDR_T;

#endif  // CONFIG_KERNEL_HAX
#endif  // HAX_DARWIN_HAX_TYPES_MAC_H_
