/*
 * Copyright (c) 2018 Kamil Rytarowski
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/xcall.h>
#include <sys/cpu.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>


#include "../../include/hax.h"
#include "../../core/include/hax_core_interface.h"
#include "../../core/include/ia32.h"

int max_cpus;
hax_cpumap_t cpu_online_map;

static const char* kLogPrefix[] = {
    "haxm: ",
    "haxm_debug: ",
    "haxm_info: ",
    "haxm_warning: ",
    "haxm_error: ",
    "haxm_panic: "
};

void hax_log(int level, const char *fmt,  ...)
{
    va_list args;
    va_start(args, fmt);
    if (level >= HAX_LOG_DEFAULT) {
        printf("%s", kLogPrefix[level]);
        vprintf(fmt, args);
    }
    va_end(args);
}

void hax_panic(const char *fmt,  ...)
{
    va_list args;
    va_start(args, fmt);
    hax_log(HAX_LOGPANIC, fmt, args);
    panic(fmt, args);
    va_end(args);
}

uint32_t hax_cpuid(void)
{
    return cpu_index(curcpu());
}

typedef struct smp_call_parameter {
    void (*func)(void *);
    void *param;
    hax_cpumap_t *cpus;
} smp_call_parameter;

static void smp_cfunction(void *a1, void *a2 __unused)
{
    struct smp_call_parameter *info = a1;
    hax_cpumap_t *cpus;
    uint32_t cpuid;

    cpus = info->cpus;
    cpuid = hax_cpuid();
    if (*cpus & (0x1 << cpuid))
        info->func(info->param);
}

int hax_smp_call_function(hax_cpumap_t *cpus, void (*scfunc)(void *),
                          void *param)
{
    smp_call_parameter info;
    uint64_t xc;

    info.func = scfunc;
    info.param = param;
    info.cpus = cpus;
    xc = xc_broadcast(XC_HIGHPRI, smp_cfunction, &info, NULL);
    xc_wait(xc);
    return 0;
}

/* XXX */
int proc_event_pending(struct vcpu_t *vcpu)
{
    return vcpu_event_pending(vcpu);
}

void hax_disable_preemption(preempt_flag *eflags)
{
    kpreempt_disable();
}

void hax_enable_preemption(preempt_flag *eflags)
{
    kpreempt_enable();
}

void hax_enable_irq(void)
{
    x86_enable_intr();
}

void hax_disable_irq(void)
{
    x86_disable_intr();
}

/* Misc */
void hax_smp_mb(void)
{
    membar_sync();
}

/* Compare-Exchange */
bool hax_cmpxchg32(uint32_t old_val, uint32_t new_val, volatile uint32_t *addr)
{
    hax_atomic_t rv;

    membar_exit();
    rv = atomic_cas_32(addr, old_val, new_val);
    membar_enter();

    return rv == old_val;
}

bool hax_cmpxchg64(uint64_t old_val, uint64_t new_val, volatile uint64_t *addr)
{
    hax_atomic_t rv;

    membar_exit();
    rv = atomic_cas_64(addr, old_val, new_val);
    membar_enter();

    return rv == old_val;
}

/* Atomics */
hax_atomic_t hax_atomic_add(volatile hax_atomic_t *atom, uint32_t value)
{
    hax_atomic_t rv;

    membar_exit();
    rv = atomic_add_32_nv(atom, value) - value;
    membar_enter();

    return rv;
}

hax_atomic_t hax_atomic_inc(volatile hax_atomic_t *atom)
{
    hax_atomic_t rv;

    membar_exit();
    rv = atomic_inc_32_nv(atom) - 1;
    membar_enter();

    return rv;
}

hax_atomic_t hax_atomic_dec(volatile hax_atomic_t *atom)
{
    hax_atomic_t rv;

    membar_exit();
    rv = atomic_dec_32_nv(atom) + 1;
    membar_enter();

    return rv;
}

int hax_test_and_set_bit(int bit, uint64_t *memory)
{
    const unsigned int units = (sizeof(*memory) * CHAR_BIT);
    volatile uint64_t *const p = &memory[bit / units];
    const uint64_t mask = (1ULL << (bit % units));
    uint64_t v;

    membar_exit();
    do {
        v = *p;
    } while (atomic_cas_64(p, v, (v | mask)) != v);
    membar_enter();

    return ((v & mask) != 0);
}

int hax_test_and_clear_bit(int bit, uint64_t *memory)
{
    const unsigned int units = (sizeof(*memory) * CHAR_BIT);
    volatile uint64_t *const p = &memory[bit / units];
    const uint64_t mask = (1ULL << (bit % units));
    uint64_t v;

    membar_exit();
    do {
        v = *p;
    } while (atomic_cas_64(p, v, (v & ~mask)) != v);
    membar_enter();

    return ((v & mask) == 0);
}

/* Spinlock */
struct hax_spinlock {
    kmutex_t lock;
};

hax_spinlock *hax_spinlock_alloc_init(void)
{
    struct hax_spinlock *lock;

    lock = kmem_alloc(sizeof(struct hax_spinlock), KM_SLEEP);
    if (!lock) {
        hax_log(HAX_LOGE, "Could not allocate spinlock\n");
        return NULL;
    }
    mutex_init(&lock->lock, MUTEX_DEFAULT, IPL_VM);

    return lock;
}

void hax_spinlock_free(hax_spinlock *lock)
{
    if (!lock)
        return;

    mutex_destroy(&lock->lock);
    kmem_free(lock, sizeof(struct hax_spinlock));
}

void hax_spin_lock(hax_spinlock *lock)
{
    mutex_spin_enter(&lock->lock);
}

void hax_spin_unlock(hax_spinlock *lock)
{
    mutex_spin_exit(&lock->lock);
}

/* Mutex */
hax_mutex hax_mutex_alloc_init(void)
{
    kmutex_t *lock;

    lock = kmem_alloc(sizeof(kmutex_t), KM_SLEEP);
    if (!lock) {
        hax_log(HAX_LOGE, "Could not allocate mutex\n");
        return NULL;
    }
    mutex_init(lock, MUTEX_DEFAULT, IPL_NONE);
    return lock;
}

void hax_mutex_lock(hax_mutex lock)
{
    if (!lock)
        return;

    mutex_enter(lock);
}

void hax_mutex_unlock(hax_mutex lock)
{
    if (!lock)
        return;

    mutex_exit(lock);
}

void hax_mutex_free(hax_mutex lock)
{
    if (!lock)
        return;

    mutex_destroy(lock);
    kmem_free(lock, sizeof(kmutex_t));
}
