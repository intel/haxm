/*
 * Copyright (c) 2009 Intel Corporation
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

#include "../include/hax.h"
#include "include/ia32_defs.h"
#include "include/cpu.h"
#include "include/cpuid.h"
#include "include/vcpu.h"
#include "include/debug.h"
#include "include/dump.h"
#include "include/name.h"
#include "include/vtlb.h"
#include "include/intr.h"
#include "include/ept.h"

static void cpu_vmentry_failed(struct vcpu_t *vcpu, vmx_result_t result);
static int cpu_vmexit_handler(struct vcpu_t *vcpu, exit_reason_t exit_reason,
                              struct hax_tunnel *htun);

static int cpu_emt64_enable(void)
{
    uint32_t efer;

    efer = ia32_rdmsr(IA32_EFER);
    return efer & 0x400;
}

static int cpu_nx_enable(void)
{
    uint32_t efer;

    efer = ia32_rdmsr(IA32_EFER);
    return efer & 0x800;
}

bool cpu_has_feature(uint32_t feature)
{
    return cpuid_host_has_feature(feature);
}

void cpu_init_vmx(void *arg)
{
    struct info_t vmx_info;
    struct per_cpu_data *cpu_data;
    uint32_t fc_msr;
    vmcs_t *vmxon;
    int nx_enable = 0, vt_enable = 0;

    cpu_data = current_cpu_data();

    hax_log(HAX_LOGD, "[#%d] cpu_init_vmx\n", cpu_data->cpu_id);

    cpu_data->cpu_features |= HAX_CPUF_VALID;
    if (!cpu_has_feature(X86_FEATURE_VMX))
        return;
    else
        cpu_data->cpu_features |= HAX_CPUF_SUPPORT_VT;

    if (!cpu_has_feature(X86_FEATURE_NX))
        return;
    else
        cpu_data->cpu_features |= HAX_CPUF_SUPPORT_NX;

    if (cpu_has_feature(X86_FEATURE_EM64T))
        cpu_data->cpu_features |= HAX_CPUF_SUPPORT_EM64T;

    nx_enable = cpu_nx_enable();
    if (nx_enable)
        cpu_data->cpu_features |= HAX_CPUF_ENABLE_NX;

    fc_msr = ia32_rdmsr(IA32_FEATURE_CONTROL);
    if ((fc_msr & FC_VMXON_OUTSMX) || !(fc_msr & FC_LOCKED))
        vt_enable = 1;
    if (vt_enable)
        cpu_data->cpu_features |= HAX_CPUF_ENABLE_VT;
    hax_log(HAX_LOGI, "fc_msr %x\n", fc_msr);
    hax_log(HAX_LOGI, "vt_enable %d\n", vt_enable);
    hax_log(HAX_LOGI, "nx_enable %d\n", nx_enable);

    if (!nx_enable || !vt_enable)
        return;

    /*
     * EM64T disabled is ok for windows, but should cause failure in Mac
     * Let Mac part roll back the whole staff
     */
    if (cpu_emt64_enable())
        cpu_data->cpu_features |= HAX_CPUF_ENABLE_EM64T;

    /* Enable FEATURE CONTROL MSR */
    if (!(fc_msr & FC_LOCKED))
        ia32_wrmsr(IA32_FEATURE_CONTROL,
                   fc_msr | FC_LOCKED | FC_VMXON_OUTSMX);

    /* get VMX capabilities */
    vmx_read_info(&vmx_info);
#if 0
    //hax_log(HAX_LOGI, "-----------cpu %d---------------\n", cpu_data->cpu_id);

    if ((cpu_data->cpu_id == 0 ||
         memcmp(&vmx_info, &hax_cpu_data[0]->vmx_info,
                sizeof(vmx_info)) != 0)) {
        dump_vmx_info(&vmx_info);
    }
#endif

    if (vmx_info._vmcs_region_length > HAX_PAGE_SIZE)
        hax_log(HAX_LOGI, "VMCS of %d bytes not supported by this Hypervisor. "
                "Max supported %u bytes\n",
                vmx_info._vmcs_region_length, (uint32_t)HAX_PAGE_SIZE);
    vmxon = (vmcs_t *)hax_page_va(cpu_data->vmxon_page);
    vmxon->_revision_id = vmx_info._vmcs_revision_id;

    //hax_log(HAX_LOGI, "enabled VMX mode (vmxon = %p)\n",
    //        hax_page_va(cpu_data->vmxon_page));

    vmx_read_info(&cpu_data->vmx_info);

    cpu_data->cpu_features |= HAX_CPUF_INITIALIZED;
}

void cpu_exit_vmx(void *arg)
{
    hax_log(HAX_LOGD, "[#%d] cpu_exit_vmx\n", current_cpu_data()->cpu_id);
}

/*
 * Retrieves information about the performance monitoring capabilities of the
 * current host logical processor.
 * |arg| is unused.
 */
void cpu_pmu_init(void *arg)
{
    struct cpu_pmu_info *pmu_info = &current_cpu_data()->pmu_info;
    cpuid_args_t cpuid_args;

    hax_log(HAX_LOGD, "[#%d] cpu_pmu_init\n", current_cpu_data()->cpu_id);

    memset(pmu_info, 0, sizeof(struct cpu_pmu_info));

    // Call CPUID with EAX = 0
    cpuid_query_leaf(&cpuid_args, 0x00);
    if (cpuid_args.eax < 0xa) {
        // Logical processor does not support APM
        return;
    }

    // Call CPUID with EAX = 0xa
    cpuid_query_leaf(&cpuid_args, 0xa);
    pmu_info->cpuid_eax = cpuid_args.eax;
    pmu_info->cpuid_ebx = cpuid_args.ebx;
    pmu_info->cpuid_edx = cpuid_args.edx;
}

static void vmread_cr(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    mword cr4, cr4_mask;

    // Update only the bits the guest is allowed to change
    // This must use the actual cr0 mask, not _cr0_mask.
    mword cr0 = vmread(vcpu, GUEST_CR0);
    mword cr0_mask = vmread(vcpu, VMX_CR0_MASK); // should cache this
    hax_log(HAX_LOGD, "vmread_cr cr0 %lx, cr0_mask %lx, state->_cr0 %llx\n",
            cr0, cr0_mask, state->_cr0);
    state->_cr0 = (cr0 & ~cr0_mask) | (state->_cr0 & cr0_mask);
    hax_log(HAX_LOGD, "vmread_cr, state->_cr0 %llx\n", state->_cr0);

    // update CR3 only if guest is allowed to change it
    if (!(vmx(vcpu, pcpu_ctls) & CR3_LOAD_EXITING))
        state->_cr3 = vmread(vcpu, GUEST_CR3);

    cr4 = vmread(vcpu, GUEST_CR4);
    cr4_mask = vmread(vcpu, VMX_CR4_MASK); // should cache this
    state->_cr4 = (cr4 & ~cr4_mask) | (state->_cr4 & cr4_mask);
}

vmx_result_t cpu_vmx_vmptrld(struct per_cpu_data *cpu_data, hax_paddr_t vmcs,
                             struct vcpu_t *vcpu)
{
    vmx_result_t r = asm_vmptrld(&vmcs);
    return r;
}

bool vcpu_is_panic(struct vcpu_t *vcpu)
{
    struct hax_tunnel *htun = vcpu->tunnel;
    if (vcpu->panicked) {
        hax_log(HAX_LOGE, "vcpu has panicked, id:%d\n", vcpu->vcpu_id);
        hax_panic_log(vcpu);
        htun->_exit_status = HAX_EXIT_STATECHANGE;
        return 1;
    }
    return 0;
}

/*
 * Return:
 * 0 if need handling from qemu
 * 1 if return to guest
 * <0 if something wrong
 */
static int cpu_vmexit_handler(struct vcpu_t *vcpu, exit_reason_t exit_reason,
                              struct hax_tunnel *htun)
{
    int ret;

    ret = vcpu_vmexit_handler(vcpu, exit_reason, htun);

    if (vcpu_is_panic(vcpu)) {
        return HAX_EXIT;
    }

    if (ret == HAX_RESUME && !vcpu->event_injected && !vcpu->nr_pending_intrs &&
        htun->request_interrupt_window) {

        htun->_exit_status = HAX_EXIT_INTERRUPT;
        ret = HAX_EXIT;
    }

    /* Return for signal handling
     * We assume the signal handling will not cause vcpus state change
     * Otherwise we need consider situation that vcpu state impact, for example
     * if PG fault pending to guest
     */

    if ((ret == HAX_RESUME) && proc_event_pending(vcpu)) {
        htun->_exit_status = HAX_EXIT_INTERRUPT;
        ret = 0;
    }
    return ret;
}

#ifdef CONFIG_DARWIN
__attribute__ ((__noinline__))
#endif
vmx_result_t cpu_vmx_run(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    vmx_result_t result = 0;
    mword host_rip;

    /* prepare the RIP */
    hax_log(HAX_LOGD, "vm entry!\n");
    vcpu_save_host_state(vcpu);
    hax_disable_irq();

    /*
     * put the vmwrite before is_running, so that the vcpu->cpu_id is set
     * when we check vcpu->is_running in vcpu_pause
     */
    host_rip = vmx_get_rip();
    vmwrite(vcpu, HOST_RIP, (mword)host_rip);
    vcpu->is_running = 1;
#ifdef  DEBUG_HOST_STATE
    vcpu_get_host_state(vcpu, 1);
#endif
    /* Must ensure the IRQ is disabled before setting CR2 */
    set_cr2(vcpu->state->_cr2);

    vcpu_load_guest_state(vcpu);

    result = asm_vmxrun(vcpu->state, vcpu->launched);

    vcpu->is_running = 0;
    vcpu_save_guest_state(vcpu);
    vcpu_load_host_state(vcpu);

#ifdef  DEBUG_HOST_STATE
    vcpu_get_host_state(vcpu, 0);
    compare_host_state(vcpu);
#endif

    if (result != VMX_SUCCEED) {
        cpu_vmentry_failed(vcpu, result);
        htun->_exit_reason = 0;
        htun->_exit_status = HAX_EXIT_UNKNOWN;
    }
    return result;
}

void vcpu_handle_vmcs_pending(struct vcpu_t *vcpu)
{
    if (!vcpu || !vcpu->vmcs_pending)
        return;
    if (vcpu->vmcs_pending_entry_error_code) {
        vmwrite(vcpu, VMX_ENTRY_EXCEPTION_ERROR_CODE,
                vmx(vcpu, entry_exception_error_code));
        vcpu->vmcs_pending_entry_error_code = 0;
    }

    if (vcpu->vmcs_pending_entry_instr_length) {
        vmwrite(vcpu, VMX_ENTRY_INSTRUCTION_LENGTH,
                vmx(vcpu, entry_instr_length));
        vcpu->vmcs_pending_entry_instr_length = 0;
    }

    if (vcpu->vmcs_pending_entry_intr_info) {
        vmwrite(vcpu, VMX_ENTRY_INTERRUPT_INFO,
                vmx(vcpu, entry_intr_info).raw);
        vcpu->vmcs_pending_entry_intr_info = 0;
    }

    if (vcpu->vmcs_pending_guest_cr3) {
        vmwrite(vcpu, GUEST_CR3, vtlb_get_cr3(vcpu));
        vcpu->vmcs_pending_guest_cr3 = 0;
    }
    vcpu->vmcs_pending = 0;
    return;
}

/* Return the value same as ioctl value */
int cpu_vmx_execute(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    vmx_result_t res = 0;
    int ret;
    preempt_flag flags;
    struct vcpu_state_t *state = vcpu->state;
    uint32_t vmcs_err = 0;

    while (1) {
        exit_reason_t exit_reason;

        if (vcpu->paused) {
            htun->_exit_status = HAX_EXIT_PAUSED;
            return 0;
        }
        if (vcpu_is_panic(vcpu))
            return 0;

        if ((vmcs_err = load_vmcs(vcpu, &flags))) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "load_vmcs fail: %x\n", vmcs_err);
            hax_panic_log(vcpu);
            return 0;
        }
        vcpu_handle_vmcs_pending(vcpu);
        vcpu_inject_intr(vcpu, htun);

        /* sometimes, the code segment type from qemu can be 10 (code segment),
         * this will cause invalid guest state, since 11 (accessed code segment),
         * not 10 is required by vmx hardware. Note: 11 is one of the allowed
         * values by vmx hardware.
         */
        {
            uint32_t temp= vmread(vcpu, GUEST_CS_AR);

            if( (temp & 0xf) == 0xa) {
                temp = temp +1;
                vmwrite(vcpu, GUEST_CS_AR, temp);
            }
        }
        /* sometimes, the TSS segment type from qemu is not right.
         * let's hard-code it for now
         */
        {
            uint32_t temp = vmread(vcpu, GUEST_TR_AR);

            temp = (temp & ~0xf) | 0xb;
            vmwrite(vcpu, GUEST_TR_AR, temp);
        }

        res = cpu_vmx_run(vcpu, htun);
        if (res) {
            hax_log(HAX_LOGE, "cpu_vmx_run error, code:%x\n", res);
            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "put_vmcs fail: %x\n", vmcs_err);
                hax_panic_log(vcpu);
            }
            return -EINVAL;
        }

        exit_reason.raw = vmread(vcpu, VM_EXIT_INFO_REASON);
        hax_log(HAX_LOGD, "....exit_reason.raw %x, vcpu %d, cpu %d\n",
                exit_reason.raw, vcpu->cpu_id, hax_cpu_id());

        /* XXX Currently we take active save/restore for MSR and FPU, the main
         * reason is, we have no schedule hook to get notified of preemption
         * This should be changed later after get better idea
         */
        vcpu->state->_rip = vmread(vcpu, GUEST_RIP);

        hax_handle_idt_vectoring(vcpu);

        vmx(vcpu, exit_qualification).raw = vmread(
                vcpu, VM_EXIT_INFO_QUALIFICATION);
        vmx(vcpu, exit_intr_info).raw = vmread(
                vcpu, VM_EXIT_INFO_INTERRUPT_INFO);
        vmx(vcpu, exit_exception_error_code) = vmread(
                vcpu, VM_EXIT_INFO_EXCEPTION_ERROR_CODE);
        vmx(vcpu, exit_idt_vectoring) = vmread(
                vcpu, VM_EXIT_INFO_IDT_VECTORING);
        vmx(vcpu, exit_instr_length) = vmread(
                vcpu, VM_EXIT_INFO_INSTRUCTION_LENGTH);
        vmx(vcpu, exit_gpa) = vmread(
                vcpu, VM_EXIT_INFO_GUEST_PHYSICAL_ADDRESS);
        vmx(vcpu, interruptibility_state).raw = vmread(
                vcpu, GUEST_INTERRUPTIBILITY);

        state->_rflags = vmread(vcpu, GUEST_RFLAGS);
        state->_rsp = vmread(vcpu, GUEST_RSP);
        VMREAD_SEG(vcpu, CS, state->_cs);
        VMREAD_SEG(vcpu, DS, state->_ds);
        VMREAD_SEG(vcpu, ES, state->_es);
        vmread_cr(vcpu);

        if (vcpu->nr_pending_intrs > 0 || hax_intr_is_blocked(vcpu))
            htun->ready_for_interrupt_injection = 0;
        else
            htun->ready_for_interrupt_injection = 1;

        vcpu->cur_state = GS_STALE;
        vmcs_err = put_vmcs(vcpu, &flags);
        if (vmcs_err) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "put_vmcs() fail before vmexit. %x\n",
                    vmcs_err);
            hax_panic_log(vcpu);
        }
        hax_enable_irq();

        ret = cpu_vmexit_handler(vcpu, exit_reason, htun);
        if (ret <= 0)
            return ret;
    }
}

uint8_t is_vmcs_loaded(struct vcpu_t *vcpu)
{
    return (vcpu && vcpu->is_vmcs_loaded);
}

int debug_vmcs_count = 0;

void restore_host_cr4_vmxe(struct per_cpu_data *cpu_data);

uint32_t log_host_cr4_vmxe = 0;
uint64_t log_host_cr4 = 0;
vmx_result_t log_vmxon_res = 0;
uint64_t log_vmxon_addr = 0;
uint32_t log_vmxon_err_type1 = 0;
uint32_t log_vmxon_err_type2 = 0;
uint32_t log_vmxon_err_type3 = 0;
uint32_t log_vmclear_err = 0;
uint32_t log_vmptrld_err = 0;
uint32_t log_vmxoff_no = 0;
vmx_result_t log_vmxoff_res = 0;

void hax_clear_panic_log(struct vcpu_t *vcpu)
{
    log_host_cr4_vmxe = 0;
    log_host_cr4 = 0;
    log_vmxon_res = 0;
    log_vmxon_addr = 0;
    log_vmxon_err_type1 = 0;
    log_vmxon_err_type2 = 0;
    log_vmxon_err_type3 = 0;
    log_vmclear_err = 0;
    log_vmptrld_err = 0;
    log_vmxoff_no = 0;
    log_vmxoff_res = 0;
}

void hax_panic_log(struct vcpu_t *vcpu)
{
    if (!vcpu)
        return;
    hax_log(HAX_LOGE, "log_host_cr4_vmxe: %x\n", log_host_cr4_vmxe);
    hax_log(HAX_LOGE, "log_host_cr4 %llx\n", log_host_cr4);
    hax_log(HAX_LOGE, "log_vmxon_res %x\n", log_vmxon_res);
    hax_log(HAX_LOGE, "log_vmxon_addr %llx\n", log_vmxon_addr);
    hax_log(HAX_LOGE, "log_vmxon_err_type1 %x\n", log_vmxon_err_type1);
    hax_log(HAX_LOGE, "log_vmxon_err_type2 %x\n", log_vmxon_err_type2);
    hax_log(HAX_LOGE, "log_vmxon_err_type3 %x\n", log_vmxon_err_type3);
    hax_log(HAX_LOGE, "log_vmclear_err %x\n", log_vmclear_err);
    hax_log(HAX_LOGE, "log_vmptrld_err %x\n", log_vmptrld_err);
    hax_log(HAX_LOGE, "log_vmoff_no %x\n", log_vmxoff_no);
    hax_log(HAX_LOGE, "log_vmxoff_res %x\n", log_vmxoff_res);
}

uint32_t load_vmcs(struct vcpu_t *vcpu, preempt_flag *flags)
{
    struct per_cpu_data *cpu_data;
    hax_paddr_t vmcs_phy;
    hax_paddr_t curr_vmcs = VMCS_NONE;

    hax_disable_preemption(flags);

    /* when wake up from sleep, we need the barrier, as vm operation
     * are not serialized instructions.
     */
    hax_smp_mb();

    cpu_data = current_cpu_data();

    if (vcpu && is_vmcs_loaded(vcpu)) {
        cpu_data->nested++;
        return 0;
    }

    if (cpu_vmxroot_enter() != VMX_SUCCEED) {
        hax_enable_preemption(flags);
        return VMXON_FAIL;
    }

    if (vcpu)
        ((vmcs_t*)(hax_page_va(vcpu->vmcs_page)))->_revision_id =
                cpu_data->vmx_info._vmcs_revision_id;

    if (vcpu)
        vmcs_phy = vcpu_vmcs_pa(vcpu);
    else
        vmcs_phy = hax_page_pa(cpu_data->vmcs_page);


    if (asm_vmptrld(&vmcs_phy) != VMX_SUCCEED) {
        hax_log(HAX_LOGE, "vmptrld failed (%08llx)\n", vmcs_phy);
        cpu_vmxroot_leave();
        log_vmxon_err_type3 = 1;
        hax_enable_preemption(flags);
        return VMPTRLD_FAIL;
    }

    if (vcpu) {
        vcpu->is_vmcs_loaded = 1;
        cpu_data->current_vcpu = vcpu;
        vcpu->prev_cpu_id = vcpu->cpu_id;
        vcpu->cpu_id = hax_cpu_id();
    }

    cpu_data->other_vmcs = curr_vmcs;
    return VMXON_SUCCESS;
}

void restore_host_cr4_vmxe(struct per_cpu_data *cpu_data)
{
    if (cpu_data->host_cr4_vmxe) {
        if (cpu_data->vmm_flag & VMXON_HAX) {
            // TODO: Need to understand why this happens (on both Windows and
            // macOS)
            hax_log(HAX_LOGD, "VMM flag (VMON_HAX) is not clear!\n");
        }
        set_cr4(get_cr4() | CR4_VMXE);
    } else {
        set_cr4(get_cr4() & (~CR4_VMXE));
    }
}

uint32_t put_vmcs(struct vcpu_t *vcpu, preempt_flag *flags)
{
    struct per_cpu_data *cpu_data = current_cpu_data();
    hax_paddr_t vmcs_phy;
    vmx_result_t vmxoff_res = 0;
    if (vcpu && cpu_data->nested > 0) {
        cpu_data->nested--;
        goto out;
    }

    if (vcpu)
        vmcs_phy = vcpu_vmcs_pa(vcpu);
    else
        vmcs_phy = hax_page_pa(cpu_data->vmcs_page);

    if (asm_vmclear(&vmcs_phy) != VMX_SUCCEED) {
        hax_log(HAX_LOGE, "vmclear failed (%llx)\n", vmcs_phy);
        log_vmclear_err = 1;
    }

    cpu_data->current_vcpu = NULL;

    vmxoff_res = cpu_vmxroot_leave();
    cpu_data->other_vmcs = VMCS_NONE;
    if (vcpu && vcpu->is_vmcs_loaded)
        vcpu->is_vmcs_loaded = 0;
out:
    hax_enable_preemption(flags);
    return vmxoff_res;
}

void load_vmcs_common(struct vcpu_t *vcpu)
{
    // Update the cache for the PIN/EXIT ctls
    vmx(vcpu, pin_ctls) = vmx(vcpu, pin_ctls_base) = vmread(
            vcpu, VMX_PIN_CONTROLS);
    vmx(vcpu, pcpu_ctls) = vmx(vcpu, pcpu_ctls_base) = vmread(
            vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS);
    vmx(vcpu, scpu_ctls) = vmx(vcpu, scpu_ctls_base) =
            vmx(vcpu, pcpu_ctls) & SECONDARY_CONTROLS ?
            vmread(vcpu, VMX_SECONDARY_PROCESSOR_CONTROLS) : 0;

    vmx(vcpu, exc_bitmap) = vmx(vcpu, exc_bitmap_base) = vmread(
            vcpu, VMX_EXCEPTION_BITMAP);
    vmx(vcpu, entry_ctls) = vmread(vcpu, VMX_ENTRY_CONTROLS);
    vmx(vcpu, exit_ctls) = vmx(vcpu, exit_ctls_base) = vmread(
            vcpu, VMX_EXIT_CONTROLS);

    if (vmx(vcpu, pcpu_ctls) & IO_BITMAP_ACTIVE) {
        vmwrite(vcpu, VMX_IO_BITMAP_A, hax_page_pa(io_bitmap_page_a));
        vmwrite(vcpu, VMX_IO_BITMAP_B, hax_page_pa(io_bitmap_page_b));
    }

    if (vmx(vcpu, pcpu_ctls) & MSR_BITMAP_ACTIVE)
        vmwrite(vcpu, VMX_MSR_BITMAP, hax_page_pa(msr_bitmap_page));

    if (vmx(vcpu, pcpu_ctls) & USE_TSC_OFFSETTING)
        vmwrite(vcpu, VMX_TSC_OFFSET, vcpu->tsc_offset);

    vmwrite(vcpu, GUEST_ACTIVITY_STATE, vcpu->state->_activity_state);
    vcpu_vmwrite_all(vcpu, 0);
}


static void cpu_vmentry_failed(struct vcpu_t *vcpu, vmx_result_t result)
{
    uint64_t error, reason;

    hax_log(HAX_LOGE, "VM entry failed: RIP=%08lx\n",
            (mword)vmread(vcpu, GUEST_RIP));

    //dump_vmcs();

    reason = vmread(vcpu, VM_EXIT_INFO_REASON);
    if (result == VMX_FAIL_VALID) {
        error = vmread(vcpu, VMX_INSTRUCTION_ERROR_CODE);
        hax_log(HAX_LOGE, "VMfailValid. Prev exit: %llx. Error code: "
                "%llu (%s)\n", reason, error, name_vmx_error(error));
    } else {
        hax_log(HAX_LOGE, "VMfailInvalid. Prev exit: %llx no error code\n",
                reason);
    }
}

vmx_result_t cpu_vmxroot_leave(void)
{
    struct per_cpu_data *cpu_data = current_cpu_data();
    vmx_result_t result = VMX_SUCCEED;

    hax_log(HAX_LOGD, "[#%d] cpu_vmxroot_leave\n", cpu_data->cpu_id);

    if (cpu_data->vmm_flag & VMXON_HAX) {
        result = asm_vmxoff();
        if (result == VMX_SUCCEED) {
            cpu_data->vmm_flag &= ~VMXON_HAX;
            restore_host_cr4_vmxe(cpu_data);
        } else {
            hax_log(HAX_LOGE, "[#%d] VMXOFF Failed..........\n",
                    cpu_data->cpu_id);
        }
    } else {
        log_vmxoff_no = 1;
#ifdef HAX_PLATFORM_DARWIN
        hax_log(HAX_LOGD, "[#%d] Skipping VMXOFF because another VMM "
                "(VirtualBox or macOS Hypervisor Framework) is running\n",
                cpu_data->cpu_id);
#else
        // It should not go here in Win64/win32
        result = VMX_FAIL_VALID;
        hax_log(HAX_LOGE, "[#%d] NO VMXOFF.......\n", cpu_data->cpu_id);
#endif
    }
    cpu_data->vmxoff_res = result;

    return result;
}

vmx_result_t cpu_vmxroot_enter(void)
{
    struct per_cpu_data *cpu_data = current_cpu_data();
    uint64_t fc_msr;
    hax_paddr_t vmxon_addr;
    vmx_result_t result = VMX_SUCCEED;

    hax_log(HAX_LOGD, "[#%d] cpu_vmxroot_enter\n", cpu_data->cpu_id);

    cpu_data->host_cr4_vmxe = (get_cr4() & CR4_VMXE);
    if (cpu_data->host_cr4_vmxe) {
        if (debug_vmcs_count % 100000 == 0) {
            hax_log(HAX_LOGD, "[#%d] host VT has enabled!\n", cpu_data->cpu_id);
            hax_log(HAX_LOGD, "[#%d] Cr4 value = 0x%lx\n",
                    get_cr4(), cpu_data->cpu_id);
            log_host_cr4_vmxe = 1;
            log_host_cr4 = get_cr4();
        }
        debug_vmcs_count++;
    }

    set_cr4(get_cr4() | CR4_VMXE);
    /* HP systems & Mac systems workaround
     * When resuming from S3, some HP/Mac set the IA32_FEATURE_CONTROL MSR to
     * zero. Setting the lock bit to zero & then doing 'vmxon' would cause a GP.
     * As a workaround, when we see this condition, we enable the bits so that
     * we can launch vmxon & thereby hax.
     * bit 0 - Lock bit
     * bit 2 - Enable VMX outside SMX operation
     *
     * ********* To Do **************************************
     * This is the workground to fix BSOD when resume from S3
     * The best way is to add one power management handler, and set
     * IA32_FEATURE_CONTROL MSR in that PM S3 handler
     * *****************************************************
     */
    fc_msr = ia32_rdmsr(IA32_FEATURE_CONTROL);
    if (!(fc_msr & FC_LOCKED))
        ia32_wrmsr(IA32_FEATURE_CONTROL,
                   fc_msr | FC_LOCKED | FC_VMXON_OUTSMX);

    vmxon_addr = hax_page_pa(cpu_data->vmxon_page);
    result = asm_vmxon(&vmxon_addr);

    log_vmxon_res = result;
    log_vmxon_addr = vmxon_addr;

    if (result == VMX_SUCCEED) {
        cpu_data->vmm_flag |= VMXON_HAX;
    } else {
        bool fatal = true;

#ifdef HAX_PLATFORM_DARWIN
        if ((result == VMX_FAIL_INVALID) && cpu_data->host_cr4_vmxe) {
            // On macOS, if VMXON fails with VMX_FAIL_INVALID and host CR4.VMXE
            // was already set, it is very likely that another VMM (VirtualBox
            // or any VMM based on macOS Hypervisor Framework, e.g. Docker) is
            // running and did not call VMXOFF. In that case, the current host
            // logical processor is already in VMX operation, and we can use an
            // innocuous VMX instruction (VMPTRST) to confirm that.
            // However, if the above assumption is wrong and the host processor
            // is not actually in VMX operation, VMPTRST will probably cause a
            // host reboot. But we don't have a better choice, and it is worth
            // taking the risk.
            hax_paddr_t vmcs_addr;
            asm_vmptrst(&vmcs_addr);

            // It is still alive - Just assumption is right.
            fatal = false;
            result = VMX_SUCCEED;
            // Indicate that it is not necessary to call VMXOFF later
            cpu_data->vmm_flag &= ~VMXON_HAX;
        }
#endif

        if (fatal) {
            hax_log(HAX_LOGE, "[#%d] VMXON failed for region 0x%llx "
                    "(result=0x%x, vmxe=%x)\n", cpu_data->cpu_id,
                    hax_page_pa(cpu_data->vmxon_page), (uint32_t)result,
                    (uint32_t)cpu_data->host_cr4_vmxe);
            restore_host_cr4_vmxe(cpu_data);
            if (result == VMX_FAIL_INVALID) {
                log_vmxon_err_type1 = 1;
            } else {
                // TODO: Should VMX_FAIL_VALID be ignored? The current VMCS can
                // be cleared (deactivated and saved to memory) using VMCLEAR
                log_vmxon_err_type2 = 1;
            }
        }
    }
    cpu_data->vmxon_res = result;
    return result;
}
