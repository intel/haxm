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

#ifndef HAX_DARWIN_HAX_MAC_H_
#define HAX_DARWIN_HAX_MAC_H_

#include <libkern/OSAtomic.h>
#include <mach/mach_types.h>
#include <IOKit/IOLib.h>
#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
#include <sys/ioccom.h>
#include <sys/errno.h>
#include <kern/locks.h>
#include <libkern/OSBase.h>

#define HAX_RAM_ENTRY_SIZE 0x4000000

/* Spinlock related definition */
__attribute__((visibility("hidden"))) extern lck_grp_t *hax_lck_grp;
__attribute__((visibility("hidden"))) extern lck_attr_t *hax_lck_attr;

#define hax_spin_lock(_lock) lck_spin_lock(_lock)

#define hax_spin_unlock(_lock) lck_spin_unlock(_lock)
#define hax_spin_lock_alloa_initc() \
        lck_spin_alloc_init(hax_lck_grp, hax_lck_attr)
#define hax_spinlock_init(_lock) \
        lck_spin_init(_lock, hax_lck_grp, hax_lck_attr)

static inline hax_spinlock *hax_spinlock_alloc_init(void)
{
    return lck_spin_alloc_init(hax_lck_grp, hax_lck_attr);
}

#define hax_spinlock_free(_lock) lck_spin_free(_lock, hax_lck_grp)

__attribute__((visibility("hidden"))) extern lck_grp_t *hax_mtx_grp;
__attribute__((visibility("hidden"))) extern lck_attr_t *hax_mtx_attr;

#define hax_mutex_lock(_lock) lck_mtx_lock(_lock)
#define hax_mutex_unlock(_lock) lck_mtx_unlock(_lock)
#define hax_mutext_init(_lock) lck_mtx_init(_lcok, hax_mtx_grp, hax_mtx_attr)

static inline hax_mutex hax_mutex_alloc_init(void)
{
    return lck_mtx_alloc_init(hax_mtx_grp, hax_mtx_attr);
}

#define hax_mutex_free(_lock) lck_mtx_free(_lock, hax_mtx_grp)

static inline hax_rw_lock *hax_rwlock_alloc_init(void)
{
    return lck_rw_alloc_init(hax_lck_grp, hax_lck_attr);
}

#define hax_rwlock_lock_read(lck) lck_rw_lock(lck, LCK_RW_TYPE_SHARED)
#define hax_rwlock_unlock_read(lck) lck_rw_unlock(lck, LCK_RW_TYPE_SHARED)
#define hax_rwlock_lock_write(lck) lck_rw_lock(lck, LCK_RW_TYPE_EXCLUSIVE)
#define hax_rwlock_unlock_write(lck) lck_rw_unlock(lck, LCK_RW_TYPE_EXCLUSIVE)
#define hax_rwlock_free(lck) lck_rw_free(lck, hax_lck_grp)

/* Don't care for the big endian situation */
static bool hax_test_bit(int bit, uint64_t *memory)
{
    int byte = bit / 8;
    unsigned char *p;
    int offset = (bit % 8);

    p = (unsigned char *)memory + byte;
    return !!(*p & (0x1 << offset));
}

/* Return true if the bit is set already */
static int hax_test_and_set_bit(int bit, uint64_t *memory)
{
    int byte = bit / 8 ;
    unsigned char *p;
    int offset = 7 - (bit % 8);

    p = (unsigned char *)memory + byte;
    return OSTestAndSet(offset, p);
}

/* Return true if the bit is cleared already */
static int hax_test_and_clear_bit(int bit, uint64_t *memory)
{
    int byte = bit / 8;
    unsigned char *p;
    int offset = 7 - (bit % 8);

    p = (unsigned char *)memory + byte;
    return OSTestAndClear(offset, p);
}

static bool hax_cmpxchg32(uint32_t old_val, uint32_t new_val, volatile uint32_t *addr)
{
    return OSCompareAndSwap(old_val, new_val, addr);
}

static bool hax_cmpxchg64(uint64_t old_val, uint64_t new_val, volatile uint64_t *addr)
{
    return OSCompareAndSwap64(old_val, new_val, addr);
}

static inline int hax_notify_host_event(enum hax_notify_event event,
                                        uint32_t *param, uint32_t size)
{
    return 0;
}

// memcpy_s() is part of the optional Bounds Checking Interfaces specified in
// Annex K of the C11 standard:
//  http://en.cppreference.com/w/c/string/byte/memcpy
// However, it is not implemented by Clang:
//  https://stackoverflow.com/questions/40829032/how-to-install-c11-compiler-on-mac-os-with-optional-string-functions-included
// Provide a simplified implementation here so memcpy_s() can be used instead of
// memcpy() everywhere else, which helps reduce the number of Klocwork warnings.
static inline errno_t memcpy_s(void *dest, size_t destsz, const void *src,
                               size_t count)
{
    char *dest_start = (char *)dest;
    char *dest_end = (char *)dest + destsz;
    char *src_start = (char *)src;
    char *src_end = (char *)src + count;
    bool overlap;

    if (count == 0)
        return 0;

    if (!dest || destsz == 0)
        return -EINVAL;

    overlap = src_start < dest_start
              ? dest_start < src_end : src_start < dest_end;
    if (!src || count > destsz || overlap) {
        memset(dest, 0, destsz);
        return -EINVAL;
    }

    memcpy(dest, src, count);
    return 0;
}

#define hax_assert(condition) assert(condition)

#endif  // HAX_DARWIN_HAX_MAC_H_
