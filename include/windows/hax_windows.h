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

#ifndef HAX_WINDOWS_HAX_WINDOWS_H_
#define HAX_WINDOWS_HAX_WINDOWS_H_

//#include <ntddk.h>
#include <ntifs.h>

#ifndef HAX_UNIFIED_BINARY

/*
 * According to DDK, the IoAllocateMdl can support at mos
 * 64M - PAGE_SIZE * (sizeof(MDL)) / sizeof(ULONG_PTR), so
 * take 32M here
 */
#if (NTDDI_VERSION <= NTDDI_WS03)
#define HAX_RAM_ENTRY_SIZE 0x2000000
#else
#define HAX_RAM_ENTRY_SIZE 0x4000000
#endif
#else /* HAX_UNIFIED_BINARY */
#define HAX_RAM_ENTRY_SIZE 0x2000000
#endif

static inline hax_spinlock *hax_spinlock_alloc_init(void)
{
    hax_spinlock *lock;

    lock = hax_vmalloc(sizeof(hax_spinlock), 0);
    if (!lock)
        return NULL;
    KeInitializeSpinLock(&lock->lock);
    return lock;
}

#define hax_spinlock_free(_lock) hax_vfree(_lock, sizeof(hax_spinlock))

static inline void hax_spin_lock(hax_spinlock *lock)
{
    KIRQL old_irq;
    ASSERT(lock);
    KeAcquireSpinLock(&lock->lock, &old_irq);
    lock->old_irq = old_irq;
}

/* Do we need a flag to track if old_irq is valid? */
static inline void hax_spin_unlock(hax_spinlock *lock)
{
    ASSERT(lock);
    KeReleaseSpinLock(&lock->lock, lock->old_irq);
}

static inline hax_mutex hax_mutex_alloc_init(void)
{
    hax_mutex mut;

    mut = (hax_mutex)hax_vmalloc(sizeof(FAST_MUTEX), 0);
    if (!mut)
        return NULL;

    ExInitializeFastMutex(mut);
    return mut;
}

static inline void hax_mutex_lock(hax_mutex lock)
{
    ExAcquireFastMutex(lock);
}

static inline void hax_mutex_unlock(hax_mutex lock)
{
    ExReleaseFastMutex(lock);
}

static inline void hax_mutex_free(hax_mutex lock)
{
    hax_vfree(lock, sizeof(FAST_MUTEX));
}

/* Return true if the bit is set already */
static int hax_test_and_set_bit(int bit, uint64_t *memory)
{
    long *base = (long *)memory;
    long nr_long;
    long bitoffset_in_long;
    long bits_per_long = sizeof(long) * 8;

    nr_long = bit / bits_per_long;
    base += nr_long;
    bitoffset_in_long = bit % bits_per_long;

    // InterlockedBitTestAndSet is implemented using a compiler intrinsic where
    // possible. For more information, see the WinBase.h header file and
    // _interlockedbittestandset.
    // ref: https://msdn.microsoft.com/en-us/library/windows/desktop/
    //      ms683549(v=vs.85).aspx
    return InterlockedBitTestAndSet(base, bitoffset_in_long);
}

/*
 * Return true if the bit is cleared already
 * Notice that InterlockedBitTestAndReset return original value in that bit
 */
static int hax_test_and_clear_bit(int bit, uint64_t *memory)
{
    long * base = (long *)memory;
    long nr_long;
    long bitoffset_in_long;
    long bits_per_long = sizeof(long) * 8;

    nr_long = bit / bits_per_long;
    base += nr_long;
    bitoffset_in_long = bit % bits_per_long;

    // InterlockedBitTestAndReset is implemented using a compiler intrinsic
    // where possible. For more information, see the WinBase.h header file and
    // __interlockedbittestandreset.
    // ref: https://msdn.microsoft.com/en-us/library/windows/desktop/
    //      ms683546(v=vs.85).aspx
    return !InterlockedBitTestAndReset(base, bitoffset_in_long);
}

/* Don't care for the big endian situation */
static bool hax_test_bit(int bit, uint64_t *memory)
{
    int byte = bit / 8;
    unsigned char *p;
    int offset = bit % 8;

    p = (unsigned char *)memory + byte;
    return !!(*p & (1 << offset));
}

/* Why it's a bool? Strange */
static bool hax_cmpxchg32(uint32_t old_val, uint32_t new_val, volatile uint32_t *addr)
{
    long ret;

    ret = InterlockedCompareExchange(addr, new_val, old_val);

    if (ret == old_val)
        return TRUE;
    else
        return FALSE;
}

static bool hax_cmpxchg64(uint64_t old_val, uint64_t new_val, volatile uint64_t *addr)
{
    LONGLONG ret;

    ret = InterlockedCompareExchange64(addr, new_val, old_val);

    if (ret == old_val)
        return TRUE;
    else
        return FALSE;
}

int hax_notify_host_event(enum hax_notify_event event, uint32_t *param,
                          uint32_t size);

#define hax_assert(condition) ASSERT(condition)

#endif  // HAX_WINDOWS_HAX_WINDOWS_H_
