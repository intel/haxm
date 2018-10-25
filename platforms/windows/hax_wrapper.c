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

#include "hax_win.h"
#include "../../core/include/ia32.h"

int default_hax_log_level = 3;
int max_cpus;
cpumap_t cpu_online_map;

int hax_log_level(int level, const char *fmt,  ...)
{
    va_list arglist;
    va_start(arglist, fmt);

    if (level >=  default_hax_log_level)
        vDbgPrintExWithPrefix("haxm: ", -1, 0, fmt, arglist);
    return 0;
}

uint32_t hax_cpuid()
{
    return KeGetCurrentProcessorNumber();
}

struct smp_call_parameter
{
    void (*func)(void *);
    void *param;
    /* Not used in DPC model*/
    cpumap_t *cpus;
};

#ifdef SMPC_DPCS
KDEFERRED_ROUTINE smp_cfunction_dpc;
static struct _KDPC *smpc_dpcs;
struct smp_call_parameter *smp_cp;
KEVENT dpc_event;

void smp_cfunction_dpc(
        __in struct _KDPC  *Dpc,
        __in_opt PVOID  DeferredContext,
        __in_opt PVOID  SystemArgument1,
        __in_opt PVOID  SystemArgument2)
{
    cpumap_t *done;
    void (*action)(void *parap);
    struct smp_call_parameter *p;

    p = (struct smp_call_parameter *)SystemArgument2;
    done = (cpumap_t*)SystemArgument1;
    action = p->func;
    action(p->param);
    hax_test_and_set_bit(hax_cpuid(), (uint64_t*)done);
}

/* IPI function is not exported to in XP, we use DPC to trigger the smp
 * call function. However, as the DPC is not happen immediately, not
 * sure how to handle such situation. Currently simply delay
 * The smp_call_function has to be synced, since we use global dpc, however,
 * we can't use spinlock here since spinlock will increase IRQL to DISPATCH
 * and cause potential deadloop. Another choice is to allocate the DPC in the
 * smp_call_function instead of globla dpc.
 */
int smp_call_function(cpumap_t *cpus, void (*scfunc)(void *), void * param)
{
    int i, self;
    BOOLEAN result;
    struct _KDPC *cur_dpc;
    cpumap_t done;
    struct smp_call_parameter *sp;
    KIRQL old_irql;
    LARGE_INTEGER delay;
    NTSTATUS event_result;

    self = hax_cpuid();

    done = 0;

    event_result = KeWaitForSingleObject(&dpc_event, Executive, KernelMode,
                                         FALSE, NULL);
    if (event_result!= STATUS_SUCCESS) {
        hax_error("Failed to get the smp_call event object\n");
        return -1;
    }

    if (((mword)1 << self) & *cpus) {
        KeRaiseIrql(DISPATCH_LEVEL, &old_irql);
        (scfunc)(param);
        done |= ((mword)1 << self);
        KeLowerIrql(old_irql);
    }

    for (i = 0; i < max_cpus; i++) {
        if (!cpu_is_online(i) || (i == self))
            continue;
        sp = smp_cp + i;
        sp->func = scfunc;
        sp->param = param;
        cur_dpc = smpc_dpcs + i;
        result = KeInsertQueueDpc(cur_dpc, &done, sp);
        if (result != TRUE)
            hax_error("Failed to insert queue on CPU %x\n", i);
    }

    /* Delay 100 ms */
    delay.QuadPart =  100 * -1 *((LONGLONG) 1 * 10 * 1000);
    if (KeDelayExecutionThread( KernelMode, TRUE, &delay ) != STATUS_SUCCESS)
        hax_error("Delay execution is not success\n");

    if (done != *cpus)
        hax_error("sm call function is not called in all required CPUs\n");

    KeSetEvent(&dpc_event, 0, FALSE);

    return (done != *cpus) ? -1 :0;
}

int
smpc_dpc_init(void)
{
    struct _KDPC *cur_dpc;
    int i;

    smpc_dpcs = hax_vmalloc(sizeof(KDPC) * max_cpus, 0);
    if (!smpc_dpcs)
        return -ENOMEM;
    smp_cp = hax_vmalloc(sizeof(struct smp_call_parameter) * max_cpus, 0);
    if (!smp_cp) {
        hax_vfree(smpc_dpcs, sizeof(KDPC) * max_cpus);
        return -ENOMEM;
    }
    cur_dpc = smpc_dpcs;
    for (i = 0; i < max_cpus; i++) {
        KeInitializeDpc(cur_dpc, smp_cfunction_dpc, NULL);
        KeSetTargetProcessorDpc(cur_dpc, i);
        /* Set the DPC as high important, so that we loop too long */
        KeSetImportanceDpc(cur_dpc, HighImportance);
        cur_dpc++;
    }
    KeInitializeEvent(&dpc_event, SynchronizationEvent, TRUE);
    return 0;
}

int smpc_dpc_exit(void)
{
    hax_vfree(smpc_dpcs, sizeof(KDPC) * max_cpus);
    hax_vfree(smp_cp, sizeof(KDPC) * max_cpus);
    return 0;
}
#else
/* This is the only function that in DIRQL */
static ULONG_PTR smp_cfunction(ULONG_PTR param)
{
    int cpu_id;
    void (*action)(void *parap) ;
    cpumap_t *hax_cpus;
    struct smp_call_parameter *p;

    p = (struct smp_call_parameter *)param;
    cpu_id = hax_cpuid();
    action = p->func;
    hax_cpus = p->cpus;
    if (*hax_cpus & ((mword)1 << cpu_id))
        action(p->param);
    return (ULONG_PTR)NULL;
}
int smp_call_function(cpumap_t *cpus, void (*scfunc)(void *), void * param)
{
    struct smp_call_parameter sp;
    sp.func = scfunc;
    sp.param = param;
    sp.cpus = cpus;
    KeIpiGenericCall(smp_cfunction, (ULONG_PTR)&sp);
    return 0;
}
#endif

/* XXX */
int proc_event_pending(struct vcpu_t *vcpu)
{
    return vcpu_event_pending(vcpu);
}

void hax_disable_preemption(preempt_flag *flags)
{
    KIRQL cur;
    cur = KeGetCurrentIrql();
    if (cur >= DISPATCH_LEVEL) {
        *flags = cur;
        return;
    }
    KeRaiseIrql(DISPATCH_LEVEL, flags);
}

void hax_enable_preemption(preempt_flag *flags)
{
    if (*flags >= DISPATCH_LEVEL)
        return;
    KeLowerIrql(*flags);
}

void hax_enable_irq(void)
{
    asm_enable_irq();
}

void hax_disable_irq(void)
{
    asm_disable_irq();
}

void hax_error(char *fmt, ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    if (HAX_LOGE >= default_hax_log_level)
        vDbgPrintExWithPrefix("haxm_error:", -1, 0, fmt, arglist);
}

void hax_warning(char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);

    if (HAX_LOGW >= default_hax_log_level)
        vDbgPrintExWithPrefix("haxm_warning:", -1, 0, fmt, arglist);
}

void hax_info(char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);

    if (HAX_LOGI >= default_hax_log_level)
        vDbgPrintExWithPrefix("haxm_info:", -1, 0, fmt, arglist);
}

void hax_debug(char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);

    if (HAX_LOGD >= default_hax_log_level)
        vDbgPrintExWithPrefix("haxm_debug:", -1, 0, fmt, arglist);
}

void hax_panic_vcpu(struct vcpu_t *v, char *fmt, ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    vDbgPrintExWithPrefix("haxm_panic:", -1, 0, fmt, arglist);
    vcpu_set_panic(v);
}
