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

/* wrap not-memory hax interface to Mac function */

#include <mach/mach_types.h>
#include <IOKit/IOLib.h>

#include <libkern/libkern.h>
#include <stdarg.h>
#include <sys/proc.h>
#include "../../include/hax.h"
#include "../../core/include/ia32_defs.h"

extern "C" int vcpu_event_pending(struct vcpu_t *vcpu);

/*
 * From the following list, we have to do tricky things to achieve this simple
 * action.
 * http://lists.apple.com/archives/darwin-kernel/2006/Dec/msg00006.html
 * But we decide to stick to the legacy method of mp_redezvous_no_intr at least
 * currently
 */

static const char* kLogPrefix[] = {
    "haxm: ",
    "haxm_debug: ",
    "haxm_info: ",
    "haxm_warning: ",
    "haxm_error: ",
    "haxm_panic: "
};

extern "C" void hax_log(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (level >= HAX_LOG_DEFAULT) {
        printf("%s", kLogPrefix[level]);
        printf(fmt, args);
    }
    va_end(args);
}

extern "C" void hax_panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    hax_log(HAX_LOGPANIC, fmt, args);
    panic(fmt, args);
    va_end(args);
}

struct smp_call_parameter {
    void (*func)(void *);
    void *param;
    hax_cpumap_t *cpus;
};

extern "C" void mp_rendezvous_no_intrs(void (*action_func)(void *), void *arg);

void smp_cfunction(void *param)
{
    int cpu_id;
    void (*action)(void *parap);
    hax_cpumap_t *hax_cpus;
    struct smp_call_parameter *p;

    p = (struct smp_call_parameter *)param;
    cpu_id = cpu_number();
    action = p->func;
    hax_cpus = p->cpus;
    //printf("cpus:%llx, current_cpu:%x\n", *cpus, cpu_id);
    if (*hax_cpus & (0x1 << cpu_id))
        action(p->param);
}

extern "C" int hax_smp_call_function(hax_cpumap_t *cpus, void (*scfunc)(void *),
                                 void *param)
{
    struct smp_call_parameter sp;
    sp.func = scfunc;
    sp.param = param;
    sp.cpus = cpus;
    mp_rendezvous_no_intrs(smp_cfunction, &sp);
    return 0;
}

extern "C" uint32_t hax_cpuid()
{
    return cpu_number();
}

extern "C" void hax_disable_preemption(preempt_flag *eflags)
{
    mword flags;
#ifdef __x86_64__
    asm volatile (
        "pushfq         \n\t"
        "popq %0"
        : "=r" (flags)
    );
#else
    asm volatile (
        "pushfd         \n\t"
        "pop %0"
        : "=r" (flags)
    );
#endif
    *eflags = flags;
    hax_disable_irq();
}

extern "C" void hax_enable_irq(void)
{
    ml_set_interrupts_enabled(true);
}

extern "C" void hax_disable_irq(void)
{
    ml_set_interrupts_enabled(false);
}

extern "C" void hax_enable_preemption(preempt_flag *eflags)
{
    if (*eflags & EFLAGS_IF)
        hax_enable_irq();
}

#define QEMU_SIGNAL_SIGMASK  (sigmask(SIGINT) | sigmask(SIGTERM) |  \
                              sigmask(SIGKILL) | sigmask(SIGALRM) | \
                              sigmask(SIGIO) | sigmask(SIGHUP) |    \
                              sigmask(SIGINT) | sigmask(SIGTERM))

extern "C" int proc_event_pending(struct vcpu_t *vcpu)
{
    int proc_id = proc_selfpid();
    return (proc_issignal(proc_id, QEMU_SIGNAL_SIGMASK) ||
            vcpu_event_pending(vcpu));
}
