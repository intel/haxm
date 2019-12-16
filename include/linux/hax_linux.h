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

#ifndef HAX_LINUX_HAX_LINUX_H_
#define HAX_LINUX_HAX_LINUX_H_

#define HAX_RAM_ENTRY_SIZE 0x4000000

hax_spinlock *hax_spinlock_alloc_init(void);
void hax_spinlock_free(hax_spinlock *lock);
void hax_spin_lock(hax_spinlock *lock);
void hax_spin_unlock(hax_spinlock *lock);

hax_mutex hax_mutex_alloc_init(void);
void hax_mutex_lock(hax_mutex lock);
void hax_mutex_unlock(hax_mutex lock);
void hax_mutex_free(hax_mutex lock);

/* Return true if the bit is set already */
int hax_test_and_set_bit(int bit, uint64_t *memory);

/* Return true if the bit is cleared already */
int hax_test_and_clear_bit(int bit, uint64_t *memory);

/* Don't care for the big endian situation */
static inline bool hax_test_bit(int bit, uint64_t *memory)
{
    int byte = bit / 8;
    unsigned char *p;
    int offset = bit % 8;

    p = (unsigned char *)memory + byte;
    return !!(*p & (1 << offset));
}

// memcpy_s() is part of the optional Bounds Checking Interfaces specified in
// Annex K of the C11 standard:
//  http://en.cppreference.com/w/c/string/byte/memcpy
// However, it is not implemented by Clang:
//  https://stackoverflow.com/questions/40829032/how-to-install-c11-compiler-on-mac-os-with-optional-string-functions-included
// Provide a simplified implementation here so memcpy_s() can be used instead of
// memcpy() everywhere else, which helps reduce the number of Klocwork warnings.
static inline int memcpy_s(void *dest, size_t destsz, const void *src,
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

/* Why it's a bool? Strange */
bool hax_cmpxchg32(uint32_t old_val, uint32_t new_val, volatile uint32_t *addr);
bool hax_cmpxchg64(uint64_t old_val, uint64_t new_val, volatile uint64_t *addr);

int hax_notify_host_event(enum hax_notify_event event, uint32_t *param,
                          uint32_t size);

//#define hax_assert(condition) BUG_ON(!(condition))
void hax_assert(bool condition);

#endif  // HAX_LINUX_HAX_LINUX_H_
