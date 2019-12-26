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
        vprintf(fmt, args);
    }
    va_end(args);
}

extern "C" void hax_panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    hax_log(HAX_LOGPANIC, fmt, args);
    (panic)(fmt, args);
    va_end(args);
}

extern "C" int cpu_number(void);
extern "C" uint32_t hax_cpu_id(void)
{
    return (uint32_t)cpu_number();
}

/* This is provided in unsupported kext */
extern unsigned int real_ncpus;
int cpu_info_init(void)
{
    uint32_t size_group, size_pos, cpu_id, group, bit;
    hax_cpumap_t omap = {0};

    memset(&cpu_online_map, 0, sizeof(cpu_online_map));

    cpu_online_map.cpu_num = real_ncpus;
    group = HAX_MAX_CPU_PER_GROUP;
    cpu_online_map.group_num = (cpu_online_map.cpu_num + group - 1) / group;
    size_group = cpu_online_map.group_num * sizeof(*cpu_online_map.cpu_map);
    size_pos = cpu_online_map.cpu_num * sizeof(*cpu_online_map.cpu_pos);

    if (cpu_online_map.group_num > HAX_MAX_CPU_GROUP ||
        cpu_online_map.cpu_num > HAX_MAX_CPUS) {
        hax_log(HAX_LOGE, "Too many cpus %d-%d in system\n",
                cpu_online_map.cpu_num, cpu_online_map.group_num);
        return -E2BIG;
    }

    cpu_online_map.cpu_map = (hax_cpu_group_t *)hax_vmalloc(size_group, 0);
    omap.cpu_map = (hax_cpu_group_t *)hax_vmalloc(size_group, 0);
    if (!cpu_online_map.cpu_map || !omap.cpu_map) {
        hax_log(HAX_LOGE, "Couldn't allocate cpu_map for cpu_online_map\n");
        goto fail_nomem;
    }

    cpu_online_map.cpu_pos = (hax_cpu_pos_t *)hax_vmalloc(size_pos, 0);
    omap.cpu_pos = (hax_cpu_pos_t *)hax_vmalloc(size_pos, 0);
    if (!cpu_online_map.cpu_pos || !omap.cpu_pos) {
        hax_log(HAX_LOGE, "Couldn't allocate cpu_pos for cpu_online_map\n");
        goto fail_nomem;
    }

    // omap is filled for get_online_map() to init all host cpu info.
    // Since smp_cfunction() will check if host cpu is online in cpu_online_map,
    // but the first call to smp_cfunction() is to init cpu_online_map itself.
    // Make smp_cfunction() always check group 0 bit 1 for get_online_map(),
    // so get_online_map() assumes all online and init the real cpu_online_map.
    omap.group_num = cpu_online_map.group_num;
    omap.cpu_num = cpu_online_map.cpu_num;
    for (cpu_id = 0; cpu_id < omap.cpu_num; cpu_id++) {
        omap.cpu_pos[cpu_id].group = 0;
        omap.cpu_pos[cpu_id].bit = 0;
    }
    for (group = 0; group < omap.group_num; group++) {
        omap.cpu_map[group].id = 0;
        omap.cpu_map[group].map = ~0ULL;
    }
    hax_smp_call_function(&omap, get_online_map, &cpu_online_map);

    for (group = 0; group < cpu_online_map.group_num; group++) {
        cpu_online_map.cpu_map[group].num = 0;
        for (bit = 0; bit < HAX_MAX_CPU_PER_GROUP; bit++) {
            if (cpu_online_map.cpu_map[group].map & ((hax_cpumask_t)1 << bit))
                ++cpu_online_map.cpu_map[group].num;
        }
    }

    hax_vfree(omap.cpu_map, size_group);
    hax_vfree(omap.cpu_pos, size_pos);

    hax_log(HAX_LOGI, "Host cpu init %d logical cpu(s) into %d group(s)\n",
            cpu_online_map.cpu_num, cpu_online_map.group_num);

    return 0;

fail_nomem:
    if (cpu_online_map.cpu_map)
        hax_vfree(cpu_online_map.cpu_map, size_group);
    if (cpu_online_map.cpu_pos)
        hax_vfree(cpu_online_map.cpu_pos, size_pos);
    if (omap.cpu_map)
        hax_vfree(omap.cpu_map, size_group);
    if (omap.cpu_pos)
        hax_vfree(omap.cpu_pos, size_pos);
    return -ENOMEM;
}

extern "C" void mp_rendezvous_no_intrs(void (*action_func)(void *), void *arg);

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
