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
#include "include/compiler.h"
#include "include/ia32_defs.h"
#include "include/vcpu.h"
#include "include/mtrr.h"
#include "include/vmx.h"
#include "include/cpu.h"
#include "include/cpuid.h"
#include "include/vm.h"
#include "include/debug.h"
#include "include/dump.h"

#include "include/intr.h"
#include "include/vtlb.h"
#include "include/ept.h"
#include "include/paging.h"
#include "include/hax_core_interface.h"
#include "include/hax_driver.h"

uint64_t gmsr_list[NR_GMSR] = {
    IA32_STAR,
    IA32_LSTAR,
    IA32_CSTAR,
    IA32_SF_MASK,
    IA32_KERNEL_GS_BASE
};

uint64_t hmsr_list[NR_HMSR] = {
    IA32_EFER,
    IA32_STAR,
    IA32_LSTAR,
    IA32_CSTAR,
    IA32_SF_MASK,
    IA32_KERNEL_GS_BASE
};

uint64_t emt64_msr[NR_EMT64MSR] = {
    IA32_STAR,
    IA32_LSTAR,
    IA32_CSTAR,
    IA32_SF_MASK,
    IA32_KERNEL_GS_BASE
};

extern uint32_t pw_reserved_bits_high_mask;

static void vcpu_init(struct vcpu_t *vcpu);
static void vcpu_prepare(struct vcpu_t *vcpu);
static void vcpu_init_emulator(struct vcpu_t *vcpu);

static void vmread_cr(struct vcpu_t *vcpu);
static void vmwrite_cr(struct vcpu_t *vcpu);

static int exit_exc_nmi(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_interrupt(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_triple_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_interrupt_window(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_taskswitch(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_hlt(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_invlpg(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_rdtsc(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_cr_access(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_dr_access(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_io_access(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_msr_read(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_msr_write(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_invalid_guest_state(struct vcpu_t *vcpu,
                                    struct hax_tunnel *htun);
static int exit_ept_misconfiguration(struct vcpu_t *vcpu,
                                     struct hax_tunnel *htun);
static int exit_ept_violation(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static int exit_unsupported_instruction(struct vcpu_t *vcpu, 
                                        struct hax_tunnel *htun);
static int null_handler(struct vcpu_t *vcpu, struct hax_tunnel *hun);

static void advance_rip(struct vcpu_t *vcpu);
static void handle_machine_check(struct vcpu_t *vcpu);

static void handle_cpuid_virtual(struct vcpu_t *vcpu, uint32_t eax, uint32_t ecx);
static void handle_mem_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static void check_flush(struct vcpu_t *vcpu, uint32_t bits);
static void vmwrite_efer(struct vcpu_t *vcpu);

static int handle_msr_read(struct vcpu_t *vcpu, uint32_t msr, uint64_t *val);
static int handle_msr_write(struct vcpu_t *vcpu, uint32_t msr, uint64_t val,
                            bool by_host);
static void handle_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static void vcpu_dump(struct vcpu_t *vcpu, uint32_t mask, const char *caption);
static void vcpu_state_dump(struct vcpu_t *vcpu);
static void vcpu_enter_fpu_state(struct vcpu_t *vcpu);

static int vcpu_set_apic_base(struct vcpu_t *vcpu, uint64_t val);
static bool vcpu_is_bsp(struct vcpu_t *vcpu);

static void vcpu_init_cpuid(struct vcpu_t *vcpu);
static int vcpu_alloc_cpuid(struct vcpu_t *vcpu);
static void vcpu_free_cpuid(struct vcpu_t *vcpu);

static uint32_t get_seg_present(uint32_t seg)
{
    mword ldtr_base;
    struct seg_desc_t *seg_desc;
    struct hstate *hstate = &get_cpu_data(hax_cpu_id())->hstate;

    ldtr_base = get_kernel_ldtr_base();
    seg_desc = (struct seg_desc_t *)ldtr_base + (seg >> 3);
    if (seg_desc->_present) {
        hstate->fake_gs = seg_desc->_raw;
    }
    return seg_desc->_present;
}

static void fake_seg_gs_entry(struct hstate *hstate)
{
    mword ldtr_base;
    struct seg_desc_t *seg_desc = NULL;
    uint16_t seg = hstate->gs;

    ldtr_base = get_kernel_ldtr_base();
    seg_desc = (struct seg_desc_t *)ldtr_base + (seg >> 3);
    if (seg_desc->_present == 0)
        seg_desc->_raw = hstate->fake_gs;
    set_kernel_gs(seg);
    ia32_wrmsr(IA32_GS_BASE, hstate->gs_base);
    seg_desc->_raw = 0;
}

static void get_segment_desc_t(segment_desc_t *sdt, uint32_t s, uint64_t b,
                               uint32_t l, uint32_t a)
{
    sdt->selector = s;
    sdt->base = b;
    sdt->limit = l;
    sdt->ar = a;
}

static inline void set_gdt(struct vcpu_state_t *state, uint64_t base,
                           uint64_t limit)
{
    state->_gdt.base = base;
    state->_gdt.limit = limit;
}

static inline void set_idt(struct vcpu_state_t *state, uint64_t base,
                           uint64_t limit)
{
    state->_idt.base = base;
    state->_idt.limit = limit;
}

static uint64_t vcpu_read_cr(struct vcpu_state_t *state, uint32_t n)
{
    uint64_t val = 0;

    switch (n) {
        case 0: {
            val = state->_cr0;
            break;
        }
        case 2: {
            val = state->_cr2;
            break;
        }
        case 3: {
            val = state->_cr3;
            break;
        }
        case 4: {
            val = state->_cr4;
            break;
        }
        default: {
            hax_log(HAX_LOGW, "Ignored unsupported CR%d read, returning 0\n",
                    n);
            break;
        }
    }

    hax_log(HAX_LOGD, "vcpu_read_cr cr %x val %llx\n", n, val);

    return val;
}

static void vcpu_write_cr(struct vcpu_state_t *state, uint32_t n, uint64_t val)
{
    hax_log(HAX_LOGD, "vcpu_write_cr cr %x val %llx\n", n, val);

    switch (n) {
        case 0: {
            state->_cr0 = val;
            break;
        }
        case 2: {
            state->_cr2 = val;
            break;
        }
        case 3: {
            state->_cr3 = val;
            break;
        }
        case 4: {
            state->_cr4 = val;
            break;
        }
        default: {
            hax_log(HAX_LOGE, "write_cr: Unsupported CR%d access\n", n);
            break;
        }
    }
}

void * vcpu_vmcs_va(struct vcpu_t *vcpu)
{
    return hax_page_va(vcpu->vmcs_page);
}

hax_paddr_t vcpu_vmcs_pa(struct vcpu_t *vcpu)
{
    return hax_page_pa(vcpu->vmcs_page);
}

static void set_activity_state(struct vcpu_state_t *vcpu_state, uint state)
{
    vcpu_state->_activity_state = state;
}

static uint get_activity_state(struct vcpu_state_t *state)
{
    return state->_activity_state;
}

void * get_vcpu_host(struct vcpu_t *vcpu)
{
    return vcpu ? vcpu->vcpu_host : NULL;
}

int set_vcpu_host(struct vcpu_t *vcpu, void *vcpu_host)
{
    if (!vcpu || (vcpu->vcpu_host && vcpu->vcpu_host != vcpu_host))
        return -1;

    vcpu->vcpu_host = vcpu_host;
    return 0;
}

int set_vcpu_tunnel(struct vcpu_t *vcpu, struct hax_tunnel *tunnel,
                    uint8_t *iobuf)
{
    if (!vcpu || (vcpu->tunnel && tunnel && vcpu->tunnel != tunnel) ||
            (vcpu->io_buf && iobuf && vcpu->io_buf != iobuf))
        return -1;

    vcpu->tunnel = tunnel;
    vcpu->io_buf = iobuf;

    return 0;
}

struct hax_tunnel * get_vcpu_tunnel(struct vcpu_t *vcpu)
{
    return vcpu ? vcpu->tunnel : NULL;
}

/*
 * vcpu_vpid_alloc()
 *
 * Allocate a non-zero unique VPID for virtual processor.
 * The allocated VPID is stored in vcpu->vpid.
 * In the case of allocating failure, the vcpu->vpid will keeps zero, which
 * means "do not enable VPID feature".
 *
 * Param: vcpu - specify the virtual processor who will get the VPID
 * Return Value: 0 - means success.  Negative value - means failure.
 */
static int vcpu_vpid_alloc(struct vcpu_t *vcpu)
{
    uint32_t vpid_seed_bits = sizeof(vcpu->vm->vpid_seed) * 8;
    uint8_t bit, max_bit;

    max_bit = vpid_seed_bits > 0xff ? 0xff : vpid_seed_bits;

    if (0 != vcpu->vpid) {
        hax_log(HAX_LOGW, "vcpu_vpid_alloc: vcpu %u in vm %d already has a "
                "valid VPID 0x%x.\n", vcpu->vcpu_id, vcpu->vm->vm_id,
                vcpu->vpid);
        return -1;
    }

    for (bit = 0; bit < max_bit; bit++) {
        if (!hax_test_and_set_bit(bit, (uint64_t *)vcpu->vm->vpid_seed))
            break;
    }

    if (bit == max_bit) {
        // No available VPID resource
        hax_log(HAX_LOGE, "vcpu_vpid_alloc: no available vpid resource. "
                "vcpu: %u, vm: %d\n", vcpu->vcpu_id, vcpu->vm->vm_id);
        return -2;
    }

    /*
     * We split vpid as high byte and low byte, the vpid seed is used to
     * generate low byte. We use the index of first zero bit in vpid seed plus 1
     * as the value of low_byte, and use vcpu->vm->vm_id as the value of high
     * byte.
     * Note: vpid can't be zero.
     */
    vcpu->vpid = (uint16_t)(vcpu->vm->vm_id << 8) + (uint16_t)(bit + 1);
    hax_log(HAX_LOGI, "vcpu_vpid_alloc: succeed! vpid: 0x%x. vcpu_id: %u, "
            "vm_id: %d.\n", vcpu->vpid, vcpu->vcpu_id, vcpu->vm->vm_id);

    return 0;
}

/*
 * vcpu_vpid_free()
 *
 * Free the VPID that stored in vcpu->vpid for virtual processor.
 * The value of vcpu->vpid will be reset to zero after freeing.
 *
 * Param: vcpu - specify the virtual processor whose VPID will be freed.
 * Return Value: 0 - means success.
 *               Negative value - means vcpu->vpid has been already freed.
 */
static int vcpu_vpid_free(struct vcpu_t *vcpu)
{
    uint8_t bit = (vcpu->vpid & 0xff) - 1;

    if (0 == vcpu->vpid) {
        hax_log(HAX_LOGW, "vcpu_vpid_free: vcpu %u in vm %d does not have a "
                "valid VPID.\n", vcpu->vcpu_id, vcpu->vm->vm_id);
        return -1;
    }

    hax_log(HAX_LOGI, "vcpu_vpid_free: Clearing bit: 0x%x, vpid_seed: 0x%llx. "
            "vcpu_id: %u, vm_id: %d.\n", bit, *(uint64_t *)vcpu->vm->vpid_seed,
            vcpu->vcpu_id, vcpu->vm->vm_id);
    if (0 != hax_test_and_clear_bit(bit, (uint64_t *)(vcpu->vm->vpid_seed))) {
        hax_log(HAX_LOGW, "vcpu_vpid_free: bit for vpid 0x%x of vcpu %u in vm "
                "%d was already clear.\n", vcpu->vpid, vcpu->vcpu_id,
                vcpu->vm->vm_id);
    }
    vcpu->vpid = 0;

    return 0;
}

static int (*handler_funcs[])(struct vcpu_t *vcpu, struct hax_tunnel *htun) = {
    [VMX_EXIT_INT_EXCEPTION_NMI]  = exit_exc_nmi,
    [VMX_EXIT_EXT_INTERRUPT]      = exit_interrupt,
    [VMX_EXIT_TRIPLE_FAULT]       = exit_triple_fault,
    [VMX_EXIT_PENDING_INTERRUPT]  = exit_interrupt_window,
    [VMX_EXIT_PENDING_NMI]        = exit_interrupt_window,
    [VMX_EXIT_CPUID]              = exit_cpuid,
    [VMX_EXIT_HLT]                = exit_hlt,
    [VMX_EXIT_INVLPG]             = exit_invlpg,
    [VMX_EXIT_RDTSC]              = exit_rdtsc,
    [VMX_EXIT_CR_ACCESS]          = exit_cr_access,
    [VMX_EXIT_DR_ACCESS]          = exit_dr_access,
    [VMX_EXIT_IO]                 = exit_io_access,
    [VMX_EXIT_MSR_READ]           = exit_msr_read,
    [VMX_EXIT_MSR_WRITE]          = exit_msr_write,
    [VMX_EXIT_FAILED_VMENTER_GS]  = exit_invalid_guest_state,
    [VMX_EXIT_EPT_VIOLATION]      = exit_ept_violation,
    [VMX_EXIT_EPT_MISCONFIG]      = exit_ept_misconfiguration,
    [VMX_EXIT_GETSEC]             = exit_unsupported_instruction,
    [VMX_EXIT_INVD]               = exit_unsupported_instruction,
    [VMX_EXIT_VMCALL]             = exit_unsupported_instruction,
    [VMX_EXIT_VMCLEAR]            = exit_unsupported_instruction,
    [VMX_EXIT_VMLAUNCH]           = exit_unsupported_instruction,
    [VMX_EXIT_VMPTRLD]            = exit_unsupported_instruction,
    [VMX_EXIT_VMPTRST]            = exit_unsupported_instruction,
    //VMREAD and VMWRITE vm-exits are conditional. When "VMCS shadowing" bit
    //in secondary CPU VM-execution control is 0, these exit. This condition
    //holds in haxm.
    [VMX_EXIT_VMREAD]             = exit_unsupported_instruction,
    [VMX_EXIT_VMWRITE]            = exit_unsupported_instruction,
    [VMX_EXIT_VMRESUME]           = exit_unsupported_instruction,
    [VMX_EXIT_VMXOFF]             = exit_unsupported_instruction,
    [VMX_EXIT_VMXON]              = exit_unsupported_instruction,
    [VMX_EXIT_XSETBV]             = exit_unsupported_instruction,
};

static int nr_handlers = ARRAY_ELEMENTS(handler_funcs);

struct vcpu_t *vcpu_create(struct vm_t *vm, void *vm_host, int vcpu_id)
{
    struct hax_tunnel_info info;
    struct vcpu_t *vcpu;

    hax_log(HAX_LOGD, "vcpu_create vcpu_id %x\n", vcpu_id);

    if (!valid_vcpu_id(vcpu_id))
        return NULL;

    vcpu = (struct vcpu_t *)hax_vmalloc(sizeof(struct vcpu_t), HAX_MEM_NONPAGE);
    if (!vcpu)
        goto fail_0;

    hax_clear_panic_log(vcpu);

    memset(vcpu, 0, sizeof(struct vcpu_t));

    if (hax_vcpu_setup_hax_tunnel(vcpu, &info) < 0) {
        hax_log(HAX_LOGE, "cannot setup hax_tunnel for vcpu.\n");
        goto fail_1;
    }

    vcpu->vmcs_page = (struct hax_page *)hax_alloc_page(0, 1);
    if (!vcpu->vmcs_page)
        goto fail_2;

    vcpu->gstate.gfxpage = (struct hax_page *)hax_alloc_page(0, 1);
    if (!vcpu->gstate.gfxpage)
        goto fail_3;

    hax_clear_page(vcpu->gstate.gfxpage);
    hax_clear_page(vcpu->vmcs_page);

    vcpu->state = (struct vcpu_state_t *)hax_vmalloc(
            sizeof(struct vcpu_state_t), HAX_MEM_NONPAGE);
    if (!vcpu->state)
        goto fail_4;
    memset(vcpu->state, 0, sizeof(struct vcpu_state_t));

    vcpu->tmutex = hax_mutex_alloc_init();
    if (!vcpu->tmutex)
        goto fail_5;

    if (!vcpu_vtlb_alloc(vcpu))
        goto fail_6;

    if (!vcpu_alloc_cpuid(vcpu))
        goto fail_7;

    if (hax_vcpu_create_host(vcpu, vm_host, vm->vm_id, vcpu_id))
        goto fail_8;

    vcpu->prev_cpu_id = (uint32_t)(~0ULL);
    vcpu->cpu_id = hax_cpu_id();
    vcpu->vcpu_id = vcpu_id;
    vcpu->is_running = 0;
    vcpu->vm = vm;
    // Must ensure it is called before fill_common_vmcs is called
    vcpu_vpid_alloc(vcpu);

    // Prepare guest environment
    vcpu_init(vcpu);

    // First time vmclear/vmptrld on current CPU
    vcpu_prepare(vcpu);



    // Publish the vcpu
    hax_mutex_lock(vm->vm_lock);
    hax_list_add(&vcpu->vcpu_list, &vm->vcpu_list);
    // The caller should get_vm thus no race with vm destroy
    hax_atomic_add(&vm->ref_count, 1);
    hax_mutex_unlock(vm->vm_lock);

    // Initialize emulator
    vcpu_init_emulator(vcpu);

    hax_log(HAX_LOGD, "vcpu %d is created.\n", vcpu->vcpu_id);
    return vcpu;
fail_8:
    vcpu_free_cpuid(vcpu);
fail_7:
    vcpu_vtlb_free(vcpu);
fail_6:
    hax_mutex_free(vcpu->tmutex);
fail_5:
    hax_vfree(vcpu->state, sizeof(struct vcpu_state_t));
fail_4:
    if (vcpu->gstate.gfxpage) {
        hax_free_pages(vcpu->gstate.gfxpage);
    }
fail_3:
    hax_free_pages(vcpu->vmcs_page);
fail_2:
    hax_vcpu_destroy_hax_tunnel(vcpu);
fail_1:
    hax_vfree(vcpu, sizeof(struct vcpu_t));
fail_0:
    hax_log(HAX_LOGE, "Cannot allocate memory to create vcpu.\n");
    return NULL;
}

/*
 * We don't need corresponding vcpu_core_close because once closed, the VM will
 * be destroyed
 */
int hax_vcpu_core_open(struct vcpu_t *vcpu)
{
    if (!vcpu)
        return -ENODEV;

    if (hax_test_and_set_bit(VCPU_STATE_FLAGS_OPENED, &(vcpu->flags)))
        return -EBUSY;

    return 0;
}

static int _vcpu_teardown(struct vcpu_t *vcpu)
{
    int vcpu_id = vcpu->vcpu_id;

    if (vcpu->mmio_fetch.kva) {
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &vcpu->mmio_fetch.kmap);
    }

    // TODO: we should call invvpid after calling vcpu_vpid_free().
    vcpu_vpid_free(vcpu);

    if (vcpu->gstate.gfxpage) {
        hax_free_pages(vcpu->gstate.gfxpage);
    }
    hax_free_pages(vcpu->vmcs_page);
    hax_vfree(vcpu->state, sizeof(struct vcpu_state_t));
    vcpu_vtlb_free(vcpu);
    hax_mutex_free(vcpu->tmutex);
    vcpu_free_cpuid(vcpu);
    hax_vfree(vcpu, sizeof(struct vcpu_t));

    hax_log(HAX_LOGI, "vcpu %d is teardown.\n", vcpu_id);
    return 0;
}

int vcpu_teardown(struct vcpu_t *vcpu)
{
    struct vm_t *vm = vcpu->vm;
    int ret;

    hax_mutex_lock(vm->vm_lock);
    hax_list_del(&vcpu->vcpu_list);
    hax_mutex_unlock(vm->vm_lock);

    ret = _vcpu_teardown(vcpu);
    // Should not hold the vmlock here
    // Trying to put vm again
    hax_put_vm(vm);
    return ret;
}

static void vcpu_init(struct vcpu_t *vcpu)
{
    // TODO: Need to decide which mode guest will start
    struct vcpu_state_t *state = vcpu->state;
    int i;

    hax_mutex_lock(vcpu->tmutex);

    // TODO: mtrr ?
    vcpu->cr_pat = 0x0007040600070406ULL;
    vcpu->cur_state = GS_VALID;
    vmx(vcpu, entry_exception_vector) = ~0u;
    vmx(vcpu, cr0_mask) = 0;
    vmx(vcpu, cr0_shadow) = 0;
    vmx(vcpu, cr4_mask) = 0;
    vmx(vcpu, cr4_shadow) = 0;

    vcpu->ref_count = 1;

    vcpu->tsc_offset = 0ULL - ia32_rdtsc();

    // Prepare the vcpu state to Power-up
    state->_rflags = 2;
    state->_rip = 0x0000fff0;
    state->_cr0 = 0x60000010;

    get_segment_desc_t(&state->_cs, 0xf000, 0xffff0000, 0xffff, 0x93);
    get_segment_desc_t(&state->_ss, 0, 0, 0xffff, 0x93);
    get_segment_desc_t(&state->_ds, 0, 0, 0xffff, 0x93);
    get_segment_desc_t(&state->_es, 0, 0, 0xffff, 0x93);
    get_segment_desc_t(&state->_fs, 0, 0, 0xffff, 0x93);
    get_segment_desc_t(&state->_gs, 0, 0, 0xffff, 0x93);
    set_gdt(state, 0, 0xffff);
    set_idt(state, 0, 0xffff);
    get_segment_desc_t(&state->_ldt, 0, 0, 0xffff, 0x82);
    get_segment_desc_t(&state->_tr, 0, 0, 0xffff, 0x83);

    state->_dr0 = state->_dr1 = state->_dr2 = state->_dr3 = 0x0;
    state->_dr6 = DR6_SETBITS;
    state->_dr7 = DR7_SETBITS;
    vcpu->dr_dirty = 1;

    // Initialize guest MSR state, i.e. a list of MSRs and their initial values.
    // Note that all zeros is not a valid state (see below). At the first VM
    // entry, these MSRs will be loaded with these values, unless QEMU has
    // overridden them using HAX_VCPU_IOCTL_SET_MSRS.
    // TODO: Enable hardware-assisted MSR save/restore (cf. IA SDM Vol. 3C
    // 31.10.2-31.10.3: Using VM-Exit/VM-Entry Controls for MSRs).
    for (i = 0; i < NR_GMSR; i++) {
        // Without this initialization, |entry| defaults to 0, which is also
        // a valid MSR (IA32_P5_MC_ADDR, often implemented as an alias for
        // IA32_MC0_CTL), but which is not one that HAXM should tamper with.
        // In fact, writing 0 to it has serious consequences, including
        // disabling SGX (cf. IA SDM Vol. 3D 42.15.2: Machine Check Enables).
        vcpu->gstate.gmsr[i].entry = gmsr_list[i];
        // 0 is an appropriate initial value for all MSRs in gmsr_list[]
        vcpu->gstate.gmsr[i].value = 0;
    }

    // Initialize IA32_APIC_BASE MSR
    vcpu->gstate.apic_base = APIC_BASE_DEFAULT_ADDR | APIC_BASE_ENABLE;
    if (vcpu_is_bsp(vcpu)) {
        vcpu->gstate.apic_base |= APIC_BASE_BSP;
    }

    // Initialize guest CPUID
    vcpu_init_cpuid(vcpu);

    hax_mutex_unlock(vcpu->tmutex);
}

#ifdef DEBUG_HOST_STATE
static int check_panic(void)
{
    char *kernel_panic = NULL;
    return 0;
}

// Code to check the host state between vmluanch and vmexit
static uint32_t get_seg_avail(uint32_t seg)
{
    mword gdtr_base;
    struct seg_desc_t *sd;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);

    return sd->_avl;
}

static void dump_segment(uint32_t seg)
{
    struct seg_desc_t *sd;
    mword gdtr_base;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);
    hax_log(HAX_LOGD, "seg %x value %llx\n", seg, sd->_raw);
}

static int check_cs(uint32_t seg)
{
    mword gdtr_base;
    mword desc_base;
    struct seg_desc_t *sd;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);
    if (sd->_base0 != 0 || sd->_base1 != 0)
        return 1;

    if (sd->_limit1 != 0xf || sd->_limit0 != 0xffff)
        return 1;
    if (sd->_type != 0xb)
        return 1;
    if (sd->_s != 1)
        return 1;
    if (sd->_dpl != 0)
        return 1;
    if (sd->_present != 1)
        return 1;
    if (sd->_longmode != 0x1)
        return 1;
    if (sd->_d != 0x0)
        return 1;
    if (sd->_granularity != 1)
        return 1;
    return 0;
}

static int check_data_seg(uint32_t seg)
{
    mword gdtr_base;
    mword desc_base;
    struct seg_desc_t *sd;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);

    if (sd->_base0 != 0 || sd->_base1 != 0)
        return 1;

    if (sd->_limit1 != 0xf || sd->_limit0 != 0xffff)
        return 1;
    if (sd->_type != 0x3)
        return 1;
    if (sd->_s != 1)
        return 1;
    if (sd->_dpl != 0) {
        // The DPL is wrong for Mac return 0 now
        return 0;
    }
    if (sd->_present != 1)
        return 1;
    // if (sd->_longmode != 0x1)
    //     return 1;
    if (sd->_d != 0x1)
        return 1;
    if (sd->_granularity != 1)
        return 1;
    return 0;
}

static int check_stack_seg(uint32_t seg)
{
    mword gdtr_base;
    mword desc_base;
    struct seg_desc_t *sd;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);

    if (sd->_base0 != 0 || sd->_base1 != 0)
        return 1;

    if (sd->_limit1 != 0xf || sd->_limit0 != 0xffff)
        return 1;
    if (sd->_type != 0x3)
        return 1;
    if (sd->_s != 1)
        return 1;
    if (sd->_dpl != 0)
        return 1;
    if (sd->_present != 1)
        return 1;
    // if (sd->_longmode != 0x1)
    //     return 1;
    if (sd->_d != 0x1)
        return 1;
    if (sd->_granularity != 1)
        return 1;
    return 0;
}

static int check_tr_seg(uint32_t seg)
{
    mword gdtr_base;
    struct seg_desc_t *sd;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);

    // if (sd->_base0 != 0 || sd->_base1 != 0)
    //     return 1;

    if (sd->_limit1 != 0x0 || sd->_limit0 != 0x67)
        return 1;
    if (sd->_type != 0xb)
        return 1;
    if (sd->_s != 0)
        return 1;
    if (sd->_dpl != 0)
        return 1;
    if (sd->_present != 1)
        return 1;
    // if (sd->_longmode != 0x1)
    //     return 1;
    if (sd->_d != 0x0)
        return 1;
    if (sd->_granularity != 0)
        return 1;
    return 0;
}

static int check_fgs_seg(uint32_t seg, uint fs)
{
    mword gdtr_base;
    mword desc_base;
    struct seg_desc_t *sd;
    mword base;

    if (seg == 0) {
        hax_log(HAX_LOGD, "fgs_seg seg is %x fs %x\n", seg, fs);
        return 0;
    }
    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);

    if (fs) {
        base = ia32_rdmsr(IA32_FS_BASE);
    } else {
        base = ia32_rdmsr(IA32_GS_BASE);
    }

    if ((base & 0xffffff) != sd->_base0 ||
        ((base >> 24) & 0xff) != sd->_base1) {
        // hax_log(HAX_LOGD, "%s base address mismatch base %llx sd %llx\n",
        //         fs ? "fs" : "gs", base, sd->_raw);
        // return 1;
        return 0;
    }
    if (sd->_base0 != 0 || sd->_base1 != 0)
        return 1;

    if (sd->_limit1 != 0xf || sd->_limit0 != 0xffff)
        return 1;
    if (sd->_type != 0x3)
        return 1;
    if (sd->_s != 1)
        return 1;
    if (sd->_dpl != 0)
        return 1;
    if (sd->_present != 1)
        return 1;
    // if (sd->_longmode != 0x1)
    //     return 1;
    if (sd->_d != 0x1)
        return 1;
    if (sd->_granularity != 1)
        return 1;
    return 0;
}

int vcpu_get_host_state(struct vcpu_t *vcpu, int pre)
{
    uint64_t value;
    struct host_state_compare *hsc;
    hsc = pre ? &vcpu->hsc_pre : &vcpu->hsc_post;
    memset(hsc, 0, sizeof(struct host_state_compare));
    hsc->cr0 = get_cr0();
    hsc->cr2 = get_cr2();
    hsc->cr3 = get_cr3();
    hsc->cr4 = get_cr4();

    // Check segmentation
    hsc->cs = get_kernel_cs();
    hsc->cs_avail = get_seg_avail(hsc->cs);
    hsc->ds = get_kernel_ds();
    hsc->ds_avail = get_seg_avail(hsc->ds);
    hsc->es = get_kernel_es();
    hsc->es_avail = get_seg_avail(hsc->es);

    hsc->ss = get_kernel_ss();
    hsc->ss_avail = get_seg_avail(hsc->ss);

    hsc->fs = get_kernel_fs();
    hsc->fs_avail = get_seg_avail(hsc->fs);
    hsc->gs = get_kernel_gs();
    hsc->gs_avail = get_seg_avail(hsc->gs);
    hsc->tr = get_kernel_tr_selector();
    hsc->tr_avail = get_seg_avail(hsc->tr);
    hsc->ldt = get_kernel_ldt();

    hsc->efer = ia32_rdmsr(IA32_EFER);
    hsc->sysenter_cs = ia32_rdmsr(IA32_SYSENTER_CS);
    hsc->sysenter_eip = ia32_rdmsr(IA32_SYSENTER_EIP);
    hsc->sysenter_esp = ia32_rdmsr(IA32_SYSENTER_ESP);

    hsc->pat_msr = ia32_rdmsr(IA32_CR_PAT);
    hsc->fs_msr = ia32_rdmsr(IA32_FS_BASE);
    hsc->gs_msr = ia32_rdmsr(IA32_GS_BASE);

    hsc->rflags = get_kernel_rflags();

    if (pre) {
        if (check_cs(hsc->cs)) {
            hax_log(HAX_LOGD, "CS does not pass the check.\n");
            dump_segment(hsc->cs);
            // check_panic();
        }
        if (check_stack_seg(hsc->ss)) {
            hax_log(HAX_LOGD, "SS does not pass the check.\n");
            dump_segment(hsc->ss);
            // check_panic();
        }
        if (check_fgs_seg(hsc->fs, 1)) {
            hax_log(HAX_LOGD, "FS does not pass the check.\n");
            dump_segment(hsc->fs);
            // check_panic();
        }
        if (check_fgs_seg(hsc->gs, 0)) {
            hax_log(HAX_LOGD, "GS does not pass the check.\n");
            dump_segment(hsc->gs);
            // check_panic();
        }
        if (check_data_seg(hsc->ds) || check_data_seg(hsc->es)) {
            hax_log(HAX_LOGD, "DS or ES does not pass the check.\n");
            dump_segment(hsc->ds);
            dump_segment(hsc->es);
            // check_panic();
        }
        if (check_tr_seg(hsc->tr)) {
            hax_log(HAX_LOGD, "TR does not pass the check.\n");
            dump_segment(hsc->tr);
            // check_panic();
        }
    }
    return 0;
}

static int dump_hsc_state(struct host_state_compare *hsc)
{
    return 0;
}

int compare_host_state(struct vcpu_t *vcpu)
{
    struct host_state_compare *pre, *post;

    pre = &vcpu->hsc_pre;
    post = &vcpu->hsc_post;
    if (memcmp(pre, post, sizeof(struct host_state_compare))) {
        hax_log(HAX_LOGD, "The previous and next is not same.\n");
        dump_hsc_state(pre);
        dump_hsc_state(post);
        check_panic();
    }
    return 0;
}
#endif

static int is_emt64_msr(uint64_t entry)
{
    int i = 0;
    for (i = 0; i < NR_EMT64MSR; i++) {
        if (entry == emt64_msr[i])
            return 1;
    }
    return 0;
}

void save_guest_msr(struct vcpu_t *vcpu)
{
    int i;
    struct gstate *gstate = &vcpu->gstate;
    bool em64t_support = cpu_has_feature(X86_FEATURE_EM64T);

    for (i = 0; i < NR_GMSR; i++) {
        gstate->gmsr[i].entry = gmsr_list[i];
        if (em64t_support || !is_emt64_msr(gmsr_list[i])) {
            gstate->gmsr[i].value = ia32_rdmsr(gstate->gmsr[i].entry);
        }
    }

    if (cpu_has_feature(X86_FEATURE_RDTSCP)) {
        gstate->tsc_aux = ia32_rdmsr(IA32_TSC_AUX);
    }

    if (!hax->apm_version)
        return;

    // APM v1: save IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32_t msr = (uint32_t)(IA32_PMC0 + i);
        gstate->apm_pmc_msrs[i] = ia32_rdmsr(msr);
        msr = (uint32_t)(IA32_PERFEVTSEL0 + i);
        gstate->apm_pes_msrs[i] = ia32_rdmsr(msr);
    }
}

void load_guest_msr(struct vcpu_t *vcpu)
{
    int i;
    struct gstate *gstate = &vcpu->gstate;
    bool em64t_support = cpu_has_feature(X86_FEATURE_EM64T);

    for (i = 0; i < NR_GMSR; i++) {
        if (em64t_support || !is_emt64_msr(gstate->gmsr[i].entry)) {
            ia32_wrmsr(gstate->gmsr[i].entry, gstate->gmsr[i].value);
        }
    }

    if (cpu_has_feature(X86_FEATURE_RDTSCP)) {
        ia32_wrmsr(IA32_TSC_AUX, gstate->tsc_aux);
    }

    if (!hax->apm_version)
        return;

    // APM v1: restore IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32_t msr = (uint32_t)(IA32_PMC0 + i);
        ia32_wrmsr(msr, gstate->apm_pmc_msrs[i]);
        msr = (uint32_t)(IA32_PERFEVTSEL0 + i);
        ia32_wrmsr(msr, gstate->apm_pes_msrs[i]);
    }
}

static void save_host_msr(struct vcpu_t *vcpu)
{
    int i;
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    bool em64t_support = cpu_has_feature(X86_FEATURE_EM64T);

    for (i = 0; i < NR_HMSR; i++) {
        hstate->hmsr[i].entry = hmsr_list[i];
        if (em64t_support || !is_emt64_msr(hmsr_list[i])) {
            hstate->hmsr[i].value = ia32_rdmsr(hstate->hmsr[i].entry);
        }
    }

    if (cpu_has_feature(X86_FEATURE_RDTSCP)) {
        hstate->tsc_aux = ia32_rdmsr(IA32_TSC_AUX);
    }

    if (!hax->apm_version)
        return;

    // APM v1: save IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32_t msr = (uint32_t)(IA32_PMC0 + i);
        hstate->apm_pmc_msrs[i] = ia32_rdmsr(msr);
        msr = (uint32_t)(IA32_PERFEVTSEL0 + i);
        hstate->apm_pes_msrs[i] = ia32_rdmsr(msr);
    }
}

static void load_host_msr(struct vcpu_t *vcpu)
{
    int i;
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    bool em64t_support = cpu_has_feature(X86_FEATURE_EM64T);

    for (i = 0; i < NR_HMSR; i++) {
        if (em64t_support || !is_emt64_msr(hstate->hmsr[i].entry)) {
            ia32_wrmsr(hstate->hmsr[i].entry, hstate->hmsr[i].value);
        }
    }

    if (cpu_has_feature(X86_FEATURE_RDTSCP)) {
        ia32_wrmsr(IA32_TSC_AUX, hstate->tsc_aux);
    }

    if (!hax->apm_version)
        return;

    // APM v1: restore IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32_t msr = (uint32_t)(IA32_PMC0 + i);
        ia32_wrmsr(msr, hstate->apm_pmc_msrs[i]);
        msr = (uint32_t)(IA32_PERFEVTSEL0 + i);
        ia32_wrmsr(msr, hstate->apm_pes_msrs[i]);
    }
}

static inline bool is_host_debug_enabled(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    return !!(hstate->dr7 & HBREAK_ENABLED_MASK);
}

static inline bool is_thread_migrated(struct vcpu_t *vcpu)
{
    /*
     * Sometimes current thread might be migrated to other CPU core. In this
     * case, registers might be different with them in original core.
     * This function is to check thread is migrated or not.
     */
    return (vcpu->cpu_id != vcpu->prev_cpu_id);
}

static void load_dirty_vmcs_fields(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;

    // rflags
    if (vcpu->debug_control_dirty) {
        // Single-stepping
        if (vcpu->debug_control & HAX_DEBUG_STEP) {
            state->_rflags |= EFLAGS_TF;
        } else {
            state->_rflags &= ~EFLAGS_TF;
        }
        vcpu->rflags_dirty = 1;
        vcpu->debug_control_dirty = 0;
    }
    if (vcpu->rflags_dirty) {
        vmwrite(vcpu, GUEST_RFLAGS, state->_rflags);
        vcpu->rflags_dirty = 0;
    }

    /*
     * interruptibility
     * 26.3.1.5 Checks on Guest Non-Register State
     * Bit 0 (blocking by STI) must be 0 if the IF flag (bit 9) is 0 in the
     *   RFLAGS field.
     * This is a WA to fix the VM-entry failure due to invalid guest state,
     *   sometimes when a snapshot is loaded but IF and interruptibility_state
     *   don't pass the checks as mentioned in SDM 26.3.1.5.
     * In in-order execution, interruptibility_state is updated when advancing
     *   the IP. However when a snapshot is loaded, EFLAGS are restored but
     *   guest non-register state not restored.
     *   TODO: Find better approach instead of letting the check pass.
     */
    if (!(state->_rflags & EFLAGS_IF)) {
        if (vmx(vcpu, interruptibility_state).raw &
            GUEST_INTRSTAT_STI_BLOCKING) {
            vmx(vcpu, interruptibility_state).raw &=
                    ~GUEST_INTRSTAT_STI_BLOCKING;
            vcpu->interruptibility_dirty = 1;
        }
    }
    if (vcpu->interruptibility_dirty) {
        vmwrite(vcpu, GUEST_INTERRUPTIBILITY,
                vmx(vcpu, interruptibility_state).raw);
        vcpu->interruptibility_dirty = 0;
    }

    // rip
    if (vcpu->rip_dirty) {
        vmwrite(vcpu, GUEST_RIP, state->_rip);
        vcpu->rip_dirty = 0;
    }

    // primary cpu ctrl
    if (vcpu->pcpu_ctls_dirty) {
        vmwrite(vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS, vmx(vcpu, pcpu_ctls));
        vcpu->pcpu_ctls_dirty = 0;
    }

    // FS base
    if (vcpu->fs_base_dirty) {
        vmwrite(vcpu, GUEST_FS_BASE, vcpu->state->_fs.base);
        vcpu->fs_base_dirty = 0;
    }
}

static inline bool is_guest_dr_dirty(struct vcpu_t *vcpu)
{
    return (vcpu->dr_dirty || is_thread_migrated(vcpu));
}

static void save_guest_dr(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;

    /*
     * Only dr6 needs to be saved. Guest couldn't change dr registers directly
     * except for dr6. Guest writing dr is captured by exit_dr_access, and
     * saved in guest state dr.
     */
    state->_dr6 = get_dr6();
}

static void load_guest_dr(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;

    if (!(is_guest_dr_dirty(vcpu) || is_host_debug_enabled(vcpu)))
        return;

    set_dr0(state->_dr0);
    set_dr1(state->_dr1);
    set_dr2(state->_dr2);
    set_dr3(state->_dr3);
    set_dr6(state->_dr6);
    vmwrite(vcpu, GUEST_DR7, state->_dr7);

    vcpu->dr_dirty = 0;
}

static void save_host_dr(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    // dr7 is used to check host debugging enabled or not
    hstate->dr7 = get_dr7();

    if (!is_host_debug_enabled(vcpu))
        return;

    hstate->dr0 = get_dr0();
    hstate->dr1 = get_dr1();
    hstate->dr2 = get_dr2();
    hstate->dr3 = get_dr3();
    hstate->dr6 = get_dr6();
}

static void load_host_dr(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    if (!is_host_debug_enabled(vcpu))
        return;

    set_dr0(hstate->dr0);
    set_dr1(hstate->dr1);
    set_dr2(hstate->dr2);
    set_dr3(hstate->dr3);
    set_dr6(hstate->dr6);
    set_dr7(hstate->dr7);
}

void vcpu_save_host_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    // In case we do not know the specific operations with different OSes,
    // we save all of them at the initial time
    uint16_t gs = get_kernel_gs();
    uint16_t fs = get_kernel_fs();

    get_kernel_gdt(&hstate->host_gdtr);
    get_kernel_idt(&hstate->host_idtr);

    vmwrite(vcpu, HOST_CR3, get_cr3());
    vmwrite(vcpu, HOST_CR4, get_cr4());

    hstate->_efer = ia32_rdmsr(IA32_EFER);

    if (vmx(vcpu, exit_ctls) & EXIT_CONTROL_LOAD_EFER) {
        vmwrite(vcpu, HOST_EFER, hstate->_efer);
    }

    hstate->_pat = ia32_rdmsr(IA32_CR_PAT);
    vmwrite(vcpu, HOST_PAT, hstate->_pat);

#ifdef HAX_ARCH_X86_64
    vmwrite(vcpu, HOST_CS_SELECTOR, get_kernel_cs());
#else
    if (is_compatible()) {  // compatible

        vmwrite(vcpu, HOST_CS_SELECTOR, HAX_KERNEL64_CS);
    } else {
        vmwrite(vcpu, HOST_CS_SELECTOR, get_kernel_cs());
    }
#endif
    vmwrite(vcpu, HOST_SS_SELECTOR, get_kernel_ss() & 0xfff8);

    if (get_kernel_ss() & 0x7) {
        hax_log(HAX_LOGD, "Kernel SS %x with 0x7\n", get_kernel_ss());
    }

    vmwrite(vcpu, HOST_DS_SELECTOR, get_kernel_ds() & 0xfff8);
    if (get_kernel_ds() & 0x7) {
        hstate->ds = get_kernel_ds();
        hstate->seg_valid |= HOST_SEG_VALID_DS;
    }
    vmwrite(vcpu, HOST_ES_SELECTOR, get_kernel_es() & 0xfff8);
    if (get_kernel_es() & 0x7) {
        hstate->es = get_kernel_es();
        hstate->seg_valid |= HOST_SEG_VALID_ES;
    }
    // If GS segmentation does not meet VT requirement, save/restore it
    // manually.
    if (gs & 0x7) {
        /*
         * The workaroud to avoid kernel clear GS entry in LDT.
         * Check if GS is in LDT or not, if yes, check if kernel clears the GS
         * entry or not. Record the last time valid GS entry.
         */
        if ((gs & 0x4) && !get_seg_present(gs)) {
            hstate->seg_not_present |= HOST_SEG_NOT_PRESENT_GS;
        }
        hstate->gs = gs;
        hstate->gs_base = ia32_rdmsr(IA32_GS_BASE);
        hstate->seg_valid |= HOST_SEG_VALID_GS;
        vmwrite(vcpu, HOST_GS_SELECTOR, 0);
    } else {
        vmwrite(vcpu, HOST_GS_SELECTOR, gs);
#ifdef HAX_ARCH_X86_64
        // For ia32e mode, the MSR holds the base 3.4.4
        vmwrite(vcpu, HOST_GS_BASE, ia32_rdmsr(IA32_GS_BASE));
#else
        if (is_compatible()) {
            vmwrite(vcpu, HOST_GS_BASE, ia32_rdmsr(IA32_GS_BASE));
        } else {
            // For other, normal segment base
            vmwrite(vcpu, HOST_GS_BASE, get_kernel_fs_gs_base(gs));
        }
#endif
    }

    if (fs & 0x7) {
        hax_log(HAX_LOGD, "fs %x\n", fs);
        hstate->fs = fs;
        hstate->fs_base = ia32_rdmsr(IA32_FS_BASE);
        hstate->seg_valid |= HOST_SEG_VALID_FS;
        vmwrite(vcpu, HOST_FS_SELECTOR, 0);
    } else {
        vmwrite(vcpu, HOST_FS_SELECTOR, fs);
#ifdef HAX_ARCH_X86_64
        // For ia32e mode, the MSR holds the base 3.4.4
        vmwrite(vcpu, HOST_FS_BASE, ia32_rdmsr(IA32_FS_BASE));
#else
        if (is_compatible()) {
            vmwrite(vcpu, HOST_FS_BASE, ia32_rdmsr(IA32_FS_BASE));
        } else {
            // For other, normal segment base
            vmwrite(vcpu, HOST_FS_BASE, get_kernel_fs_gs_base(fs));
        }
#endif
    }

    vmwrite(vcpu, HOST_TR_SELECTOR, get_kernel_tr_selector());
    vmwrite(vcpu, HOST_TR_BASE, get_kernel_tr_base());
    vmwrite(vcpu, HOST_GDTR_BASE, get_kernel_gdtr_base_4vmcs());
    vmwrite(vcpu, HOST_IDTR_BASE, get_kernel_idtr_base());

    // Handle SYSENTER/SYSEXIT MSR
    vmwrite(vcpu, HOST_SYSENTER_CS, ia32_rdmsr(IA32_SYSENTER_CS));
    vmwrite(vcpu, HOST_SYSENTER_EIP, ia32_rdmsr(IA32_SYSENTER_EIP));
    vmwrite(vcpu, HOST_SYSENTER_ESP, ia32_rdmsr(IA32_SYSENTER_ESP));

    // LDTR is unusable from spec, do we need ldt for host?
    hstate->ldt_selector = get_kernel_ldt();

    /*
     * If we don't support 64-bit guest, can we do not save/load/restore the
     * MSRs for SYSCALL/SYSRET?
     */
    save_host_msr(vcpu);

    hstate->hcr2 = get_cr2();

    save_host_dr(vcpu);

    vcpu_enter_fpu_state(vcpu);
    // CR0 should be written after host fpu state is saved
    vmwrite(vcpu, HOST_CR0, get_cr0());
}

static void vcpu_update_exception_bitmap(struct vcpu_t *vcpu)
{
    uint32_t exc_bitmap;

    exc_bitmap = (1u << VECTOR_MC);
    if (vcpu->debug_control & (HAX_DEBUG_USE_HW_BP | HAX_DEBUG_STEP)) {
        exc_bitmap |= (1u << VECTOR_DB);
    }
    if (vcpu->debug_control & HAX_DEBUG_USE_SW_BP) {
        exc_bitmap |= (1u << VECTOR_BP);
    }
    vmwrite(vcpu, VMX_EXCEPTION_BITMAP, exc_bitmap);
}

static void fill_common_vmcs(struct vcpu_t *vcpu)
{
    uint32_t pin_ctls;
    uint32_t pcpu_ctls;
    uint32_t scpu_ctls;
    uint32_t exit_ctls = 0;
    uint32_t entry_ctls;
    uint32_t vmcs_err = 0;
    uint i;
    preempt_flag flags;
    struct per_cpu_data *cpu_data;

    // How to determine the capability
    pin_ctls = EXT_INTERRUPT_EXITING | NMI_EXITING;

    pcpu_ctls = IO_BITMAP_ACTIVE | MSR_BITMAP_ACTIVE | DR_EXITING |
                INTERRUPT_WINDOW_EXITING | USE_TSC_OFFSETTING | HLT_EXITING |
                CR8_LOAD_EXITING | CR8_STORE_EXITING | SECONDARY_CONTROLS;

    scpu_ctls = ENABLE_EPT;

    // Make the RDTSCP instruction available to the guest if the host supports
    // it (cf. Intel SDM Vol. 3C Table 24-7, bit 3: Enable RDTSCP)
    if (cpu_has_feature(X86_FEATURE_RDTSCP)) {
        // TODO: Check VMX capabilities to ensure ENABLE_RDTSCP is available
        scpu_ctls |= ENABLE_RDTSCP;
    }

    // If UG exists, we want it.
    if (hax->ug_enable_flag) {
        scpu_ctls |= UNRESTRICTED_GUEST;
    }

    // If vpid exists, we want it.
    if ((ia32_rdmsr(IA32_VMX_PROCBASED_CTLS) &
        ((uint64_t)SECONDARY_CONTROLS << 32)) != 0) {
        if ((ia32_rdmsr(IA32_VMX_SECONDARY_CTLS) &
            ((uint64_t)ENABLE_VPID << 32)) != 0) {
            if (0 != vcpu->vpid) {
                scpu_ctls |= ENABLE_VPID;
                vmwrite(vcpu, VMX_VPID, vcpu->vpid);
            }
        }
    }

#ifdef HAX_ARCH_X86_64
    exit_ctls = EXIT_CONTROL_HOST_ADDR_SPACE_SIZE | EXIT_CONTROL_LOAD_EFER |
                EXIT_CONTROL_SAVE_DEBUG_CONTROLS | EXIT_CONTROL_LOAD_PAT;
#endif

#ifdef HAX_ARCH_X86_32
    if (is_compatible()) {
        exit_ctls = EXIT_CONTROL_HOST_ADDR_SPACE_SIZE | EXIT_CONTROL_LOAD_EFER |
                    EXIT_CONTROL_SAVE_DEBUG_CONTROLS | EXIT_CONTROL_LOAD_PAT;
    } else {
        exit_ctls = EXIT_CONTROL_SAVE_DEBUG_CONTROLS | EXIT_CONTROL_LOAD_PAT;
    }
#endif

    entry_ctls = ENTRY_CONTROL_LOAD_DEBUG_CONTROLS;

    if ((vmcs_err = load_vmcs(vcpu, &flags))) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "load_vmcs failed while vcpu_prepare: %x",
                vmcs_err);
        hax_panic_log(vcpu);
        return;
    }
    cpu_data = current_cpu_data();

    // Initialize host area
    vmwrite(vcpu, HOST_CR0, get_cr0());
    vmwrite(vcpu, HOST_CR3, get_cr3());
    vmwrite(vcpu, HOST_CR4, get_cr4());

#ifdef HAX_ARCH_X86_64
    vmwrite(vcpu, HOST_CS_SELECTOR, get_kernel_cs());
#else
    if (is_compatible()) {
        vmwrite(vcpu, HOST_CS_SELECTOR, HAX_KERNEL64_CS);
    } else {
        vmwrite(vcpu, HOST_CS_SELECTOR, get_kernel_cs());
    }
#endif
    vmwrite(vcpu, HOST_SS_SELECTOR, get_kernel_ss());
    vmwrite(vcpu, HOST_DS_SELECTOR, get_kernel_ds() & 0xfff8);
    vmwrite(vcpu, HOST_ES_SELECTOR, get_kernel_es() & 0xfff8);
    vmwrite(vcpu, HOST_FS_SELECTOR, get_kernel_fs());
    vmwrite(vcpu, HOST_GS_SELECTOR, get_kernel_gs());

#ifdef HAX_ARCH_X86_64
    vmwrite(vcpu, HOST_FS_BASE, ia32_rdmsr(IA32_FS_BASE));
    vmwrite(vcpu, HOST_GS_BASE, ia32_rdmsr(IA32_GS_BASE));
#else
    if (is_compatible()) {
        vmwrite(vcpu, HOST_FS_BASE, ia32_rdmsr(IA32_FS_BASE));
        vmwrite(vcpu, HOST_GS_BASE, ia32_rdmsr(IA32_GS_BASE));
    } else {
        vmwrite(vcpu, HOST_FS_BASE, 0);
        vmwrite(vcpu, HOST_GS_BASE, 0);
    }
#endif
    vmwrite(vcpu, HOST_TR_SELECTOR, get_kernel_tr_selector());
    vmwrite(vcpu, HOST_TR_BASE, get_kernel_tr_base());
    vmwrite(vcpu, HOST_GDTR_BASE, get_kernel_gdtr_base());
    vmwrite(vcpu, HOST_IDTR_BASE, get_kernel_idtr_base());

#define WRITE_CONTROLS(vcpu, f, v) {                                        \
    uint32_t g = (v & cpu_data->vmx_info.v##_1) | cpu_data->vmx_info.v##_0; \
    vmwrite(vcpu, f, v = g);                                                \
}

    WRITE_CONTROLS(vcpu, VMX_PIN_CONTROLS, pin_ctls);
    WRITE_CONTROLS(vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS, pcpu_ctls);
    if (pcpu_ctls & SECONDARY_CONTROLS) {
        WRITE_CONTROLS(vcpu, VMX_SECONDARY_PROCESSOR_CONTROLS, scpu_ctls);
    }

    vcpu_update_exception_bitmap(vcpu);

    WRITE_CONTROLS(vcpu, VMX_EXIT_CONTROLS, exit_ctls);

    // Check if we can write HOST_EFER
    if (exit_ctls & EXIT_CONTROL_LOAD_EFER) {
        vmwrite(vcpu, HOST_EFER, ia32_rdmsr(IA32_EFER));
    }

    vmwrite(vcpu, HOST_PAT, ia32_rdmsr(IA32_CR_PAT));
    vmwrite(vcpu, GUEST_PAT, vcpu->cr_pat);

    WRITE_CONTROLS(vcpu, VMX_ENTRY_CONTROLS, entry_ctls);

    vmwrite(vcpu, VMX_PAGE_FAULT_ERROR_CODE_MASK, 0);
    vmwrite(vcpu, VMX_PAGE_FAULT_ERROR_CODE_MATCH, 0);
    vmwrite(vcpu, VMX_EXIT_MSR_STORE_COUNT, 0);
    vmwrite(vcpu, VMX_EXIT_MSR_STORE_ADDRESS, 0);

    vmwrite(vcpu, VMX_EXIT_MSR_LOAD_COUNT, 0);
    vmwrite(vcpu, VMX_EXIT_MSR_LOAD_ADDRESS, 0);

    vmwrite(vcpu, VMX_ENTRY_INTERRUPT_INFO, 0);
    // vmwrite(NULL, VMX_ENTRY_EXCEPTION_ERROR_CODE, 0);
    vmwrite(vcpu, VMX_ENTRY_MSR_LOAD_COUNT, 0);
    vmwrite(vcpu, VMX_ENTRY_MSR_LOAD_ADDRESS, 0);
    vmwrite(vcpu, VMX_ENTRY_INSTRUCTION_LENGTH, 0);

    // vmwrite(NULL, VMX_TPR_THRESHOLD, 0);
    vmwrite(vcpu, VMX_CR3_TARGET_COUNT, 0);

    for (i = 0; i < cpu_data->vmx_info._max_cr3_targets; i++) {
        vmwrite(vcpu, (component_index_t)(VMX_CR3_TARGET_VAL_BASE + i * 2), 0);
    }
    vmwrite(vcpu, GUEST_DR7, DR7_SETBITS);
    vmwrite(vcpu, GUEST_PENDING_DBE, 0);

    vmwrite(vcpu, GUEST_VMCS_LINK_PTR, ~0ULL);
    vmwrite(vcpu, GUEST_INTERRUPTIBILITY, 0);

    load_vmcs_common(vcpu);

    if ((vmcs_err = put_vmcs(vcpu, &flags))) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "put_vmcs() failed in vcpu_prepare, %x\n",
                vmcs_err);
        hax_panic_log(vcpu);
    }
}

static void vcpu_prepare(struct vcpu_t *vcpu)
{
    hax_log(HAX_LOGD, "vcpu_prepare current %x, CPU %x\n", vcpu->vcpu_id,
            hax_cpu_id());
    hax_mutex_lock(vcpu->tmutex);
    fill_common_vmcs(vcpu);
    hax_mutex_unlock(vcpu->tmutex);
}

static void vcpu_exit_fpu_state(struct vcpu_t *vcpu);

void vcpu_load_host_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    set_kernel_gdt(&hstate->host_gdtr);
    set_kernel_idt(&hstate->host_idtr);

    // Should be called when lock is got
    vcpu->state->_cr2 = get_cr2();
    set_kernel_ldt(hstate->ldt_selector);
    if (hstate->seg_valid & HOST_SEG_VALID_ES) {
        set_kernel_es(hstate->es);
    }

    if (hstate->seg_valid & HOST_SEG_VALID_DS) {
        set_kernel_ds(hstate->ds);
    }

    if (hstate->seg_valid & HOST_SEG_VALID_GS) {
        /*
         * If the GS entry is cleared in LDT, fake the entry contents with the
         * recorded value and clear the entry after we restore the GS selector.
         */
        if (hstate->seg_not_present & HOST_SEG_NOT_PRESENT_GS) {
            fake_seg_gs_entry(hstate);
        } else {
            set_kernel_gs(hstate->gs);
            ia32_wrmsr(IA32_GS_BASE, hstate->gs_base);
        }
    }

    if (hstate->seg_valid & HOST_SEG_VALID_FS) {
        set_kernel_fs(hstate->fs);
        ia32_wrmsr(IA32_FS_BASE, hstate->fs_base);
    }
    hstate->seg_valid = 0;
    hstate->seg_not_present = 0;

    load_host_msr(vcpu);
    set_cr2(hstate->hcr2);

    load_host_dr(vcpu);

    vcpu_exit_fpu_state(vcpu);
}

void vcpu_save_guest_state(struct vcpu_t *vcpu)
{
    save_guest_msr(vcpu);

    save_guest_dr(vcpu);
}

void vcpu_load_guest_state(struct vcpu_t *vcpu)
{
    load_guest_msr(vcpu);

    load_guest_dr(vcpu);

    load_dirty_vmcs_fields(vcpu);
}

/*
 * Copies bits 0, 1, 2, ..., (|size| * 8 - 1) of |src| to the same positions
 * in the 64-bit buffer pointed to by |pdst|, and clears bits (|size| * 8)
 * through 63 of the destination buffer.
 * |size| is the number of bytes to copy, and must be one of {1, 2, 4, 8}.
 * Returns 0 on success, -1 if |size| is invalid.
 */
static int read_low_bits(uint64_t *pdst, uint64_t src, uint8_t size)
{
    // Assume little-endian
    switch (size) {
        case 1: {
            *pdst = (uint8_t)src;
            break;
        }
        case 2: {
            *pdst = (uint16_t)src;
            break;
        }
        case 4: {
            *pdst = (uint32_t)src;
            break;
        }
        case 8: {
            *pdst = src;
            break;
        }
        default: {
            // Should never happen
            hax_log(HAX_LOGE, "read_low_bits: Invalid size %u\n", size);
            return -1;
        }
    }
    return 0;
}

/*
 * Copies bits 0, 1, 2, ..., (|size| * 8 - 1) of |src| to the same positions
 * in the 64-bit buffer pointed to by |pdst|, while keeping bits (|size| * 8)
 * through 63 of the destination buffer unchanged.
 * |size| is the number of bytes to copy, and must be one of {1, 2, 4, 8}.
 * Returns 0 on success, -1 if |size| is invalid.
 */
static int write_low_bits(uint64_t *pdst, uint64_t src, uint8_t size)
{
    switch (size) {
        case 1: {
            *((uint8_t *)pdst) = (uint8_t)src;
            break;
        }
        case 2: {
            *((uint16_t *)pdst) = (uint16_t)src;
            break;
        }
        case 4: {
            *((uint32_t *)pdst) = (uint32_t)src;
            break;
        }
        case 8: {
            *pdst = src;
            break;
        }
        default: {
            // Should never happen
            hax_log(HAX_LOGE, "write_low_bits: Invalid size %u\n", size);
            return -1;
        }
    }
    return 0;
}

static void handle_io_post(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    int size;
    struct vcpu_state_t *state = vcpu->state;

    if (htun->io._direction == HAX_IO_OUT)
        return;

    if (htun->io._flags == 1) {
        size = htun->io._count * htun->io._size;
        if (!vcpu_write_guest_virtual(vcpu, htun->io._vaddr, IOS_MAX_BUFFER,
                                      (void *)vcpu->io_buf, size, 0)) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "Unexpected page fault, kill the VM!\n");
            dump_vmcs(vcpu);
        }
    } else {
        switch (htun->io._size) {
            case 1: {
                state->_al = *((uint8_t *)vcpu->io_buf);
                break;
            }
            case 2: {
                state->_ax = *((uint16_t *)vcpu->io_buf);
                break;
            }
            case 4: {
                state->_eax = *((uint32_t *)vcpu->io_buf);
                break;
            }
            default: {
                break;
            }
        }
    }
}

int vcpu_execute(struct vcpu_t *vcpu)
{
    struct hax_tunnel *htun = vcpu->tunnel;
    struct em_context_t *em_ctxt = &vcpu->emulate_ctxt;
    em_status_t rc;
    int err = 0;

    hax_mutex_lock(vcpu->tmutex);
    hax_log(HAX_LOGD, "vcpu begin to run....\n");
    // QEMU will do realmode stuff for us
    if (!hax->ug_enable_flag && !(vcpu->state->_cr0 & CR0_PE)) {
        htun->_exit_reason = 0;
        htun->_exit_status = HAX_EXIT_REALMODE;
        hax_log(HAX_LOGD, "Guest is in realmode.\n");
        goto out;
    }
    hax_log(HAX_LOGD, "vcpu begin to run....in PE\n");

    if (htun->_exit_status == HAX_EXIT_IO) {
        handle_io_post(vcpu, htun);
    }
    // Continue until emulation finishes
    if (!em_ctxt->finished) {
        rc = em_emulate_insn(em_ctxt);
        if (rc < 0) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "%s: em_emulate_insn() failed: vcpu_id=%u\n",
                    __func__, vcpu->vcpu_id);
            err = HAX_RESUME;
            goto out;
        }
        if (rc > 0) {
            err = HAX_EXIT;
            goto out;
        }
    }

    // Check if Qemu pauses VM
    if (htun->_exit_reason == HAX_EXIT_PAUSED) {
        htun->_exit_status = HAX_EXIT_PAUSED;
        hax_log(HAX_LOGD, "vcpu paused\n");
        goto out;
    }

    err = cpu_vmx_execute(vcpu, htun);
    vcpu_is_panic(vcpu);
out:
    if (err) {
        vcpu->cur_state = GS_STALE;
        vcpu_vmread_all(vcpu);
        vcpu_is_panic(vcpu);
    }
    htun->apic_base = vcpu->gstate.apic_base;
    hax_mutex_unlock(vcpu->tmutex);

    return err;
}

// This function must be protected by _tmutex
int vcpu_vmexit_handler(struct vcpu_t *vcpu, exit_reason_t exit_reason,
                        struct hax_tunnel *htun)
{
    uint basic_reason = exit_reason.basic_reason;
    int ret = 0;
    vmx(vcpu, exit_reason) = exit_reason;
    vcpu->event_injected = 0;

    if (basic_reason < nr_handlers && handler_funcs[basic_reason] != NULL) {
        ret = handler_funcs[basic_reason](vcpu, htun);
    } else {
        ret = null_handler(vcpu, htun);
    }
    return ret;
}

int vtlb_active(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    struct per_cpu_data *cpu_data = current_cpu_data();

    if (hax->ug_enable_flag)
        return 0;

    hax_log(HAX_LOGD, "vtlb active: cr0, %llx\n", state->_cr0);
    if ((state->_cr0 & CR0_PG) == 0)
        return 1;

    if (config.disable_ept)
        return 1;

    if (!cpu_data->vmx_info._ept_cap)
        return 1;

    return 0;
}

static void advance_rip(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32_t interruptibility = vmx(vcpu, interruptibility_state).raw;
    uint32_t intr_blocking = 0;

    intr_blocking |= GUEST_INTRSTAT_STI_BLOCKING;
    intr_blocking |= GUEST_INTRSTAT_SS_BLOCKING;
    if (interruptibility & intr_blocking) {
        interruptibility &= ~intr_blocking;
        vmx(vcpu, interruptibility_state).raw = interruptibility;
        vcpu->interruptibility_dirty = 1;
    }

    state->_rip += vmx(vcpu, exit_instr_length);
    vcpu->rip_dirty = 1;
}

void vcpu_vmread_all(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32_t vmcs_err = 0;

    if (vcpu->cur_state == GS_STALE) {
        preempt_flag flags;

        // CRs were already updated
        // TODO: Always read RIP, RFLAGs, maybe reduce them in future!
        if ((vmcs_err = load_vmcs(vcpu, &flags))) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "load_vmcs failed while "
                    "vcpu_vmread_all: %x\n", vmcs_err);
            hax_panic_log(vcpu);
            return;
        }

        if (!vcpu->rip_dirty)
            state->_rip = vmread(vcpu, GUEST_RIP);

        if (!vcpu->rflags_dirty)
            state->_rflags = vmread(vcpu, GUEST_RFLAGS);

        state->_rsp = vmread(vcpu, GUEST_RSP);

        VMREAD_SEG(vcpu, CS, state->_cs);
        VMREAD_SEG(vcpu, DS, state->_ds);
        VMREAD_SEG(vcpu, ES, state->_es);
        if (!vcpu->fs_base_dirty)
            VMREAD_SEG(vcpu, FS, state->_fs);
        VMREAD_SEG(vcpu, GS, state->_gs);
        VMREAD_SEG(vcpu, SS, state->_ss);
        VMREAD_SEG(vcpu, LDTR, state->_ldt);
        VMREAD_SEG(vcpu, TR, state->_tr);
        VMREAD_DESC(vcpu, GDTR, state->_gdt);
        VMREAD_DESC(vcpu, IDTR, state->_idt);
        vmx(vcpu, interruptibility_state).raw = vmread(
                vcpu, GUEST_INTERRUPTIBILITY);
        if ((vmcs_err = put_vmcs(vcpu, &flags))) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "put_vmcs failed while vcpu_vmread_all: %x\n",
                    vmcs_err);
            hax_panic_log(vcpu);
        }

        vcpu->cur_state = GS_VALID;
    }
}

void vcpu_vmwrite_all(struct vcpu_t *vcpu, int force_tlb_flush)
{
    struct vcpu_state_t *state = vcpu->state;

    vmwrite(vcpu, GUEST_RIP, state->_rip);
    vcpu->rip_dirty = 0;
    vmwrite(vcpu, GUEST_RFLAGS, state->_rflags);
    vcpu->rflags_dirty = 0;
    vmwrite(vcpu, GUEST_RSP, state->_rsp);

    VMWRITE_SEG(vcpu, CS, state->_cs);
    VMWRITE_SEG(vcpu, DS, state->_ds);
    VMWRITE_SEG(vcpu, ES, state->_es);
    VMWRITE_SEG(vcpu, FS, state->_fs);
    vcpu->fs_base_dirty = 0;
    VMWRITE_SEG(vcpu, GS, state->_gs);
    VMWRITE_SEG(vcpu, SS, state->_ss);
    VMWRITE_SEG(vcpu, LDTR, state->_ldt);
    VMWRITE_SEG(vcpu, TR, state->_tr);
    VMWRITE_DESC(vcpu, GDTR, state->_gdt);
    VMWRITE_DESC(vcpu, IDTR, state->_idt);

    vmwrite(vcpu, GUEST_INTERRUPTIBILITY,
            vmx(vcpu, interruptibility_state).raw);

    vmwrite_cr(vcpu);

    if (force_tlb_flush) {
        vcpu_invalidate_tlb(vcpu, 1);
    }
}

// Prepares the values (4 GPAs) to be loaded into VMCS fields PDPTE{0..3}.
// The caller must ensure the following conditions are met:
// a) The guest is running in EPT mode (see IASDM Vol. 3C 26.3.2.4), and
// b) Preemption is enabled for the current CPU.
// Returns 0 on success, < 0 on error.
static int vcpu_prepare_pae_pdpt(struct vcpu_t *vcpu)
{
    uint64_t cr3 = vcpu->state->_cr3;
    int pdpt_size = (int)sizeof(vcpu->pae_pdptes);
    // CR3 is the GPA of the page-directory-pointer table. According to IASDM
    // Vol. 3A 4.4.1, Table 4-7, bits 63..32 and 4..0 of this GPA are ignored.
    uint64_t gpa = cr3 & 0xffffffe0;
    int ret;

    // On Mac, the following call may somehow cause the XNU kernel to preempt
    // the current process (QEMU), even if preemption has been previously
    // disabled via hax_disable_preemption() (which is implemented on Mac by
    // simply disabling IRQs). Therefore, it is not safe to call this function
    // with preemption disabled.
    ret = gpa_space_read_data(&vcpu->vm->gpa_space, gpa, pdpt_size,
                              (uint8_t *)vcpu->pae_pdptes);
    // The PAE PDPT cannot span two page frames
    if (ret != pdpt_size) {
        hax_log(HAX_LOGE, "%s: Failed to read PAE PDPT: cr3=0x%llx, ret=%d\n",
                __func__, cr3, ret);
        return ret < 0 ? ret : -EIO;
    }
    vcpu->pae_pdpt_dirty = 1;
    return 0;
}

static void vmwrite_cr(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    struct per_cpu_data *cpu = current_cpu_data();

    uint32_t entry_ctls = vmx(vcpu, entry_ctls);
    uint32_t pcpu_ctls = vmx(vcpu, pcpu_ctls_base);
    uint32_t scpu_ctls = vmx(vcpu, scpu_ctls_base);
    uint32_t exc_bitmap = vmx(vcpu, exc_bitmap_base);
    uint64_t eptp;

    // If a bit is set here, the same bit of guest CR0 must be fixed to 1 (see
    // IASDM Vol. 3D A.7)
    uint64_t cr0_fixed_0 = cpu->vmx_info._cr0_fixed_0;
    // If a bit is clear here, the same bit of guest CR0 must be fixed to 0 (see
    // IASDM Vol. 3D A.7)
    uint64_t cr0_fixed_1 = cpu->vmx_info._cr0_fixed_1;

    uint64_t cr0 = (state->_cr0 & cr0_fixed_1) |
                 (cr0_fixed_0 & (uint64_t)~(CR0_PE | CR0_PG));

    uint64_t cr0_mask;
    uint64_t cr0_shadow;

    // If a bit is set here, the same bit of guest CR4 must be fixed to 1 (see
    // IASDM Vol. 3D A.8)
    uint64_t cr4_fixed_0 = cpu->vmx_info._cr4_fixed_0;
    // If a bit is clear here, the same bit of guest CR4 must be fixed to 0 (see
    // IASDM Vol. 3D A.8)
    uint64_t cr4_fixed_1 = cpu->vmx_info._cr4_fixed_1;

    uint64_t cr4 = ((state->_cr4 | CR4_MCE) & cr4_fixed_1) | cr4_fixed_0;
    uint64_t cr4_mask = vmx(vcpu, cr4_mask) | ~(cr4_fixed_0 ^ cr4_fixed_1) |
                      CR4_VMXE | CR4_SMXE | CR4_MCE;
    uint64_t cr4_shadow;

    if (hax->ug_enable_flag) {
        // In UG mode, we can allow the guest to freely modify CR0.PE without
        // causing VM exits (see IASDM Vol. 3C 25.3). If the modification
        // results in an invalid CR0 value (e.g. CR0.PE = 0 and CR0.PG = 1),
        // hardware will invoke the guest #GP handler.
        // Note: The XOR below produces a bit mask where each 0 identifies a
        // fixed bit of CR0, and each 1 a free bit of CR0 (i.e. that the guest
        // can freely modify).
        cr0_mask = vmx(vcpu, cr0_mask) | CR0_CD | CR0_NW |
                   ~((cr0_fixed_0 ^ cr0_fixed_1) | CR0_PE);
    } else {
        cr0_mask = vmx(vcpu, cr0_mask) | CR0_CD | CR0_NW | CR0_PE | CR0_PG |
                   ~(cr0_fixed_0 ^ cr0_fixed_1);
    }

    if (vtlb_active(vcpu)) {
        hax_log(HAX_LOGD, "vTLB mode, cr0 %llx\n", vcpu->state->_cr0);
        vcpu->mmu->mmu_mode = MMU_MODE_VTLB;
        exc_bitmap |= 1u << VECTOR_PF;
        cr0 |= CR0_WP;
        cr0_mask |= CR0_WP;
        cr4 |= CR4_PGE | CR4_PAE;
        cr4_mask |= CR4_PGE | CR4_PAE | CR4_PSE;
        pcpu_ctls |= CR3_LOAD_EXITING | CR3_STORE_EXITING | INVLPG_EXITING;
        scpu_ctls &= ~ENABLE_EPT;

        vmwrite(vcpu, GUEST_CR3, vtlb_get_cr3(vcpu));
        state->_efer = 0;
    } else {  // EPTE
        vcpu->mmu->mmu_mode = MMU_MODE_EPT;
        // In EPT mode, we need to monitor guest writes to CR.PAE, so that we
        // know when it wants to enter PAE paging mode (see IASDM Vol. 3A 4.1.2,
        // Figure 4-1, as well as vcpu_prepare_pae_pdpt() and its caller).
        // TODO: Monitor guest writes to CR4.{PGE, PSE, SMEP} as well (see IASDM
        // Vol. 3A 4.4.1)
        cr4_mask |= CR4_PAE;
        eptp = vm_get_eptp(vcpu->vm);
        hax_assert(eptp != INVALID_EPTP);
        // hax_log(HAX_LOGD, "Guest eip:%llx, EPT mode, eptp:%llx\n",
        //         vcpu->state->_rip, eptp);
        vmwrite(vcpu, GUEST_CR3, state->_cr3);
        scpu_ctls |= ENABLE_EPT;
        if (vcpu->pae_pdpt_dirty) {
            // vcpu_prepare_pae_pdpt() has updated vcpu->pae_pdptes
            // Note that because we do not monitor guest writes to CR3, the only
            // case where vcpu->pae_pdptes is newer than VMCS GUEST_PDPTE{0..3}
            // is following a guest write to CR0 or CR4 that requires PDPTEs to
            // be reloaded, i.e. the pae_pdpt_dirty case. When the guest is in
            // PAE paging mode but !pae_pdpt_dirty, VMCS GUEST_PDPTE{0..3} are
            // already up-to-date following each VM exit (see Intel SDM Vol. 3C
            // 27.3.4), and we must not overwrite them with our cached values
            // (vcpu->pae_pdptes), which may be outdated.
            vmwrite(vcpu, GUEST_PDPTE0, vcpu->pae_pdptes[0]);
            vmwrite(vcpu, GUEST_PDPTE1, vcpu->pae_pdptes[1]);
            vmwrite(vcpu, GUEST_PDPTE2, vcpu->pae_pdptes[2]);
            vmwrite(vcpu, GUEST_PDPTE3, vcpu->pae_pdptes[3]);
            vcpu->pae_pdpt_dirty = 0;
        }
        vmwrite(vcpu, VMX_EPTP, eptp);
        // pcpu_ctls |= RDTSC_EXITING;
    }

    vmwrite(vcpu, GUEST_CR0, cr0);
    vmwrite(vcpu, VMX_CR0_MASK, cr0_mask);
    hax_log(HAX_LOGD, "vmwrite_cr cr0 %llx, cr0_mask %llx\n", cr0, cr0_mask);
    cr0_shadow = (state->_cr0 & ~vmx(vcpu, cr0_mask)) |
                 (vmx(vcpu, cr0_shadow) & vmx(vcpu, cr0_mask));
    vmwrite(vcpu, VMX_CR0_READ_SHADOW, cr0_shadow);

    vmwrite(vcpu, GUEST_CR4, cr4);
    vmwrite(vcpu, VMX_CR4_MASK, cr4_mask);
    cr4_shadow = (state->_cr4 & ~vmx(vcpu, cr4_mask)) |
                 (vmx(vcpu, cr4_shadow) & vmx(vcpu, cr4_mask));
    vmwrite(vcpu, VMX_CR4_READ_SHADOW, cr4_shadow);

    if (!(state->_cr4 & CR4_PAE) && (state->_cr0 & CR0_PG)) {
        state->_efer = 0;
    }

    if ((state->_cr4 & CR4_PAE) && (state->_cr0 & CR0_PG) &&
        (state->_cr0 & CR0_PE)) {
        entry_ctls |= ENTRY_CONTROL_LOAD_EFER;
    } else {
        entry_ctls &= ~ENTRY_CONTROL_LOAD_EFER;
    }

    entry_ctls |= ENTRY_CONTROL_LOAD_PAT;

    if (pcpu_ctls != vmx(vcpu, pcpu_ctls)) {
        vmx(vcpu, pcpu_ctls) = pcpu_ctls;
        vcpu->pcpu_ctls_dirty = 1;
    }
    if (scpu_ctls != vmx(vcpu, scpu_ctls)) {
        vmwrite(vcpu, VMX_SECONDARY_PROCESSOR_CONTROLS,
                vmx(vcpu, scpu_ctls) = scpu_ctls);
    }
    if (exc_bitmap != vmx(vcpu, exc_bitmap)) {
        vmwrite(vcpu, VMX_EXCEPTION_BITMAP, vmx(vcpu, exc_bitmap) = exc_bitmap);
    }
    if (entry_ctls != vmx(vcpu, entry_ctls)) {
        vmwrite(vcpu, VMX_ENTRY_CONTROLS, vmx(vcpu, entry_ctls) = entry_ctls);
    }

    vmwrite_efer(vcpu);
}

static void vcpu_enter_fpu_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    struct gstate *gstate = &vcpu->gstate;
    struct fx_layout *hfx = (struct fx_layout *)hax_page_va(hstate->hfxpage);
    struct fx_layout *gfx = (struct fx_layout *)hax_page_va(gstate->gfxpage);

    hstate->cr0_ts = !!(get_cr0() & CR0_TS);

    // Before executing any FPU instruction (e.g. FXSAVE) in host kernel
    // context, make sure host CR0.TS = 0, so as to prevent a Device Not Found
    // (#NM) exception, which can result in a host kernel panic on NetBSD.
    hax_clts();

    hax_fxsave((mword *)hfx);
    hax_fxrstor((mword *)gfx);
}

static void vcpu_exit_fpu_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    struct gstate *gstate = &vcpu->gstate;
    struct fx_layout *hfx = (struct fx_layout *)hax_page_va(hstate->hfxpage);
    struct fx_layout *gfx = (struct fx_layout *)hax_page_va(gstate->gfxpage);

    hax_clts();

    hax_fxsave((mword *)gfx);
    hax_fxrstor((mword *)hfx);

    if (hstate->cr0_ts) {
        set_cr0(get_cr0() | CR0_TS);
    }
}

// Instructions are never longer than 15 bytes:
//   http://wiki.osdev.org/X86-64_Instruction_Encoding
#define INSTR_MAX_LEN               15

static bool qemu_support_fastmmio(struct vcpu_t *vcpu)
{
    struct vm_t *vm = vcpu->vm;

    return vm->features & VM_FEATURES_FASTMMIO_BASIC;
}

static bool qemu_support_fastmmio_extra(struct vcpu_t *vcpu)
{
    struct vm_t *vm = vcpu->vm;

    return vm->features & VM_FEATURES_FASTMMIO_EXTRA;
}

static bool is_mmio_address(struct vcpu_t *vcpu, hax_paddr_t gpa)
{
    hax_paddr_t hpa;
    if (vtlb_active(vcpu)) {
        hpa = hax_gpfn_to_hpa(vcpu->vm, gpa >> HAX_PAGE_SHIFT);
        // hax_gpfn_to_hpa() assumes hpa == 0 is invalid
        return !hpa;
    }

    hax_memslot *slot = memslot_find(&vcpu->vm->gpa_space, gpa >> PG_ORDER_4K);

    return !slot;
}

static int vcpu_emulate_insn(struct vcpu_t *vcpu)
{
    em_status_t rc;
    em_mode_t mode;
    em_context_t *em_ctxt = &vcpu->emulate_ctxt;
    uint8_t instr[INSTR_MAX_LEN] = {0};
    uint64_t cs_base = vcpu->state->_cs.base;
    uint64_t rip = vcpu->state->_rip;
    uint64_t va;

    // Clean up the emulation context of the previous MMIO instruction, so that
    // even if things go wrong, the behavior will still be predictable.
    vcpu_init_emulator(vcpu);

    // Detect guest mode
    if (!(vcpu->state->_cr0 & CR0_PE))
        mode = EM_MODE_REAL;
    else if (vcpu->state->_cs.long_mode == 1)
        mode = EM_MODE_PROT64;
    else if (vcpu->state->_cs.operand_size == 1)
        mode = EM_MODE_PROT32;
    else
        mode = EM_MODE_PROT16;
    em_ctxt->mode = mode;

    // Fetch the instruction at guest CS:IP = CS.Base + IP, omitting segment
    // limit and privilege checks
    va = (mode == EM_MODE_PROT64) ? rip : cs_base + rip;
    if (mmio_fetch_instruction(vcpu, va, instr, INSTR_MAX_LEN)) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "%s: mmio_fetch_instruction() failed: vcpu_id=%u,"
                " gva=0x%llx (CS:IP=0x%llx:0x%llx)\n",
                __func__, vcpu->vcpu_id, va, cs_base, rip);
        dump_vmcs(vcpu);
        return -1;
    }

    em_ctxt->rip = rip;
    rc = em_decode_insn(em_ctxt, instr);
    if (rc < 0) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "em_decode_insn() failed: vcpu_id=%u,"
                " len=%u, CS:IP=0x%llx:0x%llx, instr[0..5]="
                "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", vcpu->vcpu_id,
                vcpu->vmx.exit_instr_length, cs_base, rip, instr[0],
                instr[1], instr[2], instr[3], instr[4], instr[5]);
        dump_vmcs(vcpu);
        return HAX_RESUME;
    }
    if (em_ctxt->len != vcpu->vmx.exit_instr_length) {
        hax_log(HAX_LOGD, "Inferred instruction length %u does not match "
                "VM-exit instruction length %u (CS:IP=0x%llx:0x%llx, "
                "instr[0..5]=0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)\n", em_ctxt->len,
                vcpu->vmx.exit_instr_length, cs_base, rip, instr[0], instr[1],
                instr[2], instr[3], instr[4], instr[5]);
    }
    rc = em_emulate_insn(em_ctxt);
    if (rc < 0) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "em_emulate_insn() failed: vcpu_id=%u,"
                " len=%u, CS:IP=0x%llx:0x%llx, instr[0..5]="
                "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", vcpu->vcpu_id,
                vcpu->vmx.exit_instr_length, cs_base, rip, instr[0],
                instr[1], instr[2], instr[3], instr[4], instr[5]);
        dump_vmcs(vcpu);
        return HAX_RESUME;
    }
    return HAX_EXIT;
}

static uint64_t vcpu_read_gpr(void *obj, uint32_t reg_index)
{
    struct vcpu_t *vcpu = obj;
    if (reg_index >= 16) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "vcpu_read_gpr: Invalid register index\n");
        return 0;
    }
    return vcpu->state->_regs[reg_index];
}

static void vcpu_write_gpr(void *obj, uint32_t reg_index, uint64_t value)
{
    struct vcpu_t *vcpu = obj;
    if (reg_index >= 16) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "vcpu_write_gpr: Invalid register index\n");
        return;
    }
    vcpu->state->_regs[reg_index] = value;
}

uint64_t vcpu_read_rflags(void *obj)
{
    struct vcpu_t *vcpu = obj;
    return vcpu->state->_rflags;
}

void vcpu_write_rflags(void *obj, uint64_t value)
{
    struct vcpu_t *vcpu = obj;
    vcpu->state->_rflags = value;
    vcpu->rflags_dirty = 1;
}

static uint64_t vcpu_get_segment_base(void *obj, uint32_t segment)
{
    struct vcpu_t *vcpu = obj;
    switch (segment) {
    case SEG_CS:
        return vcpu->state->_cs.base;
    case SEG_DS:
        return vcpu->state->_ds.base;
    case SEG_ES:
        return vcpu->state->_es.base;
    case SEG_FS:
        return vcpu->state->_fs.base;
    case SEG_GS:
        return vcpu->state->_gs.base;
    case SEG_SS:
        return vcpu->state->_ss.base;
    default:
        return vcpu->state->_ds.base;
    }
}

static void vcpu_advance_rip(void *obj, uint64_t len)
{
    struct vcpu_t *vcpu = obj;
    advance_rip(vcpu);
}

static em_status_t vcpu_read_memory(void *obj, uint64_t ea, uint64_t *value,
                                    uint32_t size, uint32_t flags)
{
    struct vcpu_t *vcpu = obj;
    uint64_t pa;

    if (flags & EM_OPS_NO_TRANSLATION) {
        pa = vmx(vcpu, exit_gpa);
    } else {
        vcpu_translate(vcpu, ea, 0, &pa, NULL, false);
    }

    // Assume that instructions that don't require translation access MMIO
    if (flags & EM_OPS_NO_TRANSLATION || is_mmio_address(vcpu, pa)) {
        struct hax_tunnel *htun = vcpu->tunnel;
        struct hax_fastmmio *hft = (struct hax_fastmmio *)vcpu->io_buf;
        htun->_exit_status = HAX_EXIT_FAST_MMIO;
        hft->gpa = pa;
        hft->size = size;
        hft->direction = 0;
        return EM_EXIT_MMIO;
    } else {
        if (!vcpu_read_guest_virtual(vcpu, ea, value, size, size, 0)) {
            return EM_ERROR;
        }
        return EM_CONTINUE;
    }
}

static em_status_t vcpu_read_memory_post(void *obj,
                                         uint64_t *value, uint32_t size)
{
    struct vcpu_t *vcpu = obj;
    struct hax_fastmmio *hft = (struct hax_fastmmio *)vcpu->io_buf;
    memcpy(value, &hft->value, size);
    return EM_CONTINUE;
}

static em_status_t vcpu_write_memory(void *obj, uint64_t ea, uint64_t *value,
                                     uint32_t size, uint32_t flags)
{
    struct vcpu_t *vcpu = obj;
    uint64_t pa;

    if (flags & EM_OPS_NO_TRANSLATION) {
        pa = vmx(vcpu, exit_gpa);
    } else {
        vcpu_translate(vcpu, ea, 0, &pa, NULL, false);
    }

    // Assume that instructions that don't require translation access MMIO
    if (flags & EM_OPS_NO_TRANSLATION || is_mmio_address(vcpu, pa)) {
        struct hax_tunnel *htun = vcpu->tunnel;
        struct hax_fastmmio *hft = (struct hax_fastmmio *)vcpu->io_buf;
        htun->_exit_status = HAX_EXIT_FAST_MMIO;
        hft->gpa = pa;
        hft->size = size;
        hft->value = *value;
        hft->direction = 1;
        return EM_EXIT_MMIO;
    } else {
        if (!vcpu_write_guest_virtual(vcpu, ea, size, value, size, 0)) {
            return EM_ERROR;
        }
        return EM_CONTINUE;
    }
}

static const struct em_vcpu_ops_t em_ops = {
    .read_gpr = vcpu_read_gpr,
    .write_gpr = vcpu_write_gpr,
    .read_rflags = vcpu_read_rflags,
    .write_rflags = vcpu_write_rflags,
    .get_segment_base = vcpu_get_segment_base,
    .advance_rip = vcpu_advance_rip,
    .read_memory = vcpu_read_memory,
    .read_memory_post = vcpu_read_memory_post,
    .write_memory = vcpu_write_memory,
};

static void vcpu_init_emulator(struct vcpu_t *vcpu)
{
    struct em_context_t *em_ctxt = &vcpu->emulate_ctxt;

    memset(em_ctxt, 0, sizeof(*em_ctxt));
    em_ctxt->vcpu = vcpu;
    em_ctxt->ops = &em_ops;
    em_ctxt->finished = true;
}

static int exit_exc_nmi(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    interruption_info_t exit_intr_info;

    exit_intr_info.raw = vmx(vcpu, exit_intr_info).raw;
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    hax_log(HAX_LOGD, "exception vmexit vector:%x\n", exit_intr_info.vector);

    switch (exit_intr_info.vector) {
        case VECTOR_NMI: {
            __nmi();
            return HAX_RESUME;
        }
        case VECTOR_PF: {
            if (vtlb_active(vcpu)) {
                if (handle_vtlb(vcpu))
                    return HAX_RESUME;

                return vcpu_emulate_insn(vcpu);
            } else {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "Page fault shouldn't happen when EPT is"
                        " enabled.\n");
                dump_vmcs(vcpu);
            }
            break;
        }
        case VECTOR_MC: {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "Machine check happens!\n");
            dump_vmcs(vcpu);
            handle_machine_check(vcpu);
            break;
        }
        case VECTOR_DF: {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "Double fault!\n");
            dump_vmcs(vcpu);
            break;
        }
        case VECTOR_DB: {
            htun->_exit_status = HAX_EXIT_DEBUG;
            htun->debug.rip = vcpu->state->_rip;
            htun->debug.dr6 = vmx(vcpu, exit_qualification).raw;
            htun->debug.dr7 = vmread(vcpu, GUEST_DR7);
            return HAX_EXIT;
        }
        case VECTOR_BP: {
            htun->_exit_status = HAX_EXIT_DEBUG;
            htun->debug.rip = vcpu->state->_rip;
            htun->debug.dr6 = 0;
            htun->debug.dr7 = 0;
            return HAX_EXIT;
        }
    }

    if (exit_intr_info.vector == VECTOR_PF) {
        state->_cr2 = vmx(vcpu, exit_qualification.address);
    }

    return HAX_RESUME;
}

static void handle_machine_check(struct vcpu_t *vcpu)
{
    // Dump machine check MSRs
    uint64_t mcg_cap = ia32_rdmsr(IA32_MCG_CAP);
    uint n, i;

#define MSR_TRACE(msr) \
        hax_log(HAX_LOGD, "MSR %s (%x): %08llx\n", #msr, msr, ia32_rdmsr(msr))

    MSR_TRACE(IA32_MCG_CAP);
    MSR_TRACE(IA32_MCG_STATUS);
    if (mcg_cap & 0x100) {
        MSR_TRACE(IA32_MCG_CTL);
    }

#define MSR_TRACEi(n, a)                                               \
        hax_log(HAX_LOGD, "MSR IA32_MC%d_%s (%x): %08llx\n", i, #n, \
                a + i * 4, ia32_rdmsr(a + i * 4))

    n = mcg_cap & 0xff;
    for (i = 0; i < n; i++) {
        MSR_TRACEi(CTL, IA32_MC0_CTL);
        MSR_TRACEi(STATUS, IA32_MC0_STATUS);
        if (ia32_rdmsr(IA32_MC0_STATUS + i * 4) & ((uint64_t)1 << 58)) {
            MSR_TRACEi(ADDR, IA32_MC0_ADDR);
        }
        if (ia32_rdmsr(IA32_MC0_STATUS + i * 4) & ((uint64_t)1 << 59)) {
            MSR_TRACEi(MISC, IA32_MC0_MISC);
        }
    }

    // FIXME: IA32_MCG_MISC, etc. are no longer defined. Cf. commit 815056b
    // ("Fix btr() and bts()'s handling of large bit offsets").
//    if (mcg_cap & 0x200) {
//        MSR_TRACE(IA32_MCG_MISC);
//        MSR_TRACE(IA32_MCG_RIP);
//        MSR_TRACE(IA32_MCG_RFLAGS);
//        MSR_TRACE(IA32_MCG_RSP);
//        MSR_TRACE(IA32_MCG_RAX);
//        MSR_TRACE(IA32_MCG_RBX);
//        MSR_TRACE(IA32_MCG_RCX);
//        MSR_TRACE(IA32_MCG_RDX);
//        MSR_TRACE(IA32_MCG_RDI);
//        MSR_TRACE(IA32_MCG_RSI);
//        MSR_TRACE(IA32_MCG_RBP);
//    }

#undef MSR_TRACE
#undef MSR_TRACEi

    hax_log(HAX_LOGW, "Machine check");
}

static int exit_interrupt(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    // We don't use ACK_INTERRUPT exiting, and we call sti() before
    htun->_exit_status = HAX_EXIT_INTERRUPT;
    return HAX_EXIT;
}

static int exit_triple_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    vcpu_set_panic(vcpu);
    hax_log(HAX_LOGPANIC, "Triple fault\n");
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

static int exit_interrupt_window(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    vmx(vcpu, pcpu_ctls) &=
            vmx(vcpu, exit_reason).basic_reason == VMX_EXIT_PENDING_INTERRUPT
            ? ~INTERRUPT_WINDOW_EXITING : ~NMI_WINDOW_EXITING;

    vcpu->pcpu_ctls_dirty = 1;
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    return HAX_RESUME;
}

static int exit_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    handle_cpuid(vcpu, htun);
    advance_rip(vcpu);
    hax_log(HAX_LOGD, "...........exit_cpuid\n");
    return HAX_RESUME;
}

static void handle_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32_t a = state->_eax, c = state->_ecx;
    cpuid_args_t args;

    args.eax = state->_eax;
    args.ecx = state->_ecx;
    asm_cpuid(&args);
    state->_eax = args.eax;
    state->_ecx = args.ecx;
    state->_edx = args.edx;
    state->_ebx = args.ebx;

    handle_cpuid_virtual(vcpu, a, c);

    hax_log(HAX_LOGD, "CPUID %08x %08x: %08x %08x %08x %08x\n", a, c,
            state->_eax, state->_ebx, state->_ecx, state->_edx);
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
}

static void handle_cpuid_virtual(struct vcpu_t *vcpu, uint32_t a, uint32_t c)
{
#define VIRT_FAMILY     0x6
#define VIRT_MODEL      0x1F
#define VIRT_STEPPING   0x1
    struct vcpu_state_t *state = vcpu->state;
    uint32_t hw_family;
    uint32_t hw_model;
    uint8_t physical_address_size;
    uint32_t cpuid_1_features_ecx, cpuid_1_features_edx,
             cpuid_8000_0001_features_ecx, cpuid_8000_0001_features_edx;

    // To fully support CPUID instructions (opcode = 0F A2) by software, it is
    // recommended to add opcode_table_0FA2[] in core/emulate.c to emulate
    // (Refer to Intel SDM Vol. 2A 3.2 CPUID).
    cpuid_get_guest_features(vcpu->guest_cpuid, &cpuid_1_features_ecx,
                             &cpuid_1_features_edx,
                             &cpuid_8000_0001_features_ecx,
                             &cpuid_8000_0001_features_edx);

    switch (a) {
        case 0: {                       // Maximum Basic Information
            state->_eax = 0xa;
            return;
        }
        case 1: {                       // Version Information and Features
            /*
             * In order to avoid the initialization of unnecessary extended
             * features in the Kernel for emulator (such as the snbep
             * performance monitoring feature in Xeon E5 series system,
             * and the initialization of this feature crashes the emulator),
             * when the hardware family id is equal to 6 and hardware model id
             * is greater than 0x1f, we virtualize the returned eax to 0x106F1,
             * that is an old i7 system, so the emulator can still utilize the
             * enough extended features of the hardware, but doesn't crash.
             */
            union cpuid_1_eax {
                uint32_t raw;
                struct {
                    uint32_t steppingID    : 4;
                    uint32_t model         : 4;
                    uint32_t familyID      : 4;
                    uint32_t processorType : 2;
                    uint32_t reserved      : 2;
                    uint32_t extModelID    : 4;
                    uint32_t extFamilyID   : 8;
                    uint32_t reserved2     : 4;
                };
            } cpuid_eax;
            cpuid_eax.raw = state->_eax;

            if (0xF != cpuid_eax.familyID)
                hw_family = cpuid_eax.familyID;
            else
                hw_family = cpuid_eax.familyID + (cpuid_eax.extFamilyID << 4);

            if (0x6 == cpuid_eax.familyID || 0xF == cpuid_eax.familyID)
                hw_model = (cpuid_eax.extModelID << 4) + cpuid_eax.model;
            else
                hw_model = cpuid_eax.model;

            if (hw_family == VIRT_FAMILY && hw_model > VIRT_MODEL) {
                state->_eax = ((VIRT_FAMILY & 0xFF0) << 16) |
                              ((VIRT_FAMILY & 0xF) << 8) |
                              ((VIRT_MODEL & 0xF0) << 12) |
                              ((VIRT_MODEL & 0xF) << 4) |
                              (VIRT_STEPPING & 0xF);
            }

            /* Report all threads in one package XXXXX vapic currently, we
             * hardcode it to the maximal number of vcpus, but we should see
             * the code in QEMU to vapic initialization.
             */
            state->_ebx =
                    // Bits 31..16 are hard-coded, with the original author's
                    // reasoning given in the above comment. However, these
                    // values are not suitable for SMP guests.
                    // TODO: Use QEMU's values instead
                    // EBX[31..24]: Initial APIC ID
                    // EBX[23..16]: Maximum number of addressable IDs for
                    //              logical processors in this physical package
                    (0x01 << 16) |
                    // EBX[15..8]: CLFLUSH line size
                    // Report a 64-byte CLFLUSH line size as QEMU does
                    (0x08 << 8) |
                    // EBX[7..0]: Brand index
                    // 0 indicates that brand identification is not supported
                    // (see IA SDM Vol. 3A 3.2, Table 3-14)
                    0x00;

            // Report only the features specified, excluding any features not
            // supported by the host CPU, but including "hypervisor", which is
            // desirable for VMMs.
            // TBD: This will need to be changed to emulate new features.
            state->_ecx = (cpuid_1_features_ecx & state->_ecx) | FEATURE(HYPERVISOR);
            state->_edx = cpuid_1_features_edx & state->_edx;
            return;
        }
        case 2: {                       // Cache and TLB Information
            // These hard-coded values are questionable
            // TODO: Use QEMU's values instead
            state->_eax = 0x03020101;
            state->_ebx = 0;
            state->_ecx = 0;
            state->_edx = 0x0c040844;
            return;
        }
        case 3: {                       // Reserved
            state->_eax = state->_ebx = state->_ecx = state->_edx = 0;
            return;
        }
        case 4: {                       // Deterministic Cache Parameters
            // [31:26] cores per package - 1
            // Use host cache values.
            return;
        }
        case 5:                         // MONITOR/MWAIT
            // Unsupported because feat_monitor is not set
        case 6:                         // Thermal and Power Management
            // Unsupported
        case 7:                         // Structured Extended Feature Flags
            // Unsupported
            // Leaf 8 is undefined
        case 9: {                       // Direct Cache Access Information
            // Unsupported
            state->_eax = state->_ebx = state->_ecx = state->_edx = 0;
            return;
        }
        case 0xa: {                     // Architectural Performance Monitoring
            struct cpu_pmu_info *pmu_info = &hax->apm_cpuid_0xa;
            state->_eax = pmu_info->cpuid_eax;
            state->_ebx = pmu_info->cpuid_ebx;
            state->_ecx = 0;
            state->_edx = pmu_info->cpuid_edx;
            return;
        }
        case 0x40000000: {              // Unimplemented by real Intel CPUs
            // Most VMMs, including KVM, Xen, VMware and Hyper-V, use this
            // unofficial CPUID leaf, in conjunction with the "hypervisor"
            // feature flag (c.f. case 1 above), to identify themselves to the
            // guest OS, in a similar manner to CPUID leaf 0 for the CPU vendor
            // ID. HAXM should return its own VMM vendor ID, even though no
            // guest OS recognizes it, because it may be running as a guest VMM
            // on top of another VMM such as KVM or Hyper-V, in which case EBX,
            // ECX and EDX represent the underlying VMM's vendor ID and should
            // be overridden.
            static const char vmm_vendor_id[13] = "HAXMHAXMHAXM";
            const uint32_t *p = (const uint32_t *)vmm_vendor_id;
            // Some VMMs use EAX to indicate the maximum CPUID leaf valid for
            // the range of [0x40000000, 0x4fffffff]
            state->_eax = 0x40000000;
            state->_ebx = *p;
            state->_ecx = *(p + 1);
            state->_edx = *(p + 2);
            return;
        }
        case 0x80000000: {              // Maximum Extended Information
            state->_eax = 0x80000008;
            state->_ebx = state->_ecx = state->_edx = 0;
            return;
        }
        case 0x80000001: {              // Extended Signature and Features
            state->_eax = state->_ebx = 0;
            // Report only the features specified but turn off any features
            // this processor doesn't support.
            state->_ecx = cpuid_8000_0001_features_ecx & state->_ecx;
            state->_edx = cpuid_8000_0001_features_edx & state->_edx;
            return;
        }
        /*
         * Hard-coded following three Processor Brand String functions
         * (0x80000002, 0x80000003, and 0x80000004) to report "Virtual CPU" at
         * the middle of the CPU info string in the Kernel to indicate that the
         * system is virtualized to run the emulator.
         */
        case 0x80000002: {              // Processor Brand String - part 1
            state->_eax = 0x74726956;
            state->_ebx = 0x206c6175;
            state->_ecx = 0x20555043;
            state->_edx = 0x00000000;
            return;
        }
        case 0x80000003: {              // Processor Brand String - part 2
            state->_eax = 0x00000000;
            state->_ebx = 0x00000000;
            state->_ecx = 0x00000000;
            state->_edx = 0x00000000;
            return;
        }
        case 0x80000004: {              // Processor Brand String - part 3
            state->_eax = 0x00000000;
            state->_ebx = 0x00000000;
            state->_ecx = 0x00000000;
            state->_edx = 0x00000000;
            return;
        }
        case 0x80000005: {
            state->_eax = state->_ebx = state->_ecx = state->_edx = 0;
            return;
        }
        case 0x80000006: {
            state->_eax = state->_ebx = 0;
            state->_ecx = 0x04008040;
            state->_edx = 0;
            return;
        }
        case 0x80000007: {
            state->_eax = state->_ebx = state->_ecx = state->_edx = 0;
            return;
        }
        case 0x80000008: {              // Virtual/Physical Address Size
            // Bit mask to identify the reserved bits in paging structure high
            // order address field
            physical_address_size = (uint8_t)state->_eax & 0xff;
            pw_reserved_bits_high_mask =
                    ~((1 << (physical_address_size - 32)) - 1);

            state->_ebx = state->_ecx = state->_edx = 0;
            return;
        }
    }
}

static int exit_hlt(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    int vector;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    htun->_exit_status = HAX_EXIT_HLT;
    vector = vcpu_get_pending_intrs(vcpu);
    advance_rip(vcpu);
    if (hax_valid_vector(vector))
        return HAX_RESUME;

    htun->ready_for_interrupt_injection = 1;
    return HAX_EXIT;
}

static int exit_invlpg(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    advance_rip(vcpu);
    vcpu_invalidate_tlb_addr(vcpu, vmx(vcpu, exit_qualification).address);
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    return HAX_RESUME;
}

static int exit_rdtsc(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    hax_log(HAX_LOGD, "rdtsc exiting: rip: %llx\n", vcpu->state->_rip);
    return HAX_RESUME;
}

static void check_flush(struct vcpu_t *vcpu, uint32_t bits)
{
    switch (vmx(vcpu, exit_qualification).cr.creg) {
        case 0: {
            if (bits & (CR0_PE | CR0_PG)) {
                vcpu_invalidate_tlb(vcpu, 1);
            }
            break;
        }
        case 2: {
            break;
        }
        case 3: {
            vcpu_invalidate_tlb(vcpu, 0);
            break;
        }
        case 4: {
            if (bits & (CR4_PSE | CR4_PAE | CR4_PGE)) {
                vcpu_invalidate_tlb(vcpu, 1);
            }
            break;
        }
        case 8: {
            break;
        }
    }
}

static int exit_cr_access(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    int cr;
    struct vcpu_state_t *state = vcpu->state;
    bool is_ept_pae = false;
    preempt_flag flags;
    uint32_t vmcs_err = 0;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    cr = vmx(vcpu, exit_qualification).cr.creg;

    switch (vmx(vcpu, exit_qualification).cr.type) {
        case 0: { // MOV CR <- GPR
            uint64_t val = state->_regs[(vmx(vcpu, exit_qualification).cr.gpr)];
            uint64_t old_val = 0;

            if (cr == 8) {
                // TODO: Redirect CR8 write to user space (emulated APIC.TPR)
                hax_log(HAX_LOGW, "Ignored guest CR8 write, val=0x%llx\n", val);
                break;
            }

            old_val = vcpu_read_cr(state, cr);
            if (cr == 0) {
                uint64_t cr0_pae_triggers;

                hax_log(HAX_LOGI, "Guest writing to CR0[%u]: 0x%llx -> 0x%llx,"
                        " _cr4=0x%llx, _efer=0x%x\n", vcpu->vcpu_id,
                        old_val, val, state->_cr4, state->_efer);
                if ((val & CR0_PG) && !(val & CR0_PE)) {
                    hax_inject_exception(vcpu, VECTOR_GP, 0);
                    return HAX_RESUME;
                }
                if (!(state->_cr0 & CR0_PG) && (val & CR0_PG) &&
                    (state->_efer & IA32_EFER_LME)) {
                    if (!(state->_cr4 & CR4_PAE)) {
                        hax_inject_exception(vcpu, VECTOR_GP, 0);
                        return HAX_RESUME;
                    }
                }
                if (!hax->ug_enable_flag && (old_val & CR0_PE) &&
                    !(val & CR0_PE)) {
                    htun->_exit_status = HAX_EXIT_REALMODE;
                    hax_log(HAX_LOGD, "Enter NON-PE from PE\n");
                    return HAX_EXIT;
                }

                // See IASDM Vol. 3A 4.4.1
                cr0_pae_triggers = CR0_CD | CR0_NW | CR0_PG;
                if ((val & CR0_PG) && (state->_cr4 & CR4_PAE) &&
                    !(state->_efer & IA32_EFER_LME) && !vtlb_active(vcpu) &&
                    ((val ^ old_val) & cr0_pae_triggers)) {
                    hax_log(HAX_LOGI, "%s: vCPU #%u triggers PDPT (re)load for"
                            " EPT+PAE mode (CR0 path)\n", __func__,
                            vcpu->vcpu_id);
                    is_ept_pae = true;
                }
            } else if (cr == 4) {
                uint64_t cr4_pae_triggers;

                hax_log(HAX_LOGI, "Guest writing to CR4[%u]: 0x%llx -> 0x%llx,"
                        "_cr0=0x%llx, _efer=0x%x\n", vcpu->vcpu_id,
                        old_val, val, state->_cr0, state->_efer);
                if ((state->_efer & IA32_EFER_LMA) && !(val & CR4_PAE)) {
                    hax_inject_exception(vcpu, VECTOR_GP, 0);
                    return HAX_RESUME;
                }

                // See IASDM Vol. 3A 4.4.1
                // TODO: CR4_SMEP is not yet defined
                cr4_pae_triggers = CR4_PAE | CR4_PGE | CR4_PSE;
                if ((val & CR4_PAE) && (state->_cr0 & CR0_PG) &&
                    !(state->_efer & IA32_EFER_LME) && !vtlb_active(vcpu) &&
                    ((val ^ old_val) & cr4_pae_triggers)) {
                    hax_log(HAX_LOGI, "%s: vCPU #%u triggers PDPT (re)load for "
                            "EPT+PAE mode (CR4 path)\n", __func__,
                            vcpu->vcpu_id);
                    is_ept_pae = true;
                }
            } else {
                hax_log(HAX_LOGE, "Unsupported CR%d write, val=0x%llx\n",
                        cr, val);
                break;
            }
            check_flush(vcpu, old_val ^ val);
            vcpu_write_cr(state, cr, val);

            if (is_ept_pae) {
                // The vCPU is either about to enter PAE paging mode (see IASDM
                // Vol. 3A 4.1.2, Figure 4-1) and needs to load its PDPTE
                // registers, or already in PAE mode and needs to reload those
                // registers
                int ret = vcpu_prepare_pae_pdpt(vcpu);
                if (ret) {
                    vcpu_set_panic(vcpu);
                    hax_log(HAX_LOGPANIC, "vCPU #%u failed to (re)load PDPT for"
                            " EPT+PAE mode: ret=%d\n", vcpu->vcpu_id, ret);
                    dump_vmcs(vcpu);
                    return HAX_RESUME;
                }
            }

            if ((vmcs_err = load_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC,
                        "load_vmcs failed while exit_cr_access %x\n", vmcs_err);
                hax_panic_log(vcpu);
                return HAX_RESUME;
            }

            vmwrite_cr(vcpu);

            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC,
                        "put_vmcs failed while exit_cr_access %x\n", vmcs_err);
                hax_panic_log(vcpu);
            }

            break;
        }
        case 1: { // MOV CR -> GPR
            uint64_t val;

            hax_log(HAX_LOGI, "cr_access R CR%d\n", cr);

            val = vcpu_read_cr(state, cr);
            // TODO: Redirect CR8 read to user space (emulated APIC.TPR)
            state->_regs[vmx(vcpu, exit_qualification).cr.gpr] = val;
            break;
        }
        case 2: { // CLTS
            hax_log(HAX_LOGI, "CLTS\n");
            state->_cr0 &= ~(uint64_t)CR0_TS;
            if ((vmcs_err = load_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "load_vmcs failed while CLTS: %x\n",
                        vmcs_err);
                hax_panic_log(vcpu);
                return HAX_RESUME;
            }
            vmwrite_cr(vcpu);
            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "put_vmcs failed while CLTS: %x\n",
                        vmcs_err);
                hax_panic_log(vcpu);
            }
            break;
        }
        case 3: { // LMSW
            hax_log(HAX_LOGI, "LMSW\n");
            state->_cr0 = (state->_cr0 & ~0xfULL) |
                          (vmx(vcpu, exit_qualification).cr.lmsw_source & 0xf);
            if ((vmcs_err = load_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "load_vmcs failed while LMSW %x\n",
                        vmcs_err);
                hax_panic_log(vcpu);
                return HAX_RESUME;
            }
            vmwrite_cr(vcpu);
            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "put_vmcs failed while LMSW %x\n",
                        vmcs_err);
                hax_panic_log(vcpu);
            }
            break;
        }
        default: {
            hax_log(HAX_LOGE, "Unsupported Control Register access type.\n");
            break;
        }
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

static int exit_dr_access(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    uint64_t *dr = NULL;
    int dreg = vmx(vcpu, exit_qualification.dr.dreg);
    int gpr_reg = vmx(vcpu, exit_qualification).dr.gpr;
    bool hbreak_enabled = !!(vcpu->debug_control & HAX_DEBUG_USE_HW_BP);
    struct vcpu_state_t *state = vcpu->state;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    // General Detect(GD) Enable flag
    if (state->_dr7 & DR7_GD) {
        state->_dr7 &= ~(uint64_t)DR7_GD;
        state->_dr6 |= DR6_BD;
        vmwrite(vcpu, GUEST_DR7, state->_dr7);
        // Priority 4 fault
        hax_inject_exception(vcpu, VECTOR_DB, NO_ERROR_CODE);
        return HAX_RESUME;
    }

    switch (dreg) {
        case 0: {
            dr = &state->_dr0;
            break;
        }
        case 1: {
            dr = &state->_dr1;
            break;
        }
        case 2: {
            dr = &state->_dr2;
            break;
        }
        case 3: {
            dr = &state->_dr3;
            break;
        }
        case 4: {
            if (state->_cr4 & CR4_DE) {
                hax_inject_exception(vcpu, VECTOR_UD, NO_ERROR_CODE);
                return HAX_RESUME;
            }
            dr = &state->_dr6;
            break;
        }
        case 5: {
            if (state->_cr4 & CR4_DE) {
                hax_inject_exception(vcpu, VECTOR_UD, NO_ERROR_CODE);
                return HAX_RESUME;
            }
            dr = &state->_dr7;
            break;
        }
        case 6: {
            dr = &state->_dr6;
            break;
        }
        case 7: {
            dr = &state->_dr7;
            break;
        }
        default: {
            // It should not go here. Unreachable.
        }
    }

    if (vmx(vcpu, exit_qualification.dr.direction)) {
        // MOV DR -> GPR
        if (hbreak_enabled) {
            // HAX hardware breakpoint enabled, return dr default value
            if (dreg == 6)
                state->_regs[gpr_reg] = DR6_SETBITS;
            else if (dreg == 7)
                state->_regs[gpr_reg] = DR7_SETBITS;
            else
                state->_regs[gpr_reg] = 0;

            hax_log(HAX_LOGD, "Ignore guest DR%d read due to hw bp enabled.\n",
                    dreg);
        } else {
            state->_regs[gpr_reg] = *dr;
        }
    } else {
        // MOV DR <- GPR
        if (hbreak_enabled) {
            hax_log(HAX_LOGD, "Ignore guest DR%d write due to hw bp enabled.\n",
                    dreg);
        } else {
            *dr = state->_regs[gpr_reg];
            vcpu->dr_dirty = 1;
        }
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

static int handle_string_io(struct vcpu_t *vcpu, exit_qualification_t *qual,
                            struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint64_t count, total_size;
    uint elem_size, n, copy_size;
    hax_vaddr_t gla, start_gva;

    // 1 indicates string I/O (i.e. OUTS or INS)
    htun->io._flags = 1;

    count = qual->io.rep ? state->_rcx : 1;
    elem_size = htun->io._size;
    total_size = count * elem_size;

    // Number of data elements to copy
    n = total_size > IOS_MAX_BUFFER ? IOS_MAX_BUFFER / elem_size : (uint)count;
    htun->io._count = n;
    copy_size = n * elem_size;

    // Both OUTS and INS instructions reference a GVA, which indicates the
    // source (for OUTS) or destination (for INS) of data transfer. This GVA is
    // conveniently recorded in VMCS by hardware at VM exit time (see IA SDM
    // Vol. 3C 27.2.1, "Guest-linear address"), so there is no need to fetch the
    // instruction and parse it manually (e.g. to determine the address-size
    // attribute of the instruction, or to check the presence of a segment
    // override prefix that can make OUTS read from ES:ESI instead of DS:ESI).
    gla = vmread(vcpu, VM_EXIT_INFO_GUEST_LINEAR_ADDRESS);
    if (state->_rflags & EFLAGS_DF) {
        start_gva = gla - (n - 1) * elem_size;
        htun->io._df = 1;
    } else {
        start_gva = gla;
        htun->io._df = 0;
    }
    // For INS (see handle_io_post())
    htun->io._vaddr = start_gva;

    if (qual->io.direction == HAX_IO_OUT) {
        if (!vcpu_read_guest_virtual(vcpu, start_gva, vcpu->io_buf,
                                     IOS_MAX_BUFFER, copy_size, 0)) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "%s: vcpu_read_guest_virtual() failed,"
                    " start_gva=0x%llx, elem_size=%u, count=%llu\n",
                    __func__, start_gva, elem_size, count);
            dump_vmcs(vcpu);
            return HAX_RESUME;
        }
    }

    state->_rcx -= n;
    if (n == count) {
        advance_rip(vcpu);
    }

    if (state->_rflags & EFLAGS_DF) {
        if (qual->io.direction == HAX_IO_OUT) {
            state->_rsi -= copy_size;
        } else {
            state->_rdi -= copy_size;
        }
    } else {
        if (qual->io.direction == HAX_IO_OUT) {
            state->_rsi += copy_size;
        } else {
            state->_rdi += copy_size;
        }
    }

    htun->_exit_status = HAX_EXIT_IO;
    return HAX_EXIT;
}

static int handle_io(struct vcpu_t *vcpu, exit_qualification_t *qual,
                     struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    htun->io._count = 1;
    htun->io._flags = 0;

    if (qual->io.direction == HAX_IO_OUT) {
        switch (qual->io.size + 1) {
            case 1: {
                *((uint8_t *)vcpu->io_buf) = state->_al;
                break;
            }
            case 2: {
                *((uint16_t *)vcpu->io_buf) = state->_ax;
                break;
            }
            case 4: {
                *((uint32_t *)vcpu->io_buf) = state->_eax;
                break;
            }
            default: {
                break;
            }
        }
    }
    advance_rip(vcpu);
    htun->_exit_status = HAX_EXIT_IO;
    return HAX_EXIT;
}

static int exit_io_access(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    exit_qualification_t *qual = &vmx(vcpu, exit_qualification);

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    // Clear all the fields before using them
    htun->io._direction = 0;
    htun->io._df = 0;
    htun->io._port = 0;
    htun->io._size = 0;
    htun->io._count = 0;

    htun->io._port = qual->io.encoding ? qual->io.port : state->_dx;
    htun->io._size = qual->io.size + 1;
    htun->io._direction = qual->io.direction;

    hax_log(HAX_LOGD, "exit_io_access port %x, size %d\n", htun->io._port,
            htun->io._size);

    if (qual->io.string)
        return handle_string_io(vcpu, qual, htun);

    return handle_io(vcpu, qual, htun);
}

static int exit_msr_read(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32_t msr = state->_ecx;
    uint64_t val;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    if (!handle_msr_read(vcpu, msr, &val)) {
        state->_rax = val & 0xffffffff;
        state->_rdx = (val >> 32) & 0xffffffff;
    } else {
        hax_inject_exception(vcpu, VECTOR_GP, 0);
        return HAX_RESUME;
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

static int exit_msr_write(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32_t msr = state->_ecx;
    uint64_t val = (uint64_t)(state->_edx) << 32 | state->_eax;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    if (handle_msr_write(vcpu, msr, val, false)) {
        hax_inject_exception(vcpu, VECTOR_GP, 0);
        return HAX_RESUME;
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

/*
 * Returns 0 if handled, else returns 1
 * According to the caller, return 1 will cause GP to guest
 */
static int misc_msr_read(struct vcpu_t *vcpu, uint32_t msr, uint64_t *val)
{
    mtrr_var_t *v;

    if (msr >= IA32_MTRR_PHYSBASE0 && msr <= IA32_MTRR_PHYSMASK9) {
        hax_assert((msr >> 1 & 0x7f) < NUM_VARIABLE_MTRRS);
        v = &vcpu->mtrr_current_state.mtrr_var[msr >> 1 & 0x7f];
        *val = msr & 1 ? v->mask.raw : v->base.raw;
        return 0;
    } else if (msr >= MTRRFIX16K_80000 && msr <= MTRRFIX16K_A0000) {
        *val = vcpu->mtrr_current_state.mtrr_fixed16k[msr & 0x1];
        return 0;
    } else if (msr >= MTRRFIX4K_C0000 && msr <= MTRRFIX4K_F8000) {
        *val = vcpu->mtrr_current_state.mtrr_fixed4k[msr & 0x7];
        return 0;
    } else if ((msr >= IA32_MC0_CTL2 && msr <= IA32_MC8_CTL2) ||
               (msr >= 0x300 && msr <= 0x3ff)) {
        *val = 0;
        return 0;
    }

    return 1;
}

static int handle_msr_read(struct vcpu_t *vcpu, uint32_t msr, uint64_t *val)
{
    int index, r = 0;
    struct vcpu_state_t *state = vcpu->state;
    struct gstate *gstate = &vcpu->gstate;

    switch (msr) {
        case IA32_TSC: {
            *val = vcpu->tsc_offset + ia32_rdtsc();
            break;
        }
        case IA32_FEATURE_CONTROL: {
            *val = 0x5u;
            break;
        }
        case IA32_PLATFORM_ID: {
            *val = 0x18000000000000ULL;
            break;
        }
        case IA32_APIC_BASE: {
            *val = gstate->apic_base;
            break;
        }
        case IA32_EFER: {
            *val = state->_efer;
            break;
        }
        case IA32_STAR:
        case IA32_LSTAR:
        case IA32_CSTAR:
        case IA32_SF_MASK:
        case IA32_KERNEL_GS_BASE: {
            for (index = 0; index < NR_GMSR; index++) {
                if ((uint32_t)gstate->gmsr[index].entry == msr) {
                    *val = gstate->gmsr[index].value;
                    break;
                }
                *val = 0;
            }
            break;
        }
        case IA32_TSC_AUX: {
            if (!cpu_has_feature(X86_FEATURE_RDTSCP)) {
                r = 1;
                break;
            }
            *val = gstate->tsc_aux & 0xFFFFFFFF;
            break;
        }
        case IA32_FS_BASE: {
            if (vcpu->fs_base_dirty)
                *val = vcpu->state->_fs.base;
            else
                *val = vmread(vcpu, GUEST_FS_BASE);
            break;
        }
        case IA32_GS_BASE: {
            *val = vmread(vcpu, GUEST_GS_BASE);
            break;
        }
        case IA32_SYSENTER_CS: {
            *val = state->_sysenter_cs;
            break;
        }
        case IA32_SYSENTER_ESP: {
            *val = state->_sysenter_esp;
            break;
        }
        case IA32_SYSENTER_EIP: {
            *val = state->_sysenter_eip;
            break;
        }
        // FIXME: Not fully implemented - just restore what guest wrote to MTRR
        case IA32_MTRRCAP: {
            *val = vcpu->mtrr_current_state.mtrr_cap.raw;
            break;
        }
        case MTRRFIX64K_00000: {
            *val = vcpu->mtrr_current_state.mtrr_fixed64k;
            break;
        }
        case IA32_CR_PAT: {
            *val = vcpu->cr_pat;
            break;
        }
        case IA32_MTRR_DEF_TYPE: {
            *val = vcpu->mtrr_current_state.mtrr_def_type.raw;
            break;
        }
        case IA32_MCG_CAP: {
            // 1 MC reporting reg
            *val = 1;
            break;
        }
        case IA32_MCG_STATUS: {
            *val = 0;
            break;
        }
        case IA32_MCG_CTL: {
            *val = 0x3;
            break;
        }
        case IA32_MC0_CTL:
        case IA32_P5_MC_TYPE: // P4 Maps P5_Type to Status
        case IA32_MC0_STATUS: {
            *val = 0;
            break;
        }
        case IA32_P5_MC_ADDR: // P4 Maps P5_Type to Status
        case IA32_MC0_ADDR: {
            *val = 0;
            break;
        }
        case IA32_MC0_MISC: {
            *val = 0;
            break;
        }
        case IA32_THERM_DIODE_OFFSET: {
            *val = 0;
            break;
        }
        case IA32_FSB_FREQ: {
            *val = 4;
            break;
        }
        case IA32_TEMP_TARGET: {
            *val = 0x86791b00;
            break;
        }
        case IA32_BBL_CR_CTL3: {
            *val = 0xbe702111;
            break;
        }
        case IA32_DEBUGCTL: {
            // TODO: Will this still work when we support APM v2?
            *val = 0;
            break;
        }
        case IA32_CPUID_FEATURE_MASK: {
            cpuid_get_features_mask(vcpu->guest_cpuid, val);
            break;
        }
        case IA32_EBC_FREQUENCY_ID: {
            *val = 0;
            break;
        }
        case IA32_EBC_HARD_POWERON: {
            *val = 0;
            break;
        }
        case IA32_EBC_SOFT_POWERON: {
            *val = 0;
            break;
        }
        case IA32_BIOS_SIGN_ID: {
            *val = 0x67311111;
            break;
        }
        case IA32_MISC_ENABLE: {
            *val = 1u << 11 | 1u << 12;
            break;
        }
        // Old Linux kernels may read this MSR without first making sure that
        // the vCPU supports the "pdcm" feature (which it does not)
        case IA32_PERF_CAPABILITIES: {
            *val = 0;
            hax_log(HAX_LOGI, "handle_msr_read: IA32_PERF_CAPABILITIES\n");
            break;
        }
        // In case the host CPU does not support MSR bitmaps, emulate MSR reads
        // from performance monitoring registers
        case IA32_PMC0:
        case IA32_PMC1:
        case IA32_PMC2:
        case IA32_PMC3: {
            *val = hax->apm_version ? gstate->apm_pmc_msrs[msr - IA32_PMC0] &
                   hax->apm_general_mask : 0;
            hax_log(HAX_LOGI, "handle_msr_read: IA32_PMC%u value=0x%llx\n",
                    msr - IA32_PMC0, *val);
            break;
        }
        case IA32_PERFEVTSEL0:
        case IA32_PERFEVTSEL1:
        case IA32_PERFEVTSEL2:
        case IA32_PERFEVTSEL3: {
            *val = hax->apm_version
                   ? gstate->apm_pes_msrs[msr - IA32_PERFEVTSEL0]
                   : 0;
            hax_log(HAX_LOGI, "handle_msr_read: IA32_PERFEVTSEL%u "
                    "value=0x%llx\n", msr - IA32_PERFEVTSEL0, *val);
            break;
        }
        default: {
            r = misc_msr_read(vcpu, msr, val);
            break;
        }
    }

    return r;
}

static void vmwrite_efer(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32_t entry_ctls = vmx(vcpu, entry_ctls);

    if ((state->_cr0 & CR0_PG) && (state->_efer & IA32_EFER_LME)) {
        state->_efer |= IA32_EFER_LMA;
        entry_ctls |= ENTRY_CONTROL_LONG_MODE_GUEST;
    } else {
        state->_efer &= ~IA32_EFER_LMA;
        entry_ctls &= ~ENTRY_CONTROL_LONG_MODE_GUEST;
    }

    if (vmx(vcpu, entry_ctls) & ENTRY_CONTROL_LOAD_EFER) {
        uint32_t guest_efer = state->_efer;

        if (vtlb_active(vcpu)) {
            guest_efer |= IA32_EFER_XD;
        }

        vmwrite(vcpu, GUEST_EFER, guest_efer);
    }

    if (entry_ctls != vmx(vcpu, entry_ctls)) {
        vmwrite(vcpu, VMX_ENTRY_CONTROLS, vmx(vcpu, entry_ctls) = entry_ctls);
    }
}

static int misc_msr_write(struct vcpu_t *vcpu, uint32_t msr, uint64_t val)
{
    mtrr_var_t *v;

    if (msr >= IA32_MTRR_PHYSBASE0 && msr <= IA32_MTRR_PHYSMASK9) {
        hax_assert((msr >> 1 & 0x7f) < NUM_VARIABLE_MTRRS);
        v = &vcpu->mtrr_current_state.mtrr_var[msr >> 1 & 0x7f];
        if (msr & 1) {
            v->mask.raw = val;
        } else {
            v->base.raw = val;
        }
        return 0;
    } else if (msr >= MTRRFIX16K_80000 && msr <= MTRRFIX16K_A0000) {
        vcpu->mtrr_current_state.mtrr_fixed16k[msr & 0x1] = val;
        return 0;
    } else if (msr >= MTRRFIX4K_C0000 && msr <= MTRRFIX4K_F8000) {
        vcpu->mtrr_current_state.mtrr_fixed4k[msr & 0x7] = val;
        return 0;
    } else if ((msr >= IA32_MC0_CTL2 && msr <= IA32_MC8_CTL2) ||
               (msr >= 0x300 && msr <= 0x3ff)) {
        return 0;
    }

    return 1;
}

static inline bool is_pat_valid(uint64_t val)
{
    if (val & 0xF8F8F8F8F8F8F8F8)
        return false;

    // 0, 1, 4, 5, 6, 7 are valid values.
    return (val | ((val & 0x0202020202020202) << 1)) == val;
}

static int handle_msr_write(struct vcpu_t *vcpu, uint32_t msr, uint64_t val,
                            bool by_host)
{
    int index, r = 0;
    struct vcpu_state_t *state = vcpu->state;
    struct gstate *gstate = &vcpu->gstate;

    switch (msr) {
        case IA32_TSC: {
            vcpu->tsc_offset = val - ia32_rdtsc();
            if (vmx(vcpu, pcpu_ctls) & USE_TSC_OFFSETTING) {
                vmwrite(vcpu, VMX_TSC_OFFSET, vcpu->tsc_offset);
            }
            break;
        }
        case IA32_BIOS_SIGN_ID:
        case IA32_MCG_STATUS:
        case IA32_MC0_CTL:
        case IA32_MC0_STATUS:
        case IA32_MC0_ADDR:
        case IA32_MC0_MISC:
        case IA32_DEBUGCTL:
        case IA32_MISC_ENABLE: {
            break;
        }
        case IA32_CPUID_FEATURE_MASK: {
            cpuid_set_features_mask(vcpu->guest_cpuid, val);
            break;
        }
        case IA32_EFER: {
            hax_log(HAX_LOGI, "%s writing to EFER[%u]: 0x%x -> 0x%llx, "
                    "_cr0=0x%llx, _cr4=0x%llx\n", by_host ? "Host" : "Guest",
                    vcpu->vcpu_id, state->_efer, val,
                    state->_cr0, state->_cr4);

            /* val - "new" EFER, state->_efer - "old" EFER.*/
            if ((val &
                 ~((uint64_t)(IA32_EFER_SCE | IA32_EFER_LME |
                              IA32_EFER_LMA | IA32_EFER_XD)))) {
                hax_log(HAX_LOGE, "Illegal value 0x%llx written to EFER. "
                        "Reserved bits were set. EFER was 0x%llx\n",
                        val, (uint64_t) state->_efer);
                r = 1;
                break;
            }

            if (!by_host) {
                /*
                 * Two code paths can lead to handle_msr_write():
                 *  a) The guest invokes the WRMSR instruction;
                 *  b) The host calls the HAX_VCPU_IOCTL_SET_MSRS ioctl.
                 * The following checks are only applicable to guest-initiated
                 * EFER writes, not to host-initiated EFER writes. E.g., when
                 * booting the guest from a VM snapshot, the host (QEMU) may
                 * need to initialize the vCPU in 64-bit mode (CR0.PG = CR4.PAE
                 * = EFER.LME = EFER.LMA = CS.L = 1) via SET_REGS and SET_MSRS
                 * ioctls.
                 */
                if (((val & IA32_EFER_LMA) ^
                     (state->_efer & IA32_EFER_LMA))) {
                    hax_log(HAX_LOGW, "Ignoring guest write to IA32_EFER.LMA. "
                            "EFER: 0x%llx -> 0x%llx\n",
                            (uint64_t) state->_efer,val);
                    /*
                     * No need to explicitly fix the LMA bit here:
                     *  val ^= IA32_EFER_LMA;
                     * because in the end vmwrite_efer() will ignore the LMA
                     * bit in |val|.
                     */
                }
                if ((state->_cr0 & CR0_PG) &&
                    ((val & IA32_EFER_LME) ^
                     (state->_efer & IA32_EFER_LME))) {
                    hax_log(HAX_LOGE, "Attempted to enable or disable Long Mode"
                            " with paging enabled. EFER: 0x%llx -> 0x%llx\n",
                            (uint64_t) state->_efer, val);
                    r = 1;
                    break;
                }
            }
            state->_efer = val;

            if (!(ia32_rdmsr(IA32_EFER) & IA32_EFER_LMA) &&
                (state->_efer & IA32_EFER_LME)) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC,
                        "64-bit guest is not allowed on 32-bit host.\n");
            } else if ((state->_efer & IA32_EFER_LME) && vtlb_active(vcpu)) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "64-bit guest is not allowed on core 2 "
                        "machine.\n");
            } else {
                vmwrite_efer(vcpu);
            }
            break;
        }
        case IA32_STAR:
        case IA32_LSTAR:
        case IA32_CSTAR:
        case IA32_SF_MASK:
        case IA32_KERNEL_GS_BASE: {
            for (index = 0; index < NR_GMSR; index++) {
                if ((uint32_t)gmsr_list[index] == msr) {
                    gstate->gmsr[index].value = val;
                    gstate->gmsr[index].entry = msr;
                    break;
                }
            }
            break;
        }
        case IA32_TSC_AUX: {
            if (!cpu_has_feature(X86_FEATURE_RDTSCP) || (val >> 32)) {
                r = 1;
                break;
            }
            gstate->tsc_aux = val;
            break;
        }
        case IA32_FS_BASE: {
            /*
             * During Android emulator running, there are a lot of FS_BASE
             * msr write. To avoid unnecessary vmcs loading/putting, don't
             * write it to vmcs until right before next VM entry, when the
             * VMCS region has been loaded into memory.
             */
            vcpu->state->_fs.base = val;
            vcpu->fs_base_dirty = 1;
            break;
        }
        case IA32_GS_BASE: {
            vmwrite(vcpu, GUEST_GS_BASE, val);
            break;
        }
        case IA32_SYSENTER_CS: {
            state->_sysenter_cs = val & 0xffff;
            vmwrite(vcpu, GUEST_SYSENTER_CS, state->_sysenter_cs);
            break;
        }
        case IA32_SYSENTER_EIP: {
            state->_sysenter_eip = val;
            vmwrite(vcpu, GUEST_SYSENTER_EIP, state->_sysenter_eip);
            break;
        }
        case IA32_SYSENTER_ESP: {
            state->_sysenter_esp = val;
            vmwrite(vcpu, GUEST_SYSENTER_ESP, state->_sysenter_esp);
            break;
        }
        // FIXME: Not fully implemented - just store what guest writes to MTRR
        case IA32_MTRRCAP: {
            vcpu->mtrr_current_state.mtrr_cap.raw = val;
            break;
        }
        case MTRRFIX64K_00000: {
            vcpu->mtrr_current_state.mtrr_fixed64k = val;
            break;
        }
        case IA32_CR_PAT: {
            // Attempting to write an undefined memory type encoding into the
            // PAT causes a general-protection (#GP) exception to be generated
            if (!is_pat_valid(val)) {
                r = 1;
                break;
            }

            vcpu->cr_pat = val;
            vmwrite(vcpu, GUEST_PAT, vcpu->cr_pat);
            break;
        }
        case IA32_MTRR_DEF_TYPE: {
            vcpu->mtrr_current_state.mtrr_def_type.raw = val;
            break;
        }
        case IA32_APIC_BASE: {
            r = vcpu_set_apic_base(vcpu, val);
            break;
        }
        case IA32_BIOS_UPDT_TRIG: {
            break;
        }
        case IA32_EBC_FREQUENCY_ID:
        case IA32_EBC_HARD_POWERON:
        case IA32_EBC_SOFT_POWERON: {
            break;
        }
        // In case the host CPU does not support MSR bitmaps, emulate MSR writes
        // to performance monitoring registers
        case IA32_PMC0:
        case IA32_PMC1:
        case IA32_PMC2:
        case IA32_PMC3: {
            if (hax->apm_version) {
                // According to IA SDM Vol. 3B 18.2.5, writes to IA_PMCx use
                // only bits 31..0 of the input value
                gstate->apm_pmc_msrs[msr - IA32_PMC0] = val & 0xffffffff;
                hax_log(HAX_LOGI, "handle_msr_write: IA32_PMC%u value=0x%llx\n",
                        msr - IA32_PMC0, val);
            }
            break;
        }
        case IA32_PERFEVTSEL0:
        case IA32_PERFEVTSEL1:
        case IA32_PERFEVTSEL2:
        case IA32_PERFEVTSEL3: {
            if (hax->apm_version) {
                // According to IA SDM Vol. 3B Figure 18-1 (APM v1) and Figure
                // 18-6 (APM v3), bits 63..32 of IA_PERFEVTSELx are reserved
                gstate->apm_pes_msrs[msr - IA32_PERFEVTSEL0] = val & 0xffffffff;
                hax_log(HAX_LOGI, "handle_msr_write: IA32_PERFEVTSEL%u "
                        "value=0x%llx\n", msr - IA32_PERFEVTSEL0, val);
            }
            break;
        }
        default: {
            r = misc_msr_write(vcpu, msr, val);
            break;
        }
    }
    return r;
}

static int exit_invalid_guest_state(struct vcpu_t *vcpu,
                                    struct hax_tunnel *htun)
{
    vcpu_set_panic(vcpu);
    hax_log(HAX_LOGPANIC, "vcpu->tr:%x\n", vcpu->state->_tr.ar);
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

static int exit_ept_misconfiguration(struct vcpu_t *vcpu,
                                     struct hax_tunnel *htun)
{
    hax_paddr_t gpa;
    int ret;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    gpa = vmx(vcpu, exit_gpa);
    ret = ept_handle_misconfiguration(&vcpu->vm->gpa_space, &vcpu->vm->ept_tree,
                                      gpa);
    if (ret > 0) {
        // The misconfigured entries have been fixed
        return HAX_RESUME;
    }

    vcpu_set_panic(vcpu);
    hax_log(HAX_LOGPANIC, "%s: Unexpected EPT misconfiguration: gpa=0x%llx\n",
            __func__, gpa);
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

static int exit_ept_violation(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    exit_qualification_t *qual = &vmx(vcpu, exit_qualification);
    hax_paddr_t gpa;
    int ret = 0;
    uint64_t fault_gfn;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    if (qual->ept.gla1 == 0 && qual->ept.gla2 == 1) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "Incorrect EPT setting\n");
        dump_vmcs(vcpu);
        return HAX_RESUME;
    }

    gpa = vmx(vcpu, exit_gpa);

    ret = ept_handle_access_violation(&vcpu->vm->gpa_space, &vcpu->vm->ept_tree,
                                      *qual, gpa, &fault_gfn);
    if (ret == -EFAULT) {
        // Extract bits 5..0 from Exit Qualification. They indicate the type of
        // the faulting access (HAX_PAGEFAULT_ACC_R/W/X) and the types of access
        // allowed (HAX_PAGEFAULT_PERM_R/W/X).
        htun->pagefault.flags = qual->raw & 0x3f;
        htun->pagefault.gpa = fault_gfn << PG_ORDER_4K;
        htun->pagefault.reserved1 = 0;
        htun->pagefault.reserved2 = 0;
        htun->_exit_status = HAX_EXIT_PAGEFAULT;
        return HAX_EXIT;
    }
    if (ret == -EACCES) {
        /*
         * For some reason, during boot-up, Chrome OS guests make hundreds of
         * attempts to write to GPAs close to 4GB, which are mapped into BIOS
         * (read-only) and thus result in EPT violations.
         * TODO: Handle this case properly.
         */
        hax_log(HAX_LOGW, "%s: Unexpected EPT violation cause. Skipping "
                "instruction (len=%u)\n", __func__,
                vcpu->vmx.exit_instr_length);
        advance_rip(vcpu);
        return HAX_EXIT;
    }
    if (ret < 0) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "%s: ept_handle_access_violation() "
                "returned %d.\n", __func__, ret);
        dump_vmcs(vcpu);
        return HAX_RESUME;
    }
    if (ret > 0) {
        // The EPT violation is due to a RAM/ROM access and has been handled
        return HAX_RESUME;
    }
    // ret == 0: The EPT violation is due to MMIO

    return vcpu_emulate_insn(vcpu);
}

static int exit_unsupported_instruction(struct vcpu_t *vcpu, 
                                        struct hax_tunnel *htun)
{
    hax_inject_exception(vcpu, VECTOR_UD, NO_ERROR_CODE);
    return HAX_RESUME;
}

static void handle_mem_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    hax_log(HAX_LOGW,
            "handle_mem_fault: Setting exit status to HAX_EXIT_MMIO.\n");
    htun->_exit_status = HAX_EXIT_MMIO;
}

static int null_handler(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    vcpu_set_panic(vcpu);
    hax_log(HAX_LOGPANIC, "Unhandled vmx vmexit reason:%d\n",
            htun->_exit_reason);
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

#define COPY_SEG_DESC_FIELD(field)      \
        if (new->field != old->field) { \
            new->field = old->field;    \
            flags = 1;                  \
        }

static int _copy_desc(segment_desc_t *old, segment_desc_t *new)
{
    int flags = 0;

    COPY_SEG_DESC_FIELD(selector);
    COPY_SEG_DESC_FIELD(_dummy);
    COPY_SEG_DESC_FIELD(base);
    COPY_SEG_DESC_FIELD(limit);
    COPY_SEG_DESC_FIELD(ar);
    COPY_SEG_DESC_FIELD(ipad);

    if (old->ar == 0) {
        new->null = 1;
    }

    return flags;
}

int vcpu_get_regs(struct vcpu_t *vcpu, struct vcpu_state_t *ustate)
{
    struct vcpu_state_t *state = vcpu->state;
    int i;

    vcpu_vmread_all(vcpu);

    for (i = 0; i < 16; i++) {
        ustate->_regs[i] = state->_regs[i];
    }
    ustate->_rip = state->_rip;
    ustate->_rflags = state->_rflags;

    ustate->_cr0 = state->_cr0;
    ustate->_cr2 = state->_cr2;
    ustate->_cr3 = state->_cr3;
    ustate->_cr4 = state->_cr4;

    ustate->_dr0 = state->_dr0;
    ustate->_dr1 = state->_dr1;
    ustate->_dr2 = state->_dr2;
    ustate->_dr3 = state->_dr3;
    ustate->_dr6 = state->_dr6;
    ustate->_dr7 = state->_dr7;
    _copy_desc(&state->_cs, &ustate->_cs);
    _copy_desc(&state->_ds, &ustate->_ds);
    _copy_desc(&state->_es, &ustate->_es);
    _copy_desc(&state->_ss, &ustate->_ss);
    _copy_desc(&state->_fs, &ustate->_fs);
    _copy_desc(&state->_gs, &ustate->_gs);
    _copy_desc(&state->_ldt, &ustate->_ldt);
    _copy_desc(&state->_tr, &ustate->_tr);
    _copy_desc(&state->_gdt, &ustate->_gdt);
    _copy_desc(&state->_idt, &ustate->_idt);

    return 0;
}

#define UPDATE_VCPU_STATE(field, flags)      \
        if (state->field != ustate->field) { \
            state->field = ustate->field;    \
            flags += 1;                      \
        }

#define UPDATE_SEGMENT_STATE(seg, field)                 \
        if (_copy_desc(&ustate->field, &state->field)) { \
            VMWRITE_SEG(vcpu, seg, state->field);        \
        }

int vcpu_set_regs(struct vcpu_t *vcpu, struct vcpu_state_t *ustate)
{
    struct vcpu_state_t *state = vcpu->state;
    int i;
    int cr_dirty = 0, dr_dirty = 0;
    preempt_flag flags;
    int rsp_dirty = 0;
    uint32_t vmcs_err = 0;

    if (state->_rsp != ustate->_rsp) {
        rsp_dirty = 1;
    }

    for (i = 0; i < 16; i++) {
        state->_regs[i] = ustate->_regs[i];
    }

    if ((vmcs_err = load_vmcs(vcpu, &flags))) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "load_vmcs failed on vcpu_set_regs: %x\n",
                vmcs_err);
        hax_panic_log(vcpu);
        return -EFAULT;
    }

    if (state->_rip != ustate->_rip) {
        state->_rip = ustate->_rip;
        vcpu->rip_dirty = 1;
    }
    if (state->_rflags != ustate->_rflags) {
        state->_rflags = ustate->_rflags;
        vcpu->rflags_dirty = 1;
    }
    if (rsp_dirty) {
        state->_rsp = ustate->_rsp;
        vmwrite(vcpu, GUEST_RSP, state->_rsp);
    }

    UPDATE_VCPU_STATE(_cr0, cr_dirty);
    UPDATE_VCPU_STATE(_cr2, cr_dirty);
    UPDATE_VCPU_STATE(_cr3, cr_dirty);
    UPDATE_VCPU_STATE(_cr4, cr_dirty);
    if (cr_dirty) {
        vmwrite_cr(vcpu);
    }

    /*
     * When the guest debug feature is in use (HAX_DEBUG_ENABLE is on), guest
     * DR state is owned by the debugger (QEMU gdbserver), and must be
     * protected from SET_REGS ioctl (called by other parts of QEMU).
     *
     * An obvious case is when hardware breakpoints are enabled
     * (HAX_DEBUG_ENABLE | HAX_DEBUG_USE_HW_BP): all of DR0..7 need to be
     * filled with values provided by the debugger. This is actually also true
     * when hardware breakpoints are disabled: at least, the debugger expects
     * DR7 to be disabled/reset, so we must not allow SET_REGS to enable it.
     *
     * Checking only the HAX_DEBUG_USE_HW_BP flag can actually lead to
     * incorrect behavior, exposed by the gdb "c[ontinue]" command.
     *
     * Usually using hardware breakpoint to debug guest is running in this way:
     *
     * 1. QEMU/gdb enables hardware breakpoint via vcpu_debug ioctl;
     * 2. Guest hits the break point and vm exits;
     * 3. QEMU/gdb handles the event, then disables HW breakpoint;
     * 4. QEMU/gdb enables single step debugging then resumes guest;
     * 5. Guest hits single step and vm exits;
     * 6. QEMU/gdb re-enable HW breakpoint.
     * 7. Guest resumes running.
     *
     * From step 3 to step 5, HW breakpoint is disabled temporarily and runs
     * one step, it should be presumably to prevent the guest from looping
     * on the same HW breakpoint without setting RFAGS.RF (See Intel SDM Vol.
     * 3B 17.3.1.1), which can't be done in user space.
     */
    if (vcpu->debug_control & HAX_DEBUG_ENABLE) {
        hax_log(HAX_LOGI, "%s: Ignore DR updates because hax debugging has "
                "been enabled in %d.\n", __func__, vcpu->vcpu_id);
    } else {
        UPDATE_VCPU_STATE(_dr0, dr_dirty);
        UPDATE_VCPU_STATE(_dr1, dr_dirty);
        UPDATE_VCPU_STATE(_dr2, dr_dirty);
        UPDATE_VCPU_STATE(_dr3, dr_dirty);
        ustate->_dr6 = fix_dr6(ustate->_dr6);
        UPDATE_VCPU_STATE(_dr6, dr_dirty);
        ustate->_dr7 = fix_dr7(ustate->_dr7);
        UPDATE_VCPU_STATE(_dr7, dr_dirty);

        if (dr_dirty)
            vcpu->dr_dirty = 1;
        else
            vcpu->dr_dirty = 0;
    }

    UPDATE_SEGMENT_STATE(CS, _cs);
    UPDATE_SEGMENT_STATE(DS, _ds);
    UPDATE_SEGMENT_STATE(ES, _es);
    UPDATE_SEGMENT_STATE(FS, _fs);
    UPDATE_SEGMENT_STATE(GS, _gs);
    UPDATE_SEGMENT_STATE(SS, _ss);
    UPDATE_SEGMENT_STATE(LDTR, _ldt);
    UPDATE_SEGMENT_STATE(TR, _tr);

    if (_copy_desc(&ustate->_gdt, &state->_gdt)) {
        VMWRITE_DESC(vcpu, GDTR, state->_gdt);
    }

    if (_copy_desc(&ustate->_idt, &state->_idt)) {
        VMWRITE_DESC(vcpu, IDTR, state->_idt);
    }

    if ((vmcs_err = put_vmcs(vcpu, &flags))) {
        vcpu_set_panic(vcpu);
        hax_log(HAX_LOGPANIC, "put_vmcs failed on vcpu_set_regs: %x\n",
                vmcs_err);
        hax_panic_log(vcpu);
    }

    return 0;
}

int vcpu_get_fpu(struct vcpu_t *vcpu, struct fx_layout *ufl)
{
    struct fx_layout *fl = (struct fx_layout *)hax_page_va(
            vcpu->gstate.gfxpage);
    int i, j;

    ufl->fcw = fl->fcw;
    ufl->fsw = fl->fsw;
    ufl->ftw = fl->ftw;
    ufl->res1 = fl->res1;
    ufl->fop = fl->fop;
    ufl->fpu_ip = fl->fpu_ip;
    ufl->fpu_dp = fl->fpu_dp;
    ufl->mxcsr = fl->mxcsr;
    ufl->mxcsr_mask = fl->mxcsr_mask;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 16; j++) {
            ufl->st_mm[i][j] = fl->st_mm[i][j];
            ufl->mmx_1[i][j] = fl->mmx_1[i][j];
            ufl->mmx_2[i][j] = fl->mmx_2[i][j];
        }
    }
    for (i = 0; i < 96; i++) {
        ufl->pad[i] = fl->pad[i];
    }

    return 0;
}

int vcpu_put_fpu(struct vcpu_t *vcpu, struct fx_layout *ufl)
{
    struct fx_layout *fl = (struct fx_layout *)hax_page_va(
            vcpu->gstate.gfxpage);
    int i, j;

    fl->fcw = ufl->fcw;
    fl->fsw = ufl->fsw;
    fl->ftw = ufl->ftw;
    fl->res1 = ufl->res1;
    fl->fop = ufl->fop;
    fl->fpu_ip = ufl->fpu_ip;
    fl->fpu_dp = ufl->fpu_dp;
    fl->mxcsr = ufl->mxcsr;
    fl->mxcsr_mask = ufl->mxcsr_mask;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 16; j++) {
            fl->st_mm[i][j] = ufl->st_mm[i][j];
            fl->mmx_1[i][j] = ufl->mmx_1[i][j];
            fl->mmx_2[i][j] = ufl->mmx_2[i][j];
        }
    }
    for (i = 0; i < 96; i++) {
        fl->pad[i] = ufl->pad[i];
    }
    return 0;
}

int vcpu_get_msr(struct vcpu_t *vcpu, uint64_t entry, uint64_t *val)
{
    return handle_msr_read(vcpu, entry, val);
}

int vcpu_set_msr(struct vcpu_t *vcpu, uint64_t entry, uint64_t val)
{
    return handle_msr_write(vcpu, entry, val, true);
}

int vcpu_set_cpuid(struct vcpu_t *vcpu, hax_cpuid *cpuid_info)
{
    hax_log(HAX_LOGI, "%s: vCPU #%u is setting guest CPUID.\n", __func__,
            vcpu->vcpu_id);

    if (cpuid_info->total == 0 || cpuid_info->total > HAX_MAX_CPUID_ENTRIES) {
        hax_log(HAX_LOGW, "%s: No entry or exceeds maximum: total = %lu.\n",
                __func__, cpuid_info->total);
        return -EINVAL;
    }

    if (vcpu->is_running) {
        hax_log(HAX_LOGW, "%s: Cannot set CPUID: vcpu->is_running = %llu.\n",
                __func__, vcpu->is_running);
        return -EFAULT;
    }

    cpuid_set_guest_features(vcpu->guest_cpuid, cpuid_info);

    return 0;
}

void vcpu_debug(struct vcpu_t *vcpu, struct hax_debug_t *debug)
{
    bool hbreak_enabled = false;

    if (debug->control & HAX_DEBUG_ENABLE) {
        vcpu->debug_control = debug->control;

        // Hardware breakpoints
        if (debug->control & HAX_DEBUG_USE_HW_BP) {
            hbreak_enabled = true;
        }
    } else {
        vcpu->debug_control = 0;
    }

    if (hbreak_enabled) {
        vcpu->state->_dr0 = debug->dr[0];
        vcpu->state->_dr1 = debug->dr[1];
        vcpu->state->_dr2 = debug->dr[2];
        vcpu->state->_dr3 = debug->dr[3];
        vcpu->state->_dr7 = fix_dr7(debug->dr[7]);
    } else {
        vcpu->state->_dr0 = 0;
        vcpu->state->_dr1 = 0;
        vcpu->state->_dr2 = 0;
        vcpu->state->_dr3 = 0;
        vcpu->state->_dr7 = DR7_SETBITS;
    }
    vcpu->state->_dr6 = DR6_SETBITS;

    vcpu->debug_control_dirty = 1;
    vcpu->dr_dirty = 1;
    vcpu_update_exception_bitmap(vcpu);
};

static void vcpu_dump(struct vcpu_t *vcpu, uint32_t mask, const char *caption)
{
    vcpu_vmread_all(vcpu);
    vcpu_state_dump(vcpu);
}

static void vcpu_state_dump(struct vcpu_t *vcpu)
{
    int i;
    struct gstate *gstate = &vcpu->gstate;
    bool em64t_support = cpu_has_feature(X86_FEATURE_EM64T);
    struct fx_layout *gfx = (struct fx_layout *)hax_page_va(gstate->gfxpage);

    hax_log(HAX_LOGW,
            "RIP: %08llx  RSP: %08llx  RFLAGS: %08llx\n"
            "RAX: %08llx  RBX: %08llx  RCX: %08llx  RDX: %08llx\n"
            "RSI: %08llx  RDI: %08llx  RBP: %08llx\n"
            "CR0: %08llx  CR2: %08llx  CR3: %08llx  CR4: %08llx\n"
            "CS: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "SS: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "DS: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "ES: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "FS: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "GS: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "TR: %4x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "LDT: %3x  AR: %4x  BAS: %08llx  LIM: %08x\n"
            "GDT: %08llx  LIM: %08x\n"
            "IDT: %08llx  LIM: %08x\n"
            "EFER: %08x\n\n",
            vcpu->state->_rip,
            vcpu->state->_rsp,
            vcpu->state->_rflags,
            vcpu->state->_rax,
            vcpu->state->_rbx,
            vcpu->state->_rcx,
            vcpu->state->_rdx,
            vcpu->state->_rsi,
            vcpu->state->_rdi,
            vcpu->state->_rbp,
            vcpu->state->_cr0,
            vcpu->state->_cr2,
            vcpu->state->_cr3,
            vcpu->state->_cr4,
            vcpu->state->_cs.selector,
            vcpu->state->_cs.ar,
            vcpu->state->_cs.base,
            vcpu->state->_cs.limit,
            vcpu->state->_ss.selector,
            vcpu->state->_ss.ar,
            vcpu->state->_ss.base,
            vcpu->state->_ss.limit,
            vcpu->state->_ds.selector,
            vcpu->state->_ds.ar,
            vcpu->state->_ds.base,
            vcpu->state->_ds.limit,
            vcpu->state->_es.selector,
            vcpu->state->_es.ar,
            vcpu->state->_es.base,
            vcpu->state->_es.limit,
            vcpu->state->_fs.selector,
            vcpu->state->_fs.ar,
            vcpu->state->_fs.base,
            vcpu->state->_fs.limit,
            vcpu->state->_gs.selector,
            vcpu->state->_gs.ar,
            vcpu->state->_gs.base,
            vcpu->state->_gs.limit,
            vcpu->state->_tr.selector,
            vcpu->state->_tr.ar,
            vcpu->state->_tr.base,
            vcpu->state->_tr.limit,
            vcpu->state->_ldt.selector,
            vcpu->state->_ldt.ar,
            vcpu->state->_ldt.base,
            vcpu->state->_ldt.limit,
            vcpu->state->_gdt.base,
            vcpu->state->_gdt.limit,
            vcpu->state->_idt.base,
            vcpu->state->_idt.limit,
            (uint32_t)vcpu->state->_efer);

    // Dump FPU state
    hax_log(HAX_LOGW,
            "FX LAYOUT:\n"
            "FCW:    %08x  FSW:    %08x  FTW: %08x  RES1: %08x  FOP: %08x\n"
            "FPU_IP: %08llx  FPU_DP: %08llx\n"
            "MXCSR:  %08x  MXCSR_MASK: %08x\n",
            gfx->fcw, gfx->fsw, gfx->ftw, gfx->res1, gfx->fop, gfx->fpu_ip,
            gfx->fpu_dp, gfx->mxcsr, gfx->mxcsr_mask);

    for (i = 0; i < 8; i++) {
        hax_log(HAX_LOGW, "st_mm[%d]: %02x%02x%02x%02x %02x%02x%02x%02x"
                " %02x%02x%02x%02x %02x%02x%02x%02x\n", i,
                gfx->st_mm[i][0], gfx->st_mm[i][1], gfx->st_mm[i][2],
                gfx->st_mm[i][3], gfx->st_mm[i][4], gfx->st_mm[i][5],
                gfx->st_mm[i][6], gfx->st_mm[i][7], gfx->st_mm[i][8],
                gfx->st_mm[i][9], gfx->st_mm[i][10], gfx->st_mm[i][11],
                gfx->st_mm[i][12], gfx->st_mm[i][13], gfx->st_mm[i][14],
                gfx->st_mm[i][15]);
    }

    for (i = 0; i < 8; i++) {
        hax_log(HAX_LOGW, "mmx_1[%d]: %02x%02x%02x%02x %02x%02x%02x%02x"
                " %02x%02x%02x%02x %02x%02x%02x%02x\n", i,
                gfx->mmx_1[i][0], gfx->mmx_1[i][1], gfx->mmx_1[i][2],
                gfx->mmx_1[i][3], gfx->mmx_1[i][4], gfx->mmx_1[i][5],
                gfx->mmx_1[i][6], gfx->mmx_1[i][7], gfx->mmx_1[i][8],
                gfx->mmx_1[i][9], gfx->mmx_1[i][10], gfx->mmx_1[i][11],
                gfx->mmx_1[i][12], gfx->mmx_1[i][13], gfx->mmx_1[i][14],
                gfx->mmx_1[i][15]);

    }

    for (i = 0; i < 8; i++) {
        hax_log(HAX_LOGW, "mmx_2[%d]: %02x%02x%02x%02x %02x%02x%02x%02x"
                " %02x%02x%02x%02x %02x%02x%02x%02x\n", i,
                gfx->mmx_2[i][0], gfx->mmx_2[i][1], gfx->mmx_2[i][2],
                gfx->mmx_2[i][3], gfx->mmx_2[i][4], gfx->mmx_2[i][5],
                gfx->mmx_2[i][6], gfx->mmx_2[i][7], gfx->mmx_2[i][8],
                gfx->mmx_2[i][9], gfx->mmx_2[i][10], gfx->mmx_2[i][11],
                gfx->mmx_2[i][12], gfx->mmx_2[i][13], gfx->mmx_2[i][14],
                gfx->mmx_2[i][15]);

    }

    for (i = 0; i < 96; i += 8) {
        hax_log(HAX_LOGW, "pad: %02x%02x%02x%02x %02x%02x%02x%02x\n",
                gfx->pad[i], gfx->pad[i+1], gfx->pad[i+2],
                gfx->pad[i+3], gfx->pad[i+4], gfx->pad[i+5],
                gfx->pad[i+6], gfx->pad[i+7]);
    }

    // Dump MSRs
    for (i = 0; i < NR_GMSR; i++) {
        if (em64t_support || !is_emt64_msr(gstate->gmsr[i].entry)) {
            hax_log(HAX_LOGW, "MSR %08llx:%08llx\n", gstate->gmsr[i].entry,
                    gstate->gmsr[i].value);
        }
    }

    if (cpu_has_feature(X86_FEATURE_RDTSCP)) {
        hax_log(HAX_LOGW, "MSR IA32_TSC_AUX:%08llx\n", gstate->tsc_aux);
    }

    if (!hax->apm_version)
        return;

    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32_t msr = (uint32_t)(IA32_PMC0 + i);
        hax_log(HAX_LOGW, "MSR %08x:%08llx\n", msr, gstate->apm_pmc_msrs[i]);

        msr = (uint32_t)(IA32_PERFEVTSEL0 + i);
        hax_log(HAX_LOGW, "MSR %08x:%08llx\n", msr, gstate->apm_pes_msrs[i]);
    }
}

int vcpu_interrupt(struct vcpu_t *vcpu, uint8_t vector)
{
    hax_set_pending_intr(vcpu, vector);
    return 1;
}

// Simply to cause vmexit to vcpu, if any vcpu is running on this physical CPU
static void _vcpu_take_off(void *param)
{
    hax_cpu_pos_t *target = (hax_cpu_pos_t *)param;

    hax_log(HAX_LOGD, "[#%d] _vcpu_take_off\n", current_cpu_data()->cpu_id);
    if (target)
        hax_log(HAX_LOGD, "_vcpu_take_off on cpu (group-%d bit-%d)\n",
                target->group, target->bit);
    else
        hax_log(HAX_LOGD, "_vcpu_take_off on all cpu");

    return;
}

// Pause the vcpu, wait till vcpu is not running, or back to QEMU
int vcpu_pause(struct vcpu_t *vcpu)
{
    if (!vcpu)
        return -1;

    vcpu->paused = 1;
    hax_smp_mb();
    if (vcpu->is_running) {
        hax_smp_call_function(&cpu_online_map, _vcpu_take_off, NULL);
    }

    return 0;
}

int vcpu_takeoff(struct vcpu_t *vcpu)
{
    uint32_t cpu_id;
    hax_cpu_pos_t target = {0};

    // Don't change the sequence unless you are sure
    if (vcpu->is_running) {
        cpu_id = vcpu->cpu_id;
        hax_assert(cpu_id != hax_cpu_id());
        cpu2cpumap(cpu_id, &target);
        // If not considering Windows XP, definitely we don't need this
        hax_smp_call_function(&cpu_online_map, _vcpu_take_off, &target);
    }

    return 0;
}

int vcpu_unpause(struct vcpu_t *vcpu)
{
    vcpu->paused = 0;
    return 0;
}

int hax_put_vcpu(struct vcpu_t *vcpu)
{
    int count;

    count = hax_atomic_dec(&vcpu->ref_count);

    if (count == 1) {
        vcpu_pause(vcpu);

        hax_vcpu_destroy_host(vcpu, vcpu->vcpu_host);
        vcpu_teardown(vcpu);
    }

    return count--;
}

int vcpu_event_pending(struct vcpu_t *vcpu)
{
    return !hax_test_and_clear_bit(
            0, (uint64_t *)&vcpu->tunnel->user_event_pending);
}

void vcpu_set_panic(struct vcpu_t *vcpu)
{
    vcpu->panicked = 1;
}

static int vcpu_set_apic_base(struct vcpu_t *vcpu, uint64_t val)
{
    struct gstate *gstate = &vcpu->gstate;

    if (val & ~APIC_BASE_MASK) {
        hax_log(HAX_LOGE, "Try to set reserved bits of IA32_APIC_BASE MSR and"
                " failed to set APIC base msr to 0x%llx.\n", val);
        return -EINVAL;
    }

    if ((val & APIC_BASE_ADDR_MASK) != APIC_BASE_DEFAULT_ADDR) {
        hax_log(HAX_LOGE, "APIC base cannot be relocated to 0x%llx.\n",
                val & APIC_BASE_ADDR_MASK);
        return -EINVAL;
    }

    if (!(val & APIC_BASE_ENABLE)) {
        hax_log(HAX_LOGW, "APIC is disabled for vCPU %u.\n", vcpu->vcpu_id);
    }

    if (val & APIC_BASE_BSP) {
        if (vcpu_is_bsp(vcpu)) {
            hax_log(HAX_LOGI, "vCPU %u is set to bootstrap processor.\n",
                    vcpu->vcpu_id);
        } else {
            hax_log(HAX_LOGE, "Bootstrap processor is vCPU %u and cannot "
                    "changed to vCPU %u.\n", vcpu->vm->bsp_vcpu_id,
                    vcpu->vcpu_id);
            return -EINVAL;
        }
    }

    gstate->apic_base = val;

    return 0;
}

static bool vcpu_is_bsp(struct vcpu_t *vcpu)
{
    // TODO: add an API to set bootstrap processor
    return (vcpu->vm->bsp_vcpu_id == vcpu->vcpu_id);
}

static void vcpu_init_cpuid(struct vcpu_t *vcpu)
{
    struct vcpu_t *vcpu_0;

    if (vcpu->vcpu_id != 0) {
        vcpu_0 = hax_get_vcpu(vcpu->vm->vm_id, 0, 0);
        if (vcpu_0 == NULL) {
            hax_log(HAX_LOGE, "%s: initializing vCPU #%u with exception as "
                    "vCPU #0 is absent.\n", __func__, vcpu->vcpu_id);
            return;
        }
        vcpu->guest_cpuid = vcpu_0->guest_cpuid;
        hax_log(HAX_LOGI, "%s: referenced vcpu[%u].guest_cpuid to vcpu[%u].\n",
                __func__, vcpu->vcpu_id, vcpu_0->vcpu_id);
        return;
    }

    cpuid_guest_init(vcpu->guest_cpuid);
    hax_log(HAX_LOGI, "%s: initialized vcpu[%u].guest_cpuid with default "
            "feature set.\n", __func__, vcpu->vcpu_id);
}

static int vcpu_alloc_cpuid(struct vcpu_t *vcpu)
{
    // Only the first vCPU will allocate the CPUID memory, and other vCPUs will
    // share this memory.
    if (vcpu->vcpu_id != 0)
        return 1;

    vcpu->guest_cpuid = hax_vmalloc(sizeof(hax_cpuid_t), HAX_MEM_NONPAGE);
    if (vcpu->guest_cpuid == NULL)
        return 0;

    return 1;
}

static void vcpu_free_cpuid(struct vcpu_t *vcpu)
{
    if (vcpu->vcpu_id != 0) {
        vcpu->guest_cpuid = NULL;
        hax_log(HAX_LOGI, "%s: dereferenced vcpu[%u].guest_cpuid from vcpu[0]."
                "\n", __func__, vcpu->vcpu_id);
        return;
    }

    if (vcpu->guest_cpuid == NULL) {
        hax_log(HAX_LOGW, "%s: already freed vcpu[%u].guest_cpuid.\n",
                __func__, vcpu->vcpu_id);
        return;
    }

    hax_vfree(vcpu->guest_cpuid, sizeof(hax_cpuid_t));
    vcpu->guest_cpuid = NULL;
    hax_log(HAX_LOGI, "%s: freed vcpu[%u].guest_cpuid.\n", __func__,
            vcpu->vcpu_id);
}
