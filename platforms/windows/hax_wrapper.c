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

uint32_t hax_cpu_id(void)
{
    PROCESSOR_NUMBER ProcNumber = {0};
    return (uint32_t)KeGetCurrentProcessorNumberEx(&ProcNumber);
}

int cpu_info_init(void)
{
    uint32_t size_group, size_pos, count, group, bit;

    memset(&cpu_online_map, 0, sizeof(cpu_online_map));

    cpu_online_map.group_num = KeQueryActiveGroupCount();
    cpu_online_map.cpu_num = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    if (cpu_online_map.group_num > HAX_MAX_CPU_GROUP ||
        cpu_online_map.cpu_num > HAX_MAX_CPUS) {
        hax_log(HAX_LOGE, "Too many cpus %d-%d in system\n",
                cpu_online_map.cpu_num, cpu_online_map.group_num);
        return -1;
    }

    size_group = cpu_online_map.group_num * sizeof(*cpu_online_map.cpu_map);
    cpu_online_map.cpu_map = hax_vmalloc(size_group, 0);
    if (!cpu_online_map.cpu_map) {
        hax_log(HAX_LOGE, "Couldn't allocate cpu_map for cpu_online_map\n");
        return -1;
    }

    size_pos = cpu_online_map.cpu_num * sizeof(*cpu_online_map.cpu_pos);
    cpu_online_map.cpu_pos = hax_vmalloc(size_pos, 0);
    if (!cpu_online_map.cpu_pos) {
        hax_log(HAX_LOGE, "Couldn't allocate cpu_pos for cpu_online_map\n");
        hax_vfree(cpu_online_map.cpu_map, size_group);
        return -1;
    }

    count = 0;
    for (group = 0; group < cpu_online_map.group_num; group++) {
        cpu_online_map.cpu_map[group].map = (hax_cpumask_t)KeQueryGroupAffinity(
                group);
        cpu_online_map.cpu_map[group].id = group;
        cpu_online_map.cpu_map[group].num = 0;
        for (bit = 0; bit < HAX_MAX_CPU_PER_GROUP; bit++) {
            if (cpu_online_map.cpu_map[group].map & ((hax_cpumask_t)1 << bit)) {
                ++cpu_online_map.cpu_map[group].num;
                cpu_online_map.cpu_pos[count].group = group;
                cpu_online_map.cpu_pos[count].bit = bit;
                ++count;
            }
        }
    }

    if (count != cpu_online_map.cpu_num) {
        hax_log(HAX_LOGE, "Active logical processor count(%d)-affinity(%d) "
                "doesn't match\n", cpu_online_map.cpu_num, count);
        hax_vfree(cpu_online_map.cpu_map, size_group);
        hax_vfree(cpu_online_map.cpu_pos, size_pos);
        return -1;
    }

    hax_log(HAX_LOGI, "Host cpu init %d logical cpu(s) into %d group(s)\n",
            cpu_online_map.cpu_num, cpu_online_map.group_num);

    return 0;
}

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
    struct smp_call_parameter *p = (struct smp_call_parameter *)SystemArgument2;
    void (*action)(void *param) = p->func;
    hax_cpumap_t *done;
    uint32_t self, group, bit;

    action(p->param);

    // We only use hax_cpumap_t.hax_cpu_pos_t to mark done or not
    done = (hax_cpumap_t*)SystemArgument1;
    self = hax_cpu_id();
    group = self / HAX_MAX_CPU_PER_GROUP;
    bit = self % HAX_MAX_CPU_PER_GROUP;
    done->cpu_pos[self].group = group;
    done->cpu_pos[self].bit = bit;
}

/* IPI function is not exported to in XP, we use DPC to trigger the smp
 * call function. However, as the DPC is not happen immediately, not
 * sure how to handle such situation. Currently simply delay
 * The hax_smp_call_function has to be synced, since we use global dpc, however,
 * we can't use spinlock here since spinlock will increase IRQL to DISPATCH
 * and cause potential deadloop. Another choice is to allocate the DPC in the
 * hax_smp_call_function instead of globla dpc.
 */
int hax_smp_call_function(hax_cpumap_t *cpus, void (*scfunc)(void *), void * param)
{
    uint32_t cpu_id, self, group, bit, size_pos;
    BOOLEAN result;
    struct _KDPC *cur_dpc;
    hax_cpumap_t done = {0};
    struct smp_call_parameter *sp;
    KIRQL old_irql;
    LARGE_INTEGER delay;
    NTSTATUS event_result;
    int err = 0;

    self = hax_cpu_id();
    group = self / HAX_MAX_CPU_PER_GROUP;
    bit = self % HAX_MAX_CPU_PER_GROUP;

    size_pos = cpu_online_map.cpu_num * sizeof(*cpu_online_map.cpu_pos);
    done.cpu_pos = hax_vmalloc(size_pos, 0);
    if (!done.cpu_pos) {
        hax_log(HAX_LOGE, "Couldn't allocate done to check SMP DPC done\n");
        return -1;
    }
    memset(done.cpu_pos, 0xFF, size_pos);

    event_result = KeWaitForSingleObject(&dpc_event, Executive, KernelMode,
                                         FALSE, NULL);
    if (event_result!= STATUS_SUCCESS) {
        hax_log(HAX_LOGE, "Failed to get the smp_call event object\n");
        hax_vfree(done.cpu_pos, size_pos);
        return -1;
    }

    if (cpu_is_online(cpus, self)){
        KeRaiseIrql(DISPATCH_LEVEL, &old_irql);
        (scfunc)(param);
        done.cpu_pos[self].group = group;
        done.cpu_pos[self].bit = bit;
        KeLowerIrql(old_irql);
    }

    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        if (!cpu_is_online(&cpu_online_map, cpu_id) || (cpu_id == self))
            continue;
        sp = smp_cp + cpu_id;
        sp->func = scfunc;
        sp->param = param;
        cur_dpc = smpc_dpcs + cpu_id;
        result = KeInsertQueueDpc(cur_dpc, &done, sp);
        if (result != TRUE)
            hax_log(HAX_LOGE, "Failed to insert queue on CPU %x\n", cpu_id);
    }

    /* Delay 100 ms */
    delay.QuadPart =  100 * -1 *((LONGLONG) 1 * 10 * 1000);
    if (KeDelayExecutionThread( KernelMode, TRUE, &delay ) != STATUS_SUCCESS)
        hax_log(HAX_LOGE, "Delay execution is not success\n");

    if(!memcmp(done.cpu_pos, cpu_online_map.cpu_pos, size_pos)) {
        err = -1;
        hax_log(HAX_LOGE, "sm call function is not called in all required CPUs\n");
    }

    KeSetEvent(&dpc_event, 0, FALSE);

    hax_vfree(done.cpu_pos, size_pos);

    return err;
}

int
smpc_dpc_init(void)
{
    struct _KDPC *cur_dpc;
    uint32_t cpu_id;

    smpc_dpcs = hax_vmalloc(sizeof(KDPC) * cpu_online_map.cpu_num, 0);
    if (!smpc_dpcs)
        return -ENOMEM;
    smp_cp = hax_vmalloc(sizeof(struct smp_call_parameter) * cpu_online_map.cpu_num, 0);
    if (!smp_cp) {
        hax_vfree(smpc_dpcs, sizeof(KDPC) * cpu_online_map.cpu_num);
        return -ENOMEM;
    }
    cur_dpc = smpc_dpcs;
    for (cpu_id = 0; cpu_id < cpu_online_map.cpu_num; cpu_id++) {
        KeInitializeDpc(cur_dpc, smp_cfunction_dpc, NULL);
        KeSetTargetProcessorDpc(cur_dpc, cpu_id);
        /* Set the DPC as high important, so that we loop too long */
        KeSetImportanceDpc(cur_dpc, HighImportance);
        cur_dpc++;
    }
    KeInitializeEvent(&dpc_event, SynchronizationEvent, TRUE);
    return 0;
}

int smpc_dpc_exit(void)
{
    hax_vfree(smpc_dpcs, sizeof(KDPC) * cpu_online_map.cpu_num);
    hax_vfree(smp_cp, sizeof(KDPC) * cpu_online_map.cpu_num);
    return 0;
}
#else
// A driver calls KeIpiGenericCall to interrupt every processor and raises
// the IRQL to IPI_LEVEL, which is greater than DIRQL for every device.
int hax_smp_call_function(hax_cpumap_t *cpus, void (*scfunc)(void *), void * param)
{
    struct smp_call_parameter sp;
    sp.func = scfunc;
    sp.param = param;
    sp.cpus = cpus;
    KeIpiGenericCall((PKIPI_BROADCAST_WORKER)smp_cfunction, (ULONG_PTR)&sp);
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

static const int kLogLevel[] = {
    DPFLTR_ERROR_LEVEL,
    DPFLTR_TRACE_LEVEL,     // HAX_LOGD
    DPFLTR_INFO_LEVEL,      // HAX_LOGI
    DPFLTR_WARNING_LEVEL,   // HAX_LOGW
    DPFLTR_ERROR_LEVEL,     // HAX_LOGE
    DPFLTR_ERROR_LEVEL      // HAX_LOGPANIC
};

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
    va_list arglist;
    va_start(arglist, fmt);

    if (level >= HAX_LOG_DEFAULT)
        vDbgPrintExWithPrefix(kLogPrefix[level], DPFLTR_IHVDRIVER_ID,
                              kLogLevel[level], fmt, arglist);

    va_end(arglist);
}

void hax_panic(const char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);
    hax_log(HAX_LOGPANIC, fmt, arglist);
    va_end(arglist);
}
