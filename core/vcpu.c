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
#include "include/ia32.h"
#include "include/vcpu.h"
#include "include/mtrr.h"
#include "include/vmx.h"
#include "include/cpu.h"
#include "include/vm.h"
#include "include/debug.h"
#include "include/dump_vmcs.h"

#include "include/intr.h"
#include "include/vtlb.h"
#include "include/ept.h"
#include "include/paging.h"
#include "include/hax_core_interface.h"
#include "include/hax_driver.h"

uint64 gmsr_list[NR_GMSR] = {
    IA32_STAR,
    IA32_LSTAR,
    IA32_CSTAR,
    IA32_SF_MASK,
    IA32_KERNEL_GS_BASE
};

uint64 hmsr_list[NR_HMSR] = {
    IA32_EFER,
    IA32_STAR,
    IA32_LSTAR,
    IA32_CSTAR,
    IA32_SF_MASK,
    IA32_KERNEL_GS_BASE
};

uint64 emt64_msr[NR_EMT64MSR] = {
    IA32_STAR,
    IA32_LSTAR,
    IA32_CSTAR,
    IA32_SF_MASK,
    IA32_KERNEL_GS_BASE
};

extern uint32 pw_reserved_bits_high_mask;

static void vcpu_init(struct vcpu_t *vcpu);
static void vcpu_prepare(struct vcpu_t *vcpu);

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
static int null_handler(struct vcpu_t *vcpu, struct hax_tunnel *hun);

static void advance_rip(struct vcpu_t *vcpu);
static void handle_machine_check(struct vcpu_t *vcpu);

static void handle_cpuid_virtual(struct vcpu_t *vcpu, uint32 eax, uint32 ecx);
static void handle_mem_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static void check_flush(struct vcpu_t *vcpu, uint32 bits);
static void vmwrite_efer(struct vcpu_t *vcpu);

static int handle_msr_read(struct vcpu_t *vcpu, uint32 msr, uint64 *val);
static int handle_msr_write(struct vcpu_t *vcpu, uint32 msr, uint64 val);
static void handle_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun);
static void vcpu_dump(struct vcpu_t *vcpu, uint32 mask, const char *caption);
static void vcpu_state_dump(struct vcpu_t *vcpu);
static void vcpu_enter_fpu_state(struct vcpu_t *vcpu);

static int vcpu_set_apic_base(struct vcpu_t *vcpu, uint64 val);
static bool vcpu_is_bsp(struct vcpu_t *vcpu);

static uint32 get_seg_present(uint32 seg)
{
    mword ldtr_base;
    struct seg_desc_t *seg_desc;
    struct hstate *hstate = &get_cpu_data(hax_cpuid())->hstate;

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
    uint16 seg = hstate->gs;

    ldtr_base = get_kernel_ldtr_base();
    seg_desc = (struct seg_desc_t *)ldtr_base + (seg >> 3);
    if (seg_desc->_present == 0)
        seg_desc->_raw = hstate->fake_gs;
    set_kernel_gs(seg);
    ia32_wrmsr(IA32_GS_BASE, hstate->gs_base);
    seg_desc->_raw = 0;
}

static void get_segment_desc_t(segment_desc_t *sdt, uint32 s, uint64 b,
                               uint32 l, uint32 a)
{
    sdt->selector = s;
    sdt->base = b;
    sdt->limit = l;
    sdt->ar = a;
}

static inline void set_gdt(struct vcpu_state_t *state, uint64 base,
                           uint64 limit)
{
    state->_gdt.base = base;
    state->_gdt.limit = limit;
}

static inline void set_idt(struct vcpu_state_t *state, uint64 base,
                           uint64 limit)
{
    state->_idt.base = base;
    state->_idt.limit = limit;
}

static uint64 vcpu_read_cr(struct vcpu_state_t *state, uint32 n)
{
    uint64 val = 0;

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
            hax_error("Unsupported CR%d access\n", n);
            break;
        }
    }

    hax_debug("vcpu_read_cr cr %x val %llx\n", n, val);

    return val;
}

static void vcpu_write_cr(struct vcpu_state_t *state, uint32 n, uint64 val)
{
    hax_debug("vcpu_write_cr cr %x val %llx\n", n, val);

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
            hax_error("write_cr: Unsupported CR%d access\n", n);
            break;
        }
    }
}

void * vcpu_vmcs_va(struct vcpu_t *vcpu)
{
    return hax_page_va(vcpu->vmcs_page);
}

paddr_t vcpu_vmcs_pa(struct vcpu_t *vcpu)
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
                    uint8 *iobuf)
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
    uint32 vpid_seed_bits = sizeof(vcpu->vm->vpid_seed) * 8;
    uint8 bit, max_bit;

    max_bit = vpid_seed_bits > 0xff ? 0xff : vpid_seed_bits;

    if (0 != vcpu->vpid) {
        hax_warning("vcpu_vpid_alloc: vcpu %u in vm %d already has a valid "
                    "VPID 0x%x.\n", vcpu->vcpu_id, vcpu->vm->vm_id, vcpu->vpid);
        return -1;
    }

    for (bit = 0; bit < max_bit; bit++) {
        if (!hax_test_and_set_bit(bit, (uint64 *)vcpu->vm->vpid_seed))
            break;
    }

    if (bit == max_bit) {
        // No available VPID resource
        hax_error("vcpu_vpid_alloc: no available vpid resource. vcpu: %u, "
                  "vm: %d\n", vcpu->vcpu_id, vcpu->vm->vm_id);
        return -2;
    }

    /*
     * We split vpid as high byte and low byte, the vpid seed is used to
     * generate low byte. We use the index of first zero bit in vpid seed plus 1
     * as the value of low_byte, and use vcpu->vm->vm_id as the value of high
     * byte.
     * Note: vpid can't be zero.
     */
    vcpu->vpid = (uint16)(vcpu->vm->vm_id << 8) + (uint16)(bit + 1);
    hax_info("vcpu_vpid_alloc: succeed! vpid: 0x%x. vcpu_id: %u, vm_id: %d.\n",
             vcpu->vpid, vcpu->vcpu_id, vcpu->vm->vm_id);

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
    uint8 bit = (vcpu->vpid & 0xff) - 1;

    if (0 == vcpu->vpid) {
        hax_warning("vcpu_vpid_free: vcpu %u in vm %d does not have a valid "
                    "VPID.\n", vcpu->vcpu_id, vcpu->vm->vm_id);
        return -1;
    }

    hax_info("vcpu_vpid_free: Clearing bit: 0x%x, vpid_seed: 0x%llx. "
             "vcpu_id: %u, vm_id: %d.\n", bit, *(uint64 *)vcpu->vm->vpid_seed,
             vcpu->vcpu_id, vcpu->vm->vm_id);
    if (0 != hax_test_and_clear_bit(bit, (uint64 *)(vcpu->vm->vpid_seed))) {
        hax_warning("vcpu_vpid_free: bit for vpid 0x%x of vcpu %u in vm %d was "
                    "already clear.\n", vcpu->vpid, vcpu->vcpu_id,
                    vcpu->vm->vm_id);
    }
    vcpu->vpid = 0;

    return 0;
}

static int (*handler_funcs[])(struct vcpu_t *vcpu, struct hax_tunnel *htun) = {
    exit_exc_nmi,
    exit_interrupt,
    exit_triple_fault,
    0, 0, 0, 0,
    exit_interrupt_window,                      // Interrupt window
    exit_interrupt_window,                      // NMI window
    0,
    exit_cpuid,
    0,
    exit_hlt,
    0,
    exit_invlpg,
    0,
    exit_rdtsc,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,            // 17 ... 27
    exit_cr_access,
    exit_dr_access,
    exit_io_access,
    exit_msr_read,
    exit_msr_write,
    exit_invalid_guest_state,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 34 ... 47
    exit_ept_violation,
    exit_ept_misconfiguration,
    0, 0, 0, 0, 0, 0                            // 50 ... 55
};

static int nr_handlers = ARRAY_ELEMENTS(handler_funcs);

struct vcpu_t *vcpu_create(struct vm_t *vm, void *vm_host, int vcpu_id)
{
    struct hax_tunnel_info info;
    struct vcpu_t *vcpu;

    hax_debug("vcpu_create vcpu_id %x\n", vcpu_id);

    if (!valid_vcpu_id(vcpu_id))
        return NULL;

    vcpu = (struct vcpu_t *)hax_vmalloc(sizeof(struct vcpu_t), HAX_MEM_NONPAGE);
    if (!vcpu)
        goto fail_0;

    hax_clear_panic_log(vcpu);

    memset(vcpu, 0, sizeof(struct vcpu_t));

    if (hax_vcpu_setup_hax_tunnel(vcpu, &info) < 0) {
        hax_error("HAX: cannot setup hax_tunnel for vcpu.\n");
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

    if (hax_vcpu_create_host(vcpu, vm_host, vm->vm_id, vcpu_id))
        goto fail_7;

    vcpu->cpu_id = hax_cpuid();
    vcpu->vcpu_id = vcpu_id;
    vcpu->is_running = 0;
    vcpu->vm = vm;
    // Must ensure it is called before fill_common_vmcs is called
    vcpu_vpid_alloc(vcpu);

    // Prepare guest environment
    vcpu_init(vcpu);

    // First time vmclear/vmptrld on current CPU
    vcpu_prepare(vcpu);

    // Init IA32_APIC_BASE MSR
    vcpu->gstate.apic_base = APIC_BASE_DEFAULT_ADDR | APIC_BASE_ENABLE;
    if (vcpu_is_bsp(vcpu)) {
        vcpu->gstate.apic_base |= APIC_BASE_BSP;
    }

    // Publish the vcpu
    hax_mutex_lock(vm->vm_lock);
    hax_list_add(&vcpu->vcpu_list, &vm->vcpu_list);
    // The caller should get_vm thus no race with vm destroy
    hax_atomic_add(&vm->ref_count, 1);
    hax_mutex_unlock(vm->vm_lock);

    hax_debug("HAX: vcpu %d is created.\n", vcpu->vcpu_id);
    return vcpu;
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
    hax_error("HAX: Cannot allocate memory to create vcpu.\n");
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

#ifdef CONFIG_HAX_EPT2
    if (vcpu->mmio_fetch.kva) {
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &vcpu->mmio_fetch.kmap);
    }
#endif  // CONFIG_HAX_EPT2

    // TODO: we should call invvpid after calling vcpu_vpid_free().
    vcpu_vpid_free(vcpu);

    if (vcpu->gstate.gfxpage) {
        hax_free_pages(vcpu->gstate.gfxpage);
    }
    hax_free_pages(vcpu->vmcs_page);
    hax_vfree(vcpu->state, sizeof(struct vcpu_state_t));
    vcpu_vtlb_free(vcpu);
    hax_mutex_free(vcpu->tmutex);
    hax_vfree(vcpu, sizeof(struct vcpu_t));

    hax_info("HAX: vcpu %d is teardown.\n", vcpu_id);
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
    hax_mutex_lock(vcpu->tmutex);

    // TODO: mtrr ?
    vcpu->cr_pat = 0x0007040600070406ULL;
    vcpu->cpuid_features_flag_mask = 0xffffffffffffffffULL;
    vcpu->cur_state = GS_VALID;
    vmx(vcpu, entry_exception_vector) = ~0u;
    vmx(vcpu, cr0_mask) = 0;
    vmx(vcpu, cr0_shadow) = 0;
    vmx(vcpu, cr4_mask) = 0;
    vmx(vcpu, cr4_shadow) = 0;

    vcpu->ref_count = 1;

    vcpu->tsc_offset = 0ULL - rdtsc();

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
    state->_dr6 = 0xffff0ff0;
    state->_dr7 = 0x00000400;

    hax_mutex_unlock(vcpu->tmutex);
}

#ifdef DEBUG_HOST_STATE
static int check_panic(void)
{
    char *kernel_panic = NULL;
    return 0;
}

// Code to check the host state between vmluanch and vmexit
static uint32 get_seg_avail(uint32 seg)
{
    mword gdtr_base;
    struct seg_desc_t *sd;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);

    return sd->_avl;
}

static void dump_segment(uint32 seg)
{
    struct seg_desc_t *sd;
    mword gdtr_base;

    gdtr_base = get_kernel_gdtr_base();
    sd = (struct seg_desc_t *)gdtr_base + (seg >> 3);
    hax_debug("seg %x value %llx\n", seg, sd->_raw);
}

static int check_cs(uint32 seg)
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

static int check_data_seg(uint32 seg)
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

static int check_stack_seg(uint32 seg)
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

static int check_tr_seg(uint32 seg)
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

static int check_fgs_seg(uint32 seg, uint fs)
{
    mword gdtr_base;
    mword desc_base;
    struct seg_desc_t *sd;
    mword base;

    if (seg == 0) {
        hax_debug("fgs_seg seg is %x fs %x\n", seg, fs);
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
        // hax_debug("%s base address mismatch base %llx sd %llx\n",
        //           fs ? "fs" : "gs", base, sd->_raw);
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
    uint64 value;
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
            hax_debug("CS does not pass the check.\n");
            dump_segment(hsc->cs);
            // check_panic();
        }
        if (check_stack_seg(hsc->ss)) {
            hax_debug("SS does not pass the check.\n");
            dump_segment(hsc->ss);
            // check_panic();
        }
        if (check_fgs_seg(hsc->fs, 1)) {
            hax_debug("FS does not pass the check.\n");
            dump_segment(hsc->fs);
            // check_panic();
        }
        if (check_fgs_seg(hsc->gs, 0)) {
            hax_debug("GS does not pass the check.\n");
            dump_segment(hsc->gs);
            // check_panic();
        }
        if (check_data_seg(hsc->ds) || check_data_seg(hsc->es)) {
            hax_debug("DS or ES does not pass the check.\n");
            dump_segment(hsc->ds);
            dump_segment(hsc->es);
            // check_panic();
        }
        if (check_tr_seg(hsc->tr)) {
            hax_debug("TR does not pass the check.\n");
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
        hax_debug("The previous and next is not same.\n");
        dump_hsc_state(pre);
        dump_hsc_state(post);
        check_panic();
    }
    return 0;
}
#endif

static int is_emt64_msr(uint64 entry)
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

    for (i = 0; i < NR_GMSR; i++) {
        gstate->gmsr[i].entry = gmsr_list[i];
        if (cpu_has_emt64_support() || !is_emt64_msr(gmsr_list[i])) {
            gstate->gmsr[i].value = ia32_rdmsr(gstate->gmsr[i].entry);
        }
    }

    if (!hax->apm_version)
        return;

    // APM v1: save IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32 msr = (uint32)(IA32_PMC0 + i);
        gstate->apm_pmc_msrs[i] = ia32_rdmsr(msr);
        msr = (uint32)(IA32_PERFEVTSEL0 + i);
        gstate->apm_pes_msrs[i] = ia32_rdmsr(msr);
    }
}

void load_guest_msr(struct vcpu_t *vcpu)
{
    int i;
    struct gstate *gstate = &vcpu->gstate;

    for (i = 0; i < NR_GMSR; i++) {
        if (cpu_has_emt64_support() || !is_emt64_msr(gstate->gmsr[i].entry)) {
            ia32_wrmsr(gstate->gmsr[i].entry, gstate->gmsr[i].value);
        }
    }

    if (!hax->apm_version)
        return;

    // APM v1: restore IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32 msr = (uint32)(IA32_PMC0 + i);
        ia32_wrmsr(msr, gstate->apm_pmc_msrs[i]);
        msr = (uint32)(IA32_PERFEVTSEL0 + i);
        ia32_wrmsr(msr, gstate->apm_pes_msrs[i]);
    }
}

static void save_host_msr(struct vcpu_t *vcpu)
{
    int i;
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    for (i = 0; i < NR_HMSR; i++) {
        hstate->hmsr[i].entry = hmsr_list[i];
        if (cpu_has_emt64_support() || !is_emt64_msr(hmsr_list[i])) {
            hstate->hmsr[i].value = ia32_rdmsr(hstate->hmsr[i].entry);
        }
    }

    if (!hax->apm_version)
        return;

    // APM v1: save IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32 msr = (uint32)(IA32_PMC0 + i);
        hstate->apm_pmc_msrs[i] = ia32_rdmsr(msr);
        msr = (uint32)(IA32_PERFEVTSEL0 + i);
        hstate->apm_pes_msrs[i] = ia32_rdmsr(msr);
    }
}

static void load_host_msr(struct vcpu_t *vcpu)
{
    int i;
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    for (i = 0; i < NR_HMSR; i++) {
        if (cpu_has_emt64_support() || !is_emt64_msr(hstate->hmsr[i].entry)) {
            ia32_wrmsr(hstate->hmsr[i].entry, hstate->hmsr[i].value);
        }
    }

    if (!hax->apm_version)
        return;

    // APM v1: restore IA32_PMCx and IA32_PERFEVTSELx
    for (i = 0; i < (int)hax->apm_general_count; i++) {
        uint32 msr = (uint32)(IA32_PMC0 + i);
        ia32_wrmsr(msr, hstate->apm_pmc_msrs[i]);
        msr = (uint32)(IA32_PERFEVTSEL0 + i);
        ia32_wrmsr(msr, hstate->apm_pes_msrs[i]);
    }
}

void vcpu_save_host_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;

    // In case we do not know the specific operations with different OSes,
    // we save all of them at the initial time
    uint16 gs = get_kernel_gs();
    uint16 fs = get_kernel_fs();

    get_kernel_gdt(&hstate->host_gdtr);
    get_kernel_idt(&hstate->host_idtr);

    vmwrite(vcpu, HOST_CR3, get_cr3());
    vmwrite(vcpu, HOST_CR4, get_cr4());

    hstate->_efer = ia32_rdmsr(IA32_EFER);

    if (vmx(vcpu, exit_ctls) & EXIT_CONTROL_LOAD_EFER) {
        vmwrite(vcpu, HOST_EFER, hstate->_efer);
    }

#ifdef __x86_64__
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
        hax_debug("Kernel SS %x with 0x7\n", get_kernel_ss());
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
#ifdef __x86_64__
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
        hax_debug("fs %x\n", fs);
        hstate->fs = fs;
        hstate->fs_base = ia32_rdmsr(IA32_FS_BASE);
        hstate->seg_valid |= HOST_SEG_VALID_FS;
        vmwrite(vcpu, HOST_FS_SELECTOR, 0);
    } else {
        vmwrite(vcpu, HOST_FS_SELECTOR, fs);
#ifdef __x86_64__
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
    vcpu_enter_fpu_state(vcpu);
    // CR0 should be written after host fpu state is saved
    vmwrite(vcpu, HOST_CR0, get_cr0());
}

static void fill_common_vmcs(struct vcpu_t *vcpu)
{
    uint32 pin_ctls;
    uint32 pcpu_ctls;
    uint32 scpu_ctls;
    uint32 exc_bitmap;
    uint32 exit_ctls = 0;
    uint32 entry_ctls;
    uint32 vmcs_err = 0;
    uint i;
    preempt_flag flags;
    struct per_cpu_data *cpu_data;

    // How to determine the capability
    pin_ctls = EXT_INTERRUPT_EXITING | NMI_EXITING;

    pcpu_ctls = IO_BITMAP_ACTIVE | MSR_BITMAP_ACTIVE |
                INTERRUPT_WINDOW_EXITING | USE_TSC_OFFSETTING | HLT_EXITING |
                SECONDARY_CONTROLS;

    scpu_ctls = ENABLE_EPT;

    // If UG exists, we want it.
    if (hax->ug_enable_flag) {
        scpu_ctls |= UNRESTRICTED_GUEST;
    }

    // If vpid exists, we want it.
    if ((ia32_rdmsr(IA32_VMX_PROCBASED_CTLS) &
        ((uint64)SECONDARY_CONTROLS << 32)) != 0) {
        if ((ia32_rdmsr(IA32_VMX_SECONDARY_CTLS) &
            ((uint64)ENABLE_VPID << 32)) != 0) {
            if (0 != vcpu->vpid) {
                scpu_ctls |= ENABLE_VPID;
                vmwrite(vcpu, VMX_VPID, vcpu->vpid);
            }
        }
    }

    exc_bitmap = (1u << EXC_MACHINE_CHECK) | (1u << EXC_NOMATH);

#ifdef __x86_64__
    exit_ctls = EXIT_CONTROL_HOST_ADDR_SPACE_SIZE | EXIT_CONTROL_LOAD_EFER |
                EXIT_CONTROL_SAVE_DEBUG_CONTROLS;
#endif

#ifdef __i386__
    if (is_compatible()) {
        exit_ctls = EXIT_CONTROL_HOST_ADDR_SPACE_SIZE | EXIT_CONTROL_LOAD_EFER |
                    EXIT_CONTROL_SAVE_DEBUG_CONTROLS;
    } else {
        exit_ctls = EXIT_CONTROL_SAVE_DEBUG_CONTROLS;
    }
#endif

    entry_ctls = ENTRY_CONTROL_LOAD_DEBUG_CONTROLS;

    if ((vmcs_err = load_vmcs(vcpu, &flags))) {
        hax_panic_vcpu(vcpu, "load_vmcs failed while vcpu_prepare: %x",
                       vmcs_err);
        hax_panic_log(vcpu);
        return;
    }
    cpu_data = current_cpu_data();

    // Initialize host area
    vmwrite(vcpu, HOST_CR0, get_cr0());
    vmwrite(vcpu, HOST_CR3, get_cr3());
    vmwrite(vcpu, HOST_CR4, get_cr4());

#ifdef __x86_64__
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

#ifdef __x86_64__
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

#define WRITE_CONTROLS(vcpu, f, v) {                                    \
    uint32 g = v & cpu_data->vmx_info.v##_1 | cpu_data->vmx_info.v##_0; \
    vmwrite(vcpu, f, v = g);                                            \
}

    WRITE_CONTROLS(vcpu, VMX_PIN_CONTROLS, pin_ctls);
    WRITE_CONTROLS(vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS, pcpu_ctls);
    if (pcpu_ctls & SECONDARY_CONTROLS) {
        WRITE_CONTROLS(vcpu, VMX_SECONDARY_PROCESSOR_CONTROLS, scpu_ctls);
    }

    vmwrite(vcpu, VMX_EXCEPTION_BITMAP, exc_bitmap);

    WRITE_CONTROLS(vcpu, VMX_EXIT_CONTROLS, exit_ctls);

    // Check if we can write HOST_EFER
    if (exit_ctls & EXIT_CONTROL_LOAD_EFER) {
        vmwrite(vcpu, HOST_EFER, ia32_rdmsr(IA32_EFER));
    }

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
        hax_panic_vcpu(vcpu, "put_vmcs() failed in vcpu_prepare, %x\n",
                       vmcs_err);
        hax_panic_log(vcpu);
    }
}

static void vcpu_prepare(struct vcpu_t *vcpu)
{
    hax_debug("HAX: vcpu_prepare current %x, CPU %x\n", vcpu->vcpu_id,
              hax_cpuid());
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
    load_kernel_ldt(hstate->ldt_selector);
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

    vcpu_exit_fpu_state(vcpu);
}

/*
 * Copies bits 0, 1, 2, ..., (|size| * 8 - 1) of |src| to the same positions
 * in the 64-bit buffer pointed to by |pdst|, and clears bits (|size| * 8)
 * through 63 of the destination buffer.
 * |size| is the number of bytes to copy, and must be one of {1, 2, 4, 8}.
 * Returns 0 on success, -1 if |size| is invalid.
 */
static int read_low_bits(uint64 *pdst, uint64 src, uint8 size)
{
    // Assume little-endian
    switch (size) {
        case 1: {
            *pdst = (uint8)src;
            break;
        }
        case 2: {
            *pdst = (uint16)src;
            break;
        }
        case 4: {
            *pdst = (uint32)src;
            break;
        }
        case 8: {
            *pdst = src;
            break;
        }
        default: {
            // Should never happen
            hax_error("read_low_bits: Invalid size %u\n", size);
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
static int write_low_bits(uint64 *pdst, uint64 src, uint8 size)
{
    switch (size) {
        case 1: {
            *((uint8 *)pdst) = (uint8)src;
            break;
        }
        case 2: {
            *((uint16 *)pdst) = (uint16)src;
            break;
        }
        case 4: {
            *((uint32 *)pdst) = (uint32)src;
            break;
        }
        case 8: {
            *pdst = src;
            break;
        }
        default: {
            // Should never happen
            hax_error("write_low_bits: Invalid size %u\n", size);
            return -1;
        }
    }
    return 0;
}

static void handle_mmio_post(struct vcpu_t *vcpu, struct hax_fastmmio *hft)
{
    struct vcpu_state_t *state = vcpu->state;

    if (hft->direction)
        return;

    if (vcpu->post_mmio.op == VCPU_POST_MMIO_WRITE_REG) {
        uint64 value;
        // No special treatment for MOVZX, because the source value is
        // automatically zero-extended to 64 bits
        read_low_bits(&value, hft->value, hft->size);
        switch (vcpu->post_mmio.manip) {
            case VCPU_POST_MMIO_MANIP_AND: {
                value &= vcpu->post_mmio.value;
                break;
            }
            case VCPU_POST_MMIO_MANIP_OR: {
                value |= vcpu->post_mmio.value;
                break;
            }
            case VCPU_POST_MMIO_MANIP_XOR: {
                value ^= vcpu->post_mmio.value;
                break;
            }
            default: {
                break;
            }
        }
        // Avoid overwriting high bits of the register if hft->size < 8
        write_low_bits(&state->_regs[vcpu->post_mmio.reg_index], value,
                       hft->size);
    } else if (vcpu->post_mmio.op == VCPU_POST_MMIO_WRITE_MEM) {
        // Assume little-endian
        if (!vcpu_write_guest_virtual(vcpu, vcpu->post_mmio.va, hft->size,
                                      (uint8 *)&hft->value, hft->size, 0)) {
            hax_panic_vcpu(vcpu, "Error writing %u bytes to guest RAM "
                           "(va=0x%llx, value=0x%llx)\n", hft->size,
                           vcpu->post_mmio.va, hft->value);
        }
    } else {
        hax_warning("Unknown post-MMIO operation %d\n", vcpu->post_mmio.op);
    }
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
            hax_panic_vcpu(vcpu, "Unexpected page fault, kill the VM!\n");
            dump_vmcs(vcpu);
        }
    } else {
        switch (htun->io._size) {
            case 1: {
                state->_al = *((uint8 *)vcpu->io_buf);
                break;
            }
            case 2: {
                state->_ax = *((uint16 *)vcpu->io_buf);
                break;
            }
            case 4: {
                state->_eax = *((uint32 *)vcpu->io_buf);
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
    int err = 0;

    hax_mutex_lock(vcpu->tmutex);
    hax_debug("vcpu begin to run....\n");
    // QEMU will do realmode stuff for us
    if (!hax->ug_enable_flag && !(vcpu->state->_cr0 & CR0_PE)) {
        htun->_exit_reason = 0;
        htun->_exit_status = HAX_EXIT_REALMODE;
        hax_debug("Guest is in realmode.\n");
        goto out;
    }
    hax_debug("vcpu begin to run....in PE\n");

    if (htun->_exit_status == HAX_EXIT_IO) {
        handle_io_post(vcpu, htun);
    }
    if (htun->_exit_status == HAX_EXIT_FAST_MMIO) {
        handle_mmio_post(vcpu, (struct hax_fastmmio *)vcpu->io_buf);
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

    hax_debug("vtlb active: cr0, %llx\n", state->_cr0);
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
    preempt_flag flags;
    uint32 interruptibility = vmread(vcpu, GUEST_INTERRUPTIBILITY);
    uint32 vmcs_err = 0;
    if ((vmcs_err = load_vmcs(vcpu, &flags))) {
        hax_panic_vcpu(vcpu, "load_vmcs while advance_rip: %x", vmcs_err);
        hax_panic_log(vcpu);
        return;
    }

    if (interruptibility & 3u) {
        interruptibility &= ~3u;
        vmwrite(vcpu, GUEST_INTERRUPTIBILITY, interruptibility);
    }
    state->_rip += vmread(vcpu, VM_EXIT_INFO_INSTRUCTION_LENGTH);
    vmwrite(vcpu, GUEST_RIP, state->_rip);

    if ((vmcs_err = put_vmcs(vcpu, &flags))) {
        hax_panic_vcpu(vcpu, "put_vmcs while advance_rip: %x\n", vmcs_err);
        hax_panic_log(vcpu);
    }
}

static void advance_rip_step(struct vcpu_t *vcpu, int step)
{
    struct vcpu_state_t *state = vcpu->state;
    preempt_flag flags;
    uint32 interruptibility = vmread(vcpu, GUEST_INTERRUPTIBILITY);
    uint32 vmcs_err = 0;
    if ((vmcs_err = load_vmcs(vcpu, &flags))) {
        hax_panic_vcpu(vcpu, "load_vmcs while advance_rip_step: %x\n",
                       vmcs_err);
        hax_panic_log(vcpu);
        return;
    }

    if (interruptibility & 3u) {
        interruptibility &= ~3u;
        vmwrite(vcpu, GUEST_INTERRUPTIBILITY, interruptibility);
    }
    if (step) {
        state->_rip += step;
        vmwrite(vcpu, GUEST_RIP, state->_rip);
    }

    if ((vmcs_err = put_vmcs(vcpu, &flags))) {
        hax_panic_vcpu(vcpu, "put_vmcs() while advance_rip_step: %x\n",
                       vmcs_err);
        hax_panic_log(vcpu);
    }
}

void vcpu_vmread_all(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32 vmcs_err = 0;

    if (vcpu->cur_state == GS_STALE) {
        preempt_flag flags;

        // CRs were already updated
        // TODO: Always read RIP, RFLAGs, maybe reduce them in future!
        if ((vmcs_err = load_vmcs(vcpu, &flags))) {
            hax_panic_vcpu(vcpu, "load_vmcs failed while vcpu_vmread_all: %x\n",
                           vmcs_err);
            hax_panic_log(vcpu);
            return;
        }

        state->_rip = vmread(vcpu, GUEST_RIP);
        state->_rflags = vmread(vcpu, GUEST_RFLAGS);
        state->_rsp = vmread(vcpu, GUEST_RSP);

        VMREAD_SEG(vcpu, CS, state->_cs);
        VMREAD_SEG(vcpu, DS, state->_ds);
        VMREAD_SEG(vcpu, ES, state->_es);
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
            hax_panic_vcpu(vcpu, "put_vmcs failed while vcpu_vmread_all: %x\n",
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
    vmwrite(vcpu, GUEST_RFLAGS, state->_rflags);
    vmwrite(vcpu, GUEST_RSP, state->_rsp);

    VMWRITE_SEG(vcpu, CS, state->_cs);
    VMWRITE_SEG(vcpu, DS, state->_ds);
    VMWRITE_SEG(vcpu, ES, state->_es);
    VMWRITE_SEG(vcpu, FS, state->_fs);
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
    uint64 cr3 = vcpu->state->_cr3;
    int pdpt_size = (int)sizeof(vcpu->pae_pdptes);
#ifdef CONFIG_HAX_EPT2
    // CR3 is the GPA of the page-directory-pointer table. According to IASDM
    // Vol. 3A 4.4.1, Table 4-7, bits 63..32 and 4..0 of this GPA are ignored.
    uint64 gpa = cr3 & 0xffffffe0;
    int ret;

    // On Mac, the following call may somehow cause the XNU kernel to preempt
    // the current process (QEMU), even if preemption has been previously
    // disabled via hax_disable_preemption() (which is implemented on Mac by
    // simply disabling IRQs). Therefore, it is not safe to call this function
    // with preemption disabled.
    ret = gpa_space_read_data(&vcpu->vm->gpa_space, gpa, pdpt_size,
                              (uint8 *)vcpu->pae_pdptes);
    // The PAE PDPT cannot span two page frames
    if (ret != pdpt_size) {
        hax_error("%s: Failed to read PAE PDPT: cr3=0x%llx, ret=%d\n", __func__,
                  cr3, ret);
        return ret < 0 ? ret : -EIO;
    }
    return 0;
#else // !CONFIG_HAX_EPT2
    uint64 gpfn = (cr3 & 0xfffff000) >> PG_ORDER_4K;
    uint8 *buf, *pdpt;
#if (defined(__MACH__) || defined(_WIN64))
    buf = hax_map_gpfn(vcpu->vm, gpfn);
#else  // !defined(__MACH__) && !defined(_WIN64), i.e. Win32
    buf = hax_map_gpfn(vcpu->vm, gpfn, false, cr3 & 0xfffff000, 1);
#endif  // defined(__MACH__) || defined(_WIN64)
    if (!buf) {
        hax_error("%s: Failed to map guest page frame containing PAE PDPT:"
                  " cr3=0x%llx\n",  __func__, cr3);
        return -ENOMEM;
    }
    pdpt = buf + (cr3 & 0xfe0);
    memcpy_s(vcpu->pae_pdptes, pdpt_size, pdpt, pdpt_size);
#if (defined(__MACH__) || defined(_WIN64))
    hax_unmap_gpfn(buf);
#else  // !defined(__MACH__) && !defined(_WIN64), i.e. Win32
    hax_unmap_gpfn(vcpu->vm, buf, gpfn);
#endif  // defined(__MACH__) || defined(_WIN64)
    return 0;
#endif  // CONFIG_HAX_EPT2
}

static void vmwrite_cr(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    struct per_cpu_data *cpu = current_cpu_data();

    uint32 entry_ctls = vmx(vcpu, entry_ctls_base);
    uint32 pcpu_ctls = vmx(vcpu, pcpu_ctls_base);
    uint32 scpu_ctls = vmx(vcpu, scpu_ctls_base);
    uint32 exc_bitmap = vmx(vcpu, exc_bitmap_base);
    uint64 eptp;

    // If a bit is set here, the same bit of guest CR0 must be fixed to 1 (see
    // IASDM Vol. 3D A.7)
    uint64 cr0_fixed_0 = cpu->vmx_info._cr0_fixed_0;
    // If a bit is clear here, the same bit of guest CR0 must be fixed to 0 (see
    // IASDM Vol. 3D A.7)
    uint64 cr0_fixed_1 = cpu->vmx_info._cr0_fixed_1;

    uint64 cr0 = (state->_cr0 & cr0_fixed_1) |
                 (cr0_fixed_0 & (uint64)~(CR0_PE | CR0_PG));

    uint64 cr0_mask;
    uint64 cr0_shadow;

    // If a bit is set here, the same bit of guest CR4 must be fixed to 1 (see
    // IASDM Vol. 3D A.8)
    uint64 cr4_fixed_0 = cpu->vmx_info._cr4_fixed_0;
    // If a bit is clear here, the same bit of guest CR4 must be fixed to 0 (see
    // IASDM Vol. 3D A.8)
    uint64 cr4_fixed_1 = cpu->vmx_info._cr4_fixed_1;

    uint64 cr4 = ((state->_cr4 | CR4_MCE) & cr4_fixed_1) | cr4_fixed_0;
    uint64 cr4_mask = vmx(vcpu, cr4_mask) | ~(cr4_fixed_0 ^ cr4_fixed_1) |
                      CR4_VMXE | CR4_SMXE | CR4_MCE;
    uint64 cr4_shadow;

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
        hax_debug("vTLB mode, cr0 %llx\n", vcpu->state->_cr0);
        vcpu->mmu->mmu_mode = MMU_MODE_VTLB;
        exc_bitmap |= 1u << EXC_PAGEFAULT;
        cr0 |= CR0_WP;
        cr0_mask |= CR0_WP;
        cr4 |= CR4_PGE | CR4_PAE;
        cr4_mask |= CR4_PGE | CR4_PAE | CR4_PSE;
        pcpu_ctls |= CR3_LOAD_EXITING | CR3_STORE_EXITING | INVLPG_EXITING;
        scpu_ctls &= ~ENABLE_EPT;

        vmwrite(vcpu, GUEST_CR3, vtlb_get_cr3(vcpu));
        state->_efer = 0;
    } else {  // EPTE
#ifndef CONFIG_HAX_EPT2
        struct hax_ept *ept = vcpu->vm->ept;
        ept->is_enabled = 1;
#endif  // !CONFIG_HAX_EPT2
        vcpu->mmu->mmu_mode = MMU_MODE_EPT;
        // In EPT mode, we need to monitor guest writes to CR.PAE, so that we
        // know when it wants to enter PAE paging mode (see IASDM Vol. 3A 4.1.2,
        // Figure 4-1, as well as vcpu_prepare_pae_pdpt() and its caller).
        // TODO: Monitor guest writes to CR4.{PGE, PSE, SMEP} as well (see IASDM
        // Vol. 3A 4.4.1)
        cr4_mask |= CR4_PAE;
        eptp = vm_get_eptp(vcpu->vm);
        ASSERT(eptp != INVALID_EPTP);
        // hax_debug("Guest eip:%llx, EPT mode, eptp:%llx\n", vcpu->state->_rip,
        //           eptp);
        vmwrite(vcpu, GUEST_CR3, state->_cr3);
        scpu_ctls |= ENABLE_EPT;
        // Set PDPTEs for vCPU if it's in or about to enter PAE paging mode
        if ((state->_cr4 & CR4_PAE) && !(state->_efer & IA32_EFER_LME) &&
            (state->_cr0 & CR0_PG)) {
            // vcpu_prepare_pae_pdpt() has populated vcpu->pae_pdptes
            // TODO: Enable CR3_LOAD_EXITING so as to update vcpu->pae_pdptes
            // whenever guest writes to CR3 in EPT+PAE mode
            vmwrite(vcpu, GUEST_PDPTE0, vcpu->pae_pdptes[0]);
            vmwrite(vcpu, GUEST_PDPTE1, vcpu->pae_pdptes[1]);
            vmwrite(vcpu, GUEST_PDPTE2, vcpu->pae_pdptes[2]);
            vmwrite(vcpu, GUEST_PDPTE3, vcpu->pae_pdptes[3]);
        }
        vmwrite(vcpu, VMX_EPTP, eptp);
        // pcpu_ctls |= RDTSC_EXITING;
    }

    vmwrite(vcpu, GUEST_CR0, cr0);
    vmwrite(vcpu, VMX_CR0_MASK, cr0_mask);
    hax_debug("vmwrite_cr cr0 %llx, cr0_mask %llx\n", cr0, cr0_mask);
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
        vmx(vcpu, entry_ctls) |= ENTRY_CONTROL_LOAD_EFER;
    } else {
        entry_ctls &= ~ENTRY_CONTROL_LOAD_EFER;
        vmx(vcpu, entry_ctls) &= ~ENTRY_CONTROL_LOAD_EFER;
    }

    vmwrite_efer(vcpu);
    if (state->_efer & IA32_EFER_LMA) {
        entry_ctls |= ENTRY_CONTROL_LONG_MODE_GUEST;
    }
    if (pcpu_ctls != vmx(vcpu, pcpu_ctls)) {
        vmwrite(vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS,
                vmx(vcpu, pcpu_ctls) = pcpu_ctls);
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
}

static void vcpu_enter_fpu_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    struct gstate *gstate = &vcpu->gstate;
    struct fx_layout *hfx = (struct fx_layout *)hax_page_va(hstate->hfxpage);
    struct fx_layout *gfx = (struct fx_layout *)hax_page_va(gstate->gfxpage);

    if (vcpu->is_fpu_used) {
        fxsave((mword *)hfx);
        fxrstor((mword *)gfx);
    }
}

static void vcpu_exit_fpu_state(struct vcpu_t *vcpu)
{
    struct hstate *hstate = &get_cpu_data(vcpu->cpu_id)->hstate;
    struct gstate *gstate = &vcpu->gstate;
    struct fx_layout *hfx = (struct fx_layout *)hax_page_va(hstate->hfxpage);
    struct fx_layout *gfx = (struct fx_layout *)hax_page_va(gstate->gfxpage);

    if (vcpu->is_fpu_used) {
        fxsave((mword *)gfx);
        fxrstor((mword *)hfx);
    }
}

struct decode {
    paddr_t gpa;
    paddr_t value;
    uint8_t size;       // Operand/value size in bytes (1, 2, 4 or 8)
    uint8_t addr_size;  // Address size in bytes (2, 4 or 8)
    uint8_t opcode_dir;
    uint8_t reg_index;
    vaddr_t va;         // Non-I/O GVA operand (e.g. in MOVS instructions)
    paddr_t src_pa;
    paddr_t dst_pa;
    uint8_t advance;
    bool    has_rep;    // Whether the instruction is prefixed with REP
};

// ModR/M byte (see IA SDM Vol. 2A 2.1 Figure 2-1)
union modrm_byte {
    uint8_t value;
    struct {
        uint8_t rm  : 3;
        uint8_t reg : 3;
        uint8_t mod : 2;
    };
} PACKED;

// SIB byte (see IA SDM Vol. 2A 2.1 Figure 2-1)
union sib_byte {
    uint8_t value;
    struct {
        uint8_t base  : 3;
        uint8_t index : 3;
        uint8_t scale : 2;
    };
} PACKED;

// Data transfer operations
#define OPCODE_MOV_IOMEM_TO_REG     0
#define OPCODE_MOV_REG_TO_IOMEM     1
#define OPCODE_MOV_NUM_TO_IOMEM     3
#define OPCODE_STOS                 4
#define OPCODE_MOVS_MEM_TO_IOMEM    5
#define OPCODE_MOVS_IOMEM_TO_MEM    6
#define OPCODE_MOVS_IOMEM_TO_IOMEM  7
#define OPCODE_MOVZX_IOMEM_TO_REG   8

// Bitwise operations
#define OPCODE_AND_NUM_TO_IOMEM     20  // not supported yet
#define OPCODE_AND_REG_TO_IOMEM     21  // not supported yet
#define OPCODE_AND_IOMEM_TO_REG     22
#define OPCODE_OR_NUM_TO_IOMEM      23  // not supported yet
#define OPCODE_OR_REG_TO_IOMEM      24  // not supported yet
#define OPCODE_OR_IOMEM_TO_REG      25
#define OPCODE_XOR_NUM_TO_IOMEM     26  // not supported yet
#define OPCODE_XOR_REG_TO_IOMEM     27  // not supported yet
#define OPCODE_XOR_IOMEM_TO_REG     28
#define OPCODE_NOT_IOMEM            29  // not supported yet

#define PF_SEG_OVERRIDE_NONE        0
// Each of the following denotes the presence of a segment override prefix
//   http://wiki.osdev.org/X86-64_Instruction_Encoding
#define PF_SEG_OVERRIDE_CS          1  // 0x2e
#define PF_SEG_OVERRIDE_SS          2  // 0x36
#define PF_SEG_OVERRIDE_DS          3  // 0x3e
#define PF_SEG_OVERRIDE_ES          4  // 0x26
#define PF_SEG_OVERRIDE_FS          5  // 0x64
#define PF_SEG_OVERRIDE_GS          6  // 0x65

// An instruction can have up to 4 legacy prefixes:
//   http://wiki.osdev.org/X86-64_Instruction_Encoding
#define INSTR_MAX_LEGACY_PF         4
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

static bool is_mmio_address(struct vcpu_t *vcpu, paddr_t gpa)
{
    paddr_t hpa;
    if (vtlb_active(vcpu)) {
        hpa = hax_gpfn_to_hpa(vcpu->vm, gpa >> page_shift);
        // hax_gpfn_to_hpa() assumes hpa == 0 is invalid
        return !hpa;
    } else {
#ifdef CONFIG_HAX_EPT2
        hax_memslot *slot = memslot_find(&vcpu->vm->gpa_space,
                                         gpa >> PG_ORDER_4K);
        return !slot;
#else  // !CONFIG_HAX_EPT2
        return !ept_translate(vcpu, gpa, PG_ORDER_4K, &hpa);
#endif  // CONFIG_HAX_EPT2
    }
}

// Returns 0 on success, < 0 on error, > 0 if HAX_EXIT_MMIO is necessary.
static int vcpu_simple_decode(struct vcpu_t *vcpu, struct decode *dc)
{
    uint64 cs_base = vcpu->state->_cs.base;
    uint64 rip = vcpu->state->_rip;
    vaddr_t va;
    uint8 instr[INSTR_MAX_LEN] = {0};
    uint8 len = 0;
    bool has_modrm;          // Whether ModR/M byte is present
    union modrm_byte modrm;  // ModR/M byte
    bool has_sib;            // Whether SIB byte is present
    union sib_byte sib;      // SIB byte
    uint8 disp_size;         // Displacement size in bytes
    uint8 imm_size;          // Immediate size in bytes
    int num;
    int rex_w = 0;
    int rex_r = 0;
    bool is_64bit_mode;
    int default_16bit;  // Whether operand/address sizes default to 16-bit
    int override_operand_size = 0;
    int override_address_size = 0;
    int override_segment = PF_SEG_OVERRIDE_NONE;
    int use_16bit_operands;
    uint8 operand_size;
    bool has_esc = false;  // Whether opcode begins with 0f (escape opcode byte)

    if (!qemu_support_fastmmio(vcpu)) {
        hax_warning("vcpu_simple_decode: QEMU does not support fast MMIO!\n");
        return 1;
    }

    // TODO: Is this the correct way to check if we are in 64-bit mode?
    is_64bit_mode = vcpu->state->_cs.long_mode != 0;

    // Fetch the instruction at guest CS:IP = CS.Base + IP, omitting segment
    // limit and privilege checks
    va = is_64bit_mode ? rip : cs_base + rip;
#ifdef CONFIG_HAX_EPT2
    if (mmio_fetch_instruction(vcpu, va, instr, INSTR_MAX_LEN)) {
        hax_panic_vcpu(vcpu, "%s: mmio_fetch_instruction() failed: vcpu_id=%u,"
                       " gva=0x%llx (CS:IP=0x%llx:0x%llx), mmio_gpa=0x%llx\n",
                       __func__, vcpu->vcpu_id, va, cs_base, rip, dc->gpa);
        dump_vmcs(vcpu);
        return -1;
    }
#else  // !CONFIG_HAX_EPT2
    if (!vcpu_read_guest_virtual(vcpu, va, &instr, INSTR_MAX_LEN, INSTR_MAX_LEN,
                                 0)) {
        hax_panic_vcpu(vcpu, "Error reading instruction at 0x%llx for decoding"
                       " (CS:IP=0x%llx:0x%llx)\n", va, cs_base, rip);
        dump_vmcs(vcpu);
        return -1;
    }
#endif  // CONFIG_HAX_EPT2

    // See http://wiki.osdev.org/X86-64_Instruction_Encoding
    default_16bit = !is_64bit_mode && ((vcpu->state->_cr0 & CR0_PE) == 0 ||
                    vcpu->state->_cs.operand_size == 0);

    // Parse legacy prefixes
    dc->has_rep = false;
    for (num = 0; num < INSTR_MAX_LEGACY_PF; num++) {
        switch (instr[num]) {
            case 0xf0: {
                // LOCK prefix
                // Ignored (is it possible to emulate atomic operations?)
                break;
            }
            case 0xf3: {
                // REP prefix
                dc->has_rep = true;
                break;
            }
            case 0x66: {
                // Operand-size override prefix
                override_operand_size = 1;
                break;
            }
            case 0x67: {
                // Address-size override prefix
                override_address_size = 1;
                break;
            }
            case 0x2e: {
                // CS segment override prefix
                override_segment = PF_SEG_OVERRIDE_CS;
                break;
            }
            case 0x36: {
                // SS segment override prefix
                override_segment = PF_SEG_OVERRIDE_SS;
                break;
            }
            case 0x3e: {
                // DS segment override prefix
                override_segment = PF_SEG_OVERRIDE_DS;
                break;
            }
            case 0x26: {
                // ES segment override prefix
                override_segment = PF_SEG_OVERRIDE_ES;
                break;
            }
            case 0x64: {
                // FS segment override prefix
                override_segment = PF_SEG_OVERRIDE_FS;
                break;
            }
            case 0x65: {
                // GS segment override prefix
                override_segment = PF_SEG_OVERRIDE_GS;
                break;
            }
            default: {
                goto done_legacy_pf;
            }
        }
    }

done_legacy_pf:
    // (1) For 32-bit code, 0x40 ~ 0x4f are inc/dec to one of general purpose
    //     registers.  This does not apply.
    // (2) For 64-bit code, 0x40 ~ 0x4f are size/reg (REX) prefix.

    if (instr[num] >= 0x40 && instr[num] <= 0x4f) {
        rex_w = (instr[num] & 0x08) != 0;
        rex_r = (instr[num] & 0x04) != 0;
        num++;
    }

    // See http://wiki.osdev.org/X86-64_Instruction_Encoding
    use_16bit_operands = default_16bit ^ override_operand_size;
    operand_size = rex_w ? 8 : (use_16bit_operands ? 2 : 4);
    if (is_64bit_mode) {
        dc->addr_size = override_address_size ? 4 : 8;
    } else {
        int use_16bit_addresses = default_16bit ^ override_address_size;
        dc->addr_size = use_16bit_addresses ? 2 : 4;
    }

    if (instr[num] == 0x0f) {  // Escape opcode byte
        has_esc = true;
        num++;
        // TODO: Support 3-byte opcodes
    }

    // ModR/M byte is present in most instructions we deal with
    has_modrm = true;
    modrm.value = instr[num + 1];  // Valid only if has_modrm is true
    sib.value = instr[num + 2];    // Valid only if has_sib is true

    // Assuming ModR/M byte is valid, determine has_sib and disp_size (see IA
    // SDM Vol. 2A 2.1.5, Table 2-1 and Table 2-2)
    // Mod == 01b always indicates an 8-bit displacement
    disp_size = modrm.mod == 1 ? 1 : 0;
    if (dc->addr_size == 2) {  // 16-bit addressing
        has_sib = false;
        if ((modrm.mod == 0 && modrm.rm == 6) || modrm.mod == 2) {
            disp_size = 2;
        }
    } else {  // 32 or 64-bit addressing
        has_sib = modrm.mod != 3 && modrm.rm == 4;
        // The third case is documented in the notes below IA SDM Vol. 2A 2.1.5,
        // Table 2-3
        if ((modrm.mod == 0 && modrm.rm == 5) || modrm.mod == 2 ||
            (has_sib && modrm.mod == 0 && sib.base == 5)) {
            disp_size = 4;
        }
    }

    imm_size = 0;
    // Parse the real opcode
    switch (instr[num]) {
        case 0x88:    // MOV reg => reg/mem, 8-bit
        case 0x89:    // MOV reg => reg/mem, 16/32/64-bit
        case 0x8a:    // MOV reg/mem => reg, 8-bit
        case 0x8b: {  // MOV reg/mem => reg, 16/32/64-bit
            if (modrm.mod == 3) {
                hax_error("Unexpected reg-to-reg MOV instruction\n");
                goto case_unexpected_opcode;
            }

            dc->opcode_dir = instr[num] == 0x88 || instr[num] == 0x89
                             ? OPCODE_MOV_REG_TO_IOMEM
                             : OPCODE_MOV_IOMEM_TO_REG;

            dc->size = instr[num] == 0x88 || instr[num] == 0x8a
                       ? 1 : operand_size;
            dc->reg_index = modrm.reg + (rex_r ? 8 : 0);
            break;
        }
        case 0xa0:    // MOV mem => AL
        case 0xa1:    // MOV mem => *AX
        case 0xa2:    // MOV AL => mem
        case 0xa3: {  // MOV *AX => mem
            has_modrm = false;
            dc->opcode_dir = instr[num] == 0xa0 || instr[num] == 0xa1
                             ? OPCODE_MOV_IOMEM_TO_REG
                             : OPCODE_MOV_REG_TO_IOMEM;

            dc->size = instr[num] == 0xa0 || instr[num] == 0xa2
                       ? 1 : operand_size;
            // The moffset (direct memory-offset) operand is similar to an
            // immediate as far as instruction length calculation is concerned
            // (see IA SDM Vol. 2A 2.2.1.4)
            imm_size = dc->size;
            dc->reg_index = 0;
            break;
        }
        case 0xc6:    // MOV imm => reg/mem, 8-bit
        case 0xc7: {  // MOV imm => reg/mem, 16/32-bit
            int imm_offset;
            uint32 imm_value;

            if (modrm.reg != 0) {
                hax_error("Invalid MOV instruction\n");
                goto case_unexpected_opcode;
            }
            if (modrm.mod == 3) {
                hax_error("Unexpected reg-to-reg MOV instruction\n");
                goto case_unexpected_opcode;
            }

            dc->size = operand_size;
            dc->opcode_dir = OPCODE_MOV_NUM_TO_IOMEM;
            // In 64-bit mode, source operand is still a 32-bit immediate
            imm_size = instr[num] == 0xc6 ? 1 : (operand_size == 2 ? 2 : 4);
            imm_offset = num + 2;  // 1 for opcode, 1 for ModR/M
            if (has_sib) {
                imm_offset++;
            }
            imm_offset += disp_size;
            switch (imm_size) {
                case 1: {
                    imm_value = *((uint8 *)&instr[imm_offset]);
                    break;
                }
                case 2: {
                    imm_value = *((uint16 *)&instr[imm_offset]);
                    break;
                }
                case 4: {
                    imm_value = *((uint32 *)&instr[imm_offset]);
                    break;
                }
                default: {
                    // Should never happen
                    hax_error("Invalid MOV instruction\n");
                    goto case_unexpected_opcode;
                }
            }
            if (operand_size == 8) {
                // In 64-bit mode, the 32-bit immediate is sign-extended
                int64 imm_signed = (int32)imm_value;
                dc->value = (uint64)imm_signed;
            } else {
                dc->value = imm_value;
            }
            break;
        }
        case 0xaa:    // STOSB
        case 0xab: {  // STOSW, STOSD, STOSQ
            has_modrm = false;
            dc->opcode_dir = OPCODE_STOS;
            dc->reg_index = 0;  // Source operand of STOS is always AL/*AX
            dc->size = instr[num] == 0xaa ? 1 : operand_size;
            break;
        }
        case 0xa4:    // MOVSB
        case 0xa5: {  // MOVSW, MOVSD, MOVSQ
            vaddr_t src_va, dst_va;
            paddr_t src_pa, dst_pa;
            bool is_src_mmio, is_dst_mmio;

            has_modrm = false;
            if (is_64bit_mode) {
                src_va = dc->addr_size == 8
                         ? vcpu->state->_rsi : vcpu->state->_esi;
                dst_va = dc->addr_size == 8
                         ? vcpu->state->_rdi : vcpu->state->_edi;
            } else {
                // Source segment defaults to DS but may be overridden
                uint64 src_base;
                // Destination segment is always ES
                uint64 dst_base = vcpu->state->_es.base;
                switch (override_segment) {
                    case PF_SEG_OVERRIDE_CS: {
                        src_base = vcpu->state->_cs.base;
                        break;
                    }
                    case PF_SEG_OVERRIDE_SS: {
                        src_base = vcpu->state->_ss.base;
                        break;
                    }
                    case PF_SEG_OVERRIDE_ES: {
                        src_base = vcpu->state->_es.base;
                        break;
                    }
                    case PF_SEG_OVERRIDE_FS: {
                        src_base = vcpu->state->_fs.base;
                        break;
                    }
                    case PF_SEG_OVERRIDE_GS: {
                        src_base = vcpu->state->_gs.base;
                        break;
                    }
                    default: {
                        src_base = vcpu->state->_ds.base;
                        break;
                    }
                }
                src_va = dc->addr_size == 2 ? (src_base + vcpu->state->_si)
                         : (src_base + vcpu->state->_esi);
                dst_va = dc->addr_size == 2 ? (dst_base + vcpu->state->_di)
                         : (dst_base + vcpu->state->_edi);
            }
            src_pa = dst_pa = 0xffffffffffffffffULL;
            // TODO: Can vcpu_translate() fail?
            vcpu_translate(vcpu, src_va, 0, &src_pa, NULL, true);
            vcpu_translate(vcpu, dst_va, 0, &dst_pa, NULL, true);
            is_src_mmio = src_pa == dc->gpa || is_mmio_address(vcpu, src_pa);
            is_dst_mmio = dst_pa == dc->gpa || is_mmio_address(vcpu, dst_pa);
            if (is_src_mmio && is_dst_mmio) {
                dc->opcode_dir = OPCODE_MOVS_IOMEM_TO_IOMEM;
                dc->src_pa = src_pa;
                dc->dst_pa = dst_pa;
            } else if (is_dst_mmio) {
                dc->opcode_dir = OPCODE_MOVS_MEM_TO_IOMEM;
                dc->va = src_va;
            } else {
                // is_src_mmio
                dc->opcode_dir = OPCODE_MOVS_IOMEM_TO_MEM;
                dc->va = dst_va;
            }
            dc->size = instr[num] == 0xa4 ? 1 : operand_size;
            break;
        }
        case 0xb6:    // MOVZX, 8-bit to 16/32/64-bit
        case 0xb7: {  // MOVZX, 16-bit to 32/64-bit
            if (!has_esc) {
                hax_error("Invalid MOVZX instruction: missing 0x0f\n");
                goto case_unexpected_opcode;
            }
            if (modrm.mod == 3) {
                hax_error("Unexpected reg-to-reg MOVZX instruction\n");
                goto case_unexpected_opcode;
            }

            dc->opcode_dir = OPCODE_MOVZX_IOMEM_TO_REG;
            dc->size = instr[num] == 0xb6 ? 1 : 2;  // Source operand size
            // Destination operand size does not really matter; the value is
            // always zero-extended to 64 bits
            dc->reg_index = modrm.reg + (rex_r ? 8 : 0);
            break;
        }
        // TODO: Handle 0x20, 0x21, 0x80 (/4), 0x81 (/4) and 0x83 (/4)
        case 0x22:    // AND reg/mem => reg, 8-bit
        case 0x23: {  // AND reg/mem => reg, 16/32/64-bit
            if (modrm.mod == 3) {
                hax_error("Unexpected reg-to-reg AND instruction\n");
                goto case_unexpected_opcode;
            }

            dc->opcode_dir = OPCODE_AND_IOMEM_TO_REG;
            dc->size = instr[num] == 0x22 ? 1 : operand_size;
            dc->reg_index = modrm.reg + (rex_r ? 8 : 0);
            break;
        }
        // TODO: Handle 0x08, 0x09, 0x80 (/1), 0x81 (/1) and 0x83 (/1)
        case 0x0a:    // OR reg/mem => reg, 8-bit
        case 0x0b: {  // OR reg/mem => reg, 16/32/64-bit
            if (modrm.mod == 3) {
                hax_error("Unexpected reg-to-reg OR instruction\n");
                goto case_unexpected_opcode;
            }

            dc->opcode_dir = OPCODE_OR_IOMEM_TO_REG;
            dc->size = instr[num] == 0x0a ? 1 : operand_size;
            dc->reg_index = modrm.reg + (rex_r ? 8 : 0);
            break;
        }
        // TODO: Handle 0x30, 0x31, 0x80 (/6), 0x81 (/6) and 0x83 (/6)
        case 0x32:    // XOR reg/mem => reg, 8-bit
        case 0x33: {  // XOR reg/mem => reg, 16/32/64-bit
            if (modrm.mod == 3) {
                hax_error("Unexpected reg-to-reg XOR instruction\n");
                goto case_unexpected_opcode;
            }

            dc->opcode_dir = OPCODE_XOR_IOMEM_TO_REG;
            dc->size = instr[num] == 0x32 ? 1 : operand_size;
            dc->reg_index = modrm.reg + (rex_r ? 8 : 0);
            break;
        }
case_unexpected_opcode:
        default: {
            hax_panic_vcpu(vcpu, "Unexpected MMIO instruction (opcode=0x%x,"
                           " exit_instr_length=%u, num=%d, gpa=0x%llx,"
                           " instr[0..5]=0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)\n",
                           instr[num], vcpu->vmx.exit_instr_length, num,
                           dc->gpa, instr[0], instr[1], instr[2], instr[3],
                           instr[4], instr[5]);
            dump_vmcs(vcpu);
            return -1;
        }
    }

    // Calculate instruction length
    len = (uint8)(num + 1);  // 1 for opcode
    if (has_modrm) {
        len++;
        if (has_sib) {
            len++;
        }
        len += disp_size;
    }
    len += imm_size;

    if (len != vcpu->vmx.exit_instr_length) {
        hax_debug("Inferred instruction length %u does not match VM-exit"
                  " instruction length %u (CS:IP=0x%llx:0x%llx, instr[0..5]="
                  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)\n", len,
                  vcpu->vmx.exit_instr_length, cs_base, rip, instr[0], instr[1],
                  instr[2], instr[3], instr[4], instr[5]);
    }
    dc->advance = len;
    return 0;
}

static int hax_setup_fastmmio(struct vcpu_t *vcpu, struct hax_tunnel *htun,
                              struct decode *dec)
{
    struct hax_fastmmio *hft = (struct hax_fastmmio *)vcpu->io_buf;
    struct vcpu_state_t *state = vcpu->state;
    bool advance = true;
    uint8_t buf[8] = { 0 };

    htun->_exit_status = HAX_EXIT_FAST_MMIO;
    hft->gpa = dec->gpa;
    hft->size = dec->size;

    hft->reg_index = 0;
    hft->value = 0;
    vcpu->post_mmio.op = VCPU_POST_MMIO_NOOP;

    switch (dec->opcode_dir) {
        case OPCODE_MOV_REG_TO_IOMEM:
        case OPCODE_STOS: {
            hft->value = vcpu->state->_regs[dec->reg_index];
            hft->direction = 1;
            break;
        }
        case OPCODE_MOV_IOMEM_TO_REG:
        case OPCODE_MOVZX_IOMEM_TO_REG:
        case OPCODE_AND_IOMEM_TO_REG:
        case OPCODE_OR_IOMEM_TO_REG:
        case OPCODE_XOR_IOMEM_TO_REG: {
            vcpu->post_mmio.op = VCPU_POST_MMIO_WRITE_REG;
            vcpu->post_mmio.reg_index = dec->reg_index;
            hft->direction = 0;
            break;
        }
        case OPCODE_MOV_NUM_TO_IOMEM: {
            hft->value = dec->value;
            hft->direction = 1;
            break;
        }
        case OPCODE_MOVS_MEM_TO_IOMEM: {
            // Source operand (saved in dec->va) is a non-I/O GVA
            if (!vcpu_read_guest_virtual(vcpu, dec->va, buf, 8, dec->size, 0)) {
                hax_panic_vcpu(vcpu, "Error reading %u bytes from guest RAM"
                               " (va=0x%llx, DS:RSI=0x%llx:0x%llx)\n",
                               dec->size, dec->va, vcpu->state->_ds.base,
                               vcpu->state->_rsi);
                dump_vmcs(vcpu);
                return -1;
            }
            hft->value = *((uint64 *)buf);  // Assume little-endian
            hft->direction = 1;
            break;
        }
        case OPCODE_MOVS_IOMEM_TO_MEM: {
            // Destination operand (saved in dec->va) is a non-I/O GVA
            vcpu->post_mmio.op = VCPU_POST_MMIO_WRITE_MEM;
            vcpu->post_mmio.va = dec->va;
            hft->direction = 0;
            break;
        }
        case OPCODE_MOVS_IOMEM_TO_IOMEM: {
            if (!qemu_support_fastmmio_extra(vcpu)) {
                hax_panic_vcpu(vcpu, "MOVS between two MMIO addresses requires"
                               " a newer version of QEMU HAXM module.\n");
                dump_vmcs(vcpu);
                return -1;
            }
            if (dec->src_pa == dec->gpa) {
                hft->direction = 2;
                hft->gpa2 = dec->dst_pa;
            } else {
                hft->direction = 3;
                hft->gpa2 = dec->src_pa;
                hax_warning("MOVS MMIO=>MMIO in reverse direction!\n");
            }
            break;
        }
        default: {
            hax_panic_vcpu(vcpu, "Unsupported MMIO operation %d\n",
                           dec->opcode_dir);
            dump_vmcs(vcpu);
            return -1;
        }
    }

    // Set up additional post-MMIO fields for value manipulation
    switch (dec->opcode_dir) {
        case OPCODE_AND_IOMEM_TO_REG: {
            vcpu->post_mmio.manip = VCPU_POST_MMIO_MANIP_AND;
            read_low_bits(&vcpu->post_mmio.value,
                          vcpu->state->_regs[dec->reg_index], dec->size);
            break;
        }
        case OPCODE_OR_IOMEM_TO_REG: {
            vcpu->post_mmio.manip = VCPU_POST_MMIO_MANIP_OR;
            read_low_bits(&vcpu->post_mmio.value,
                          vcpu->state->_regs[dec->reg_index], dec->size);
            break;
        }
        case OPCODE_XOR_IOMEM_TO_REG: {
            vcpu->post_mmio.manip = VCPU_POST_MMIO_MANIP_XOR;
            read_low_bits(&vcpu->post_mmio.value,
                          vcpu->state->_regs[dec->reg_index], dec->size);
            break;
        }
        default: {
            vcpu->post_mmio.manip = VCPU_POST_MMIO_MANIP_NONE;
            vcpu->post_mmio.value = 0;
            break;
        }
    }

    state = vcpu->state;
    hft->_cr0 = state->_cr0;
    hft->_cr2 = state->_cr2;
    hft->_cr3 = state->_cr3;
    hft->_cr4 = state->_cr4;

    if (dec->opcode_dir == OPCODE_STOS ||
        (dec->opcode_dir >= OPCODE_MOVS_MEM_TO_IOMEM &&
        dec->opcode_dir <= OPCODE_MOVS_IOMEM_TO_IOMEM)) {
        // STOS and MOVS require incrementing (DF=0) or decrementing (DF=1) *DI
        // MOVS also requires incrementing (DF=0) or decrementing (DF=1) *SI
        int df = (int)(state->_rflags & 0x0400);
        if (df) {
            state->_rdi -= dec->size;
            if (dec->opcode_dir >= OPCODE_MOVS_MEM_TO_IOMEM &&
                dec->opcode_dir <= OPCODE_MOVS_IOMEM_TO_IOMEM) {
                state->_rsi -= dec->size;
            }
        } else {
            state->_rdi += dec->size;
            if (dec->opcode_dir >= OPCODE_MOVS_MEM_TO_IOMEM &&
                dec->opcode_dir <= OPCODE_MOVS_IOMEM_TO_IOMEM) {
                state->_rsi += dec->size;
            }
        }
    }

    if (dec->has_rep) {
        // REP means the instruction is to be repeated *CX times
        // The exact *CX (CX/ECX/RCX) is determined by the address size
        advance = false;
        switch (dec->addr_size) {
            case 2: {
                if (--state->_cx == 0) {
                    advance = true;
                }
                break;
            }
            case 4: {
                if (--state->_ecx == 0) {
                    advance = true;
                }
                break;
            }
            case 8: {
                if (--state->_rcx == 0) {
                    advance = true;
                }
                break;
            }
            default: {
                hax_panic_vcpu(vcpu, "Invalid address size %u\n",
                               dec->addr_size);
                dump_vmcs(vcpu);
                return -1;
            }
        }
    }
    advance_rip_step(vcpu, advance ? dec->advance : 0);
    return 0;
}

static int exit_exc_nmi(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    interruption_info_t exit_intr_info;
    uint64 cr0;

    exit_intr_info.raw = vmx(vcpu, exit_intr_info).raw;
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    hax_debug("exception vmexit vector:%x\n", exit_intr_info.vector);

    switch (exit_intr_info.vector) {
        case EXC_NMI: {
            __nmi();
            return HAX_RESUME;
        }
        case EXC_PAGEFAULT: {
            if (vtlb_active(vcpu)) {
                if (handle_vtlb(vcpu))
                    return HAX_RESUME;

                paddr_t pa;
                struct decode dec;
                int ret;
                vaddr_t cr2 = vmx(vcpu, exit_qualification).address;

                ret = vcpu_simple_decode(vcpu, &dec);
                if (ret < 0) {
                    // vcpu_simple_decode() has called hax_panic_vcpu()
                    return HAX_RESUME;
                } else if (ret > 0) {
                    handle_mem_fault(vcpu, htun);
                } else {
                    vcpu_translate(vcpu, cr2, 0, &pa, (uint64_t *)NULL, 0);
                    dec.gpa = pa & 0xffffffff;
                    if (hax_setup_fastmmio(vcpu, htun, &dec)) {
                        // hax_setup_fastmmio() has called hax_panic_vcpu()
                        return HAX_RESUME;
                    }
                }
                return HAX_EXIT;
            } else {
                hax_panic_vcpu(vcpu, "Page fault shouldn't happen when EPT is "
                               "enabled.\n");
                dump_vmcs(vcpu);
            }
            break;
        }
        case EXC_NOMATH: {
            cr0 = vcpu_read_cr(state, 0);
            if (cr0 & CR0_TS) {
                uint32 exc_bitmap = vmx(vcpu, exc_bitmap);
                if (!vcpu->is_fpu_used) {
                    vcpu->is_fpu_used = 1;
                }
                exc_bitmap &= ~(1u << EXC_NOMATH);
                vmwrite(vcpu, VMX_EXCEPTION_BITMAP,
                        vmx(vcpu, exc_bitmap) = exc_bitmap);
            }
            return HAX_RESUME;
        }
        case EXC_MACHINE_CHECK: {
            hax_panic_vcpu(vcpu, "Machine check happens!\n");
            dump_vmcs(vcpu);
            handle_machine_check(vcpu);
            break;
        }
        case EXC_DOUBLEFAULT: {
            hax_panic_vcpu(vcpu, "Double fault!\n");
            dump_vmcs(vcpu);
            break;
        }
    }

    if (exit_intr_info.vector == EXC_PAGEFAULT) {
        state->_cr2 = vmx(vcpu, exit_qualification.address);
    }

    return HAX_RESUME;
}

static void handle_machine_check(struct vcpu_t *vcpu)
{
    // Dump machine check MSRs
    uint64 mcg_cap = ia32_rdmsr(IA32_MCG_CAP);
    uint n, i;

#define MSR_TRACE(msr) \
        hax_debug("MSR %s (%x): %08llx\n", #msr, msr, ia32_rdmsr(msr))

    MSR_TRACE(IA32_MCG_CAP);
    MSR_TRACE(IA32_MCG_STATUS);
    if (mcg_cap & 0x100) {
        MSR_TRACE(IA32_MCG_CTL);
    }

#define MSR_TRACEi(n, a)                                               \
        hax_debug("MSR IA32_MC%d_%s (%x): %08llx\n", i, #n, a + i * 4, \
                  ia32_rdmsr(a + i * 4))

    n = mcg_cap & 0xff;
    for (i = 0; i < n; i++) {
        MSR_TRACEi(CTL, IA32_MC0_CTL);
        MSR_TRACEi(STATUS, IA32_MC0_STATUS);
        if (ia32_rdmsr(IA32_MC0_STATUS + i * 4) & ((uint64)1 << 58)) {
            MSR_TRACEi(ADDR, IA32_MC0_ADDR);
        }
        if (ia32_rdmsr(IA32_MC0_STATUS + i * 4) & ((uint64)1 << 59)) {
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

    hax_warning("Machine check");
}

static int exit_interrupt(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    // We don't use ACK_INTERRUPT exiting, and we call sti() before
    htun->_exit_status = HAX_EXIT_INTERRUPT;
    return HAX_EXIT;
}

static int exit_triple_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    hax_panic_vcpu(vcpu, "Triple fault\n");
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

static int exit_interrupt_window(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    vmx(vcpu, pcpu_ctls) &=
            vmx(vcpu, exit_reason).basic_reason == PENDING_INTERRUPT
            ? ~INTERRUPT_WINDOW_EXITING : ~NMI_WINDOW_EXITING;

    vmwrite(vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS, vmx(vcpu, pcpu_ctls));
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    return HAX_RESUME;
}

static int exit_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    handle_cpuid(vcpu, htun);
    advance_rip(vcpu);
    hax_debug("...........exit_cpuid\n");
    return HAX_RESUME;
}

static void handle_cpuid(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32 a = state->_eax, c = state->_ecx;

    __handle_cpuid(state);

    handle_cpuid_virtual(vcpu, a, c);

    hax_debug("CPUID %08x %08x: %08x %08x %08x %08x\n", a, c, state->_eax,
              state->_ebx, state->_ecx, state->_edx);
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
}

static void handle_cpuid_virtual(struct vcpu_t *vcpu, uint32 a, uint32 c)
{
#define VIRT_FAMILY     0x6
#define VIRT_MODEL      0x1F
#define VIRT_STEPPING   0x1
    struct vcpu_state_t *state = vcpu->state;
    uint32 hw_family;
    uint32 hw_model;

    static uint32 cpu_features_1 =
            // pat is disabled!
            feat_fpu        |
            feat_vme        |
            feat_de         |
            feat_tsc        |
            feat_msr        |
            feat_pae        |
            feat_mce        |
            feat_cx8        |
            feat_apic       |
            feat_sep        |
            feat_mtrr       |
            feat_pge        |
            feat_mca        |
            feat_cmov       |
            feat_clfsh      |
            feat_mmx        |
            feat_fxsr       |
            feat_sse        |
            feat_sse2       |
            feat_ss         |
            feat_pse        |
            feat_htt;

    static uint32 cpu_features_2 =
            feat_sse3       |
            feat_ssse3      |
            feat_sse41      |
            feat_sse42      |
            feat_cmpxchg16b |
            feat_movbe      |
            feat_popcnt;

    uint32 cpu_features_ext =
            feat_execute_disable |
            feat_syscall         |
            feat_em64t;

    uint8 physical_address_size;

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
            hw_family = ((state->_eax >> 16) & 0xFF0) |
                        ((state->_eax >> 8) & 0xF);
            hw_model = ((state->_eax >> 12) & 0xF0) |
                       ((state->_eax >> 4) & 0xF);
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
            state->_ecx = (cpu_features_2 & state->_ecx) | feat_hypervisor;
            state->_edx = cpu_features_1 & state->_edx;
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
        case 3:                         // Reserved
        case 4: {                       // Deterministic Cache Parameters
            // [31:26] cores per package - 1
            state->_eax = state->_ebx = state->_ecx = state->_edx = 0;
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
            const uint32 *p = (const uint32 *)vmm_vendor_id;
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
            state->_eax = state->_ebx = state->_ecx = 0;
            // Report only the features specified but turn off any features
            // this processor doesn't support.
            state->_edx = cpu_features_ext & state->_edx;
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
            physical_address_size = (uint8)state->_eax & 0xff;
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
    hax_debug("rdtsc exiting: rip: %llx\n", vcpu->state->_rip);
    return HAX_RESUME;
}

static void check_flush(struct vcpu_t *vcpu, uint32 bits)
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
    uint64 cr_ptr;
    int cr;
    struct vcpu_state_t *state = vcpu->state;
    bool is_ept_pae = false;
    preempt_flag flags;
    uint32 vmcs_err = 0;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    cr = vmx(vcpu, exit_qualification).cr.creg;
    cr_ptr = vcpu_read_cr(state, cr);

    switch (vmx(vcpu, exit_qualification).cr.type) {
        case 0: { // MOV CR <- GPR
            uint64 val = state->_regs[(vmx(vcpu, exit_qualification).cr.gpr)];

            hax_debug("cr_access W CR%d: %08llx -> %08llx\n", cr, cr_ptr, val);
            if (cr == 0) {
                uint64 cr0_pae_triggers;

                hax_info("Guest writing to CR0[%u]: 0x%llx -> 0x%llx,"
                         " _cr4=0x%llx, _efer=0x%x\n", vcpu->vcpu_id,
                         state->_cr0, val, state->_cr4, state->_efer);
                if ((val & CR0_PG) && !(val & CR0_PE)) {
                    hax_inject_exception(vcpu, EXC_GENERAL_PROTECTION, 0);
                    return HAX_RESUME;
                }
                if (!(state->_cr0 & CR0_PG) && (val & CR0_PG) &&
                    (state->_efer & IA32_EFER_LME)) {
                    if (!(state->_cr4 & CR4_PAE)) {
                        hax_inject_exception(vcpu, EXC_GENERAL_PROTECTION, 0);
                        return HAX_RESUME;
                    }
                }
                if (!hax->ug_enable_flag && (cr_ptr & CR0_PE) &&
                    !(val & CR0_PE)) {
                    htun->_exit_status = HAX_EXIT_REALMODE;
                    hax_debug("Enter NON-PE from PE\n");
                    return HAX_EXIT;
                }

                // See IASDM Vol. 3A 4.4.1
                cr0_pae_triggers = CR0_CD | CR0_NW | CR0_PG;
                if ((val & CR0_PG) && (state->_cr4 & CR4_PAE) &&
                    !(state->_efer & IA32_EFER_LME) && !vtlb_active(vcpu) &&
                    ((val ^ cr_ptr) & cr0_pae_triggers)) {
                    hax_info("%s: vCPU #%u triggers PDPT (re)load for EPT+PAE"
                             " mode (CR0 path)\n", __func__, vcpu->vcpu_id);
                    is_ept_pae = true;
                }
            }

            if (cr == 4) {
                uint64 cr4_pae_triggers;

                hax_info("Guest writing to CR4[%u]: 0x%llx -> 0x%llx,"
                         "_cr0=0x%llx, _efer=0x%x\n", vcpu->vcpu_id,
                         state->_cr4, val, state->_cr0, state->_efer);
                if ((state->_efer & IA32_EFER_LMA) && !(val & CR4_PAE)) {
                    hax_inject_exception(vcpu, EXC_GENERAL_PROTECTION, 0);
                    return HAX_RESUME;
                }

                // See IASDM Vol. 3A 4.4.1
                // TODO: CR4_SMEP is not yet defined
                cr4_pae_triggers = CR4_PAE | CR4_PGE | CR4_PSE;
                if ((val & CR4_PAE) && (state->_cr0 & CR0_PG) &&
                    !(state->_efer & IA32_EFER_LME) && !vtlb_active(vcpu) &&
                    ((val ^ cr_ptr) & cr4_pae_triggers)) {
                    hax_info("%s: vCPU #%u triggers PDPT (re)load for EPT+PAE"
                             " mode (CR4 path)\n", __func__, vcpu->vcpu_id);
                    is_ept_pae = true;
                }
            }

            if (cr == 8) {
                hax_error("Unsupported CR%d access\n", cr);
                break;
            }
            check_flush(vcpu, cr_ptr ^ val);
            vcpu_write_cr(state, cr, val);

            if (is_ept_pae) {
                // The vCPU is either about to enter PAE paging mode (see IASDM
                // Vol. 3A 4.1.2, Figure 4-1) and needs to load its PDPTE
                // registers, or already in PAE mode and needs to reload those
                // registers
                int ret = vcpu_prepare_pae_pdpt(vcpu);
                if (ret) {
                    hax_panic_vcpu(vcpu, "vCPU #%u failed to (re)load PDPT for"
                                   " EPT+PAE mode: ret=%d\n",
                                   vcpu->vcpu_id, ret);
                    dump_vmcs(vcpu);
                    return HAX_RESUME;
                }
            }

            if ((vmcs_err = load_vmcs(vcpu, &flags))) {
                hax_panic_vcpu(vcpu,
                               "load_vmcs failed while exit_cr_access %x\n",
                               vmcs_err);
                hax_panic_log(vcpu);
                return HAX_RESUME;
            }

            vmwrite_cr(vcpu);

            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                hax_panic_vcpu(vcpu,
                               "put_vmcs failed while exit_cr_access %x\n",
                               vmcs_err);
                hax_panic_log(vcpu);
            }

            break;
        }
        case 1: { // MOV CR -> GPR
            hax_info("cr_access R CR%d\n", cr);

            state->_regs[vmx(vcpu, exit_qualification).cr.gpr] = cr_ptr;
            if (cr == 8) {
                hax_info("Unsupported CR%d access\n", cr);
                break;
            }
            break;
        }
        case 2: { // CLTS
            hax_info("CLTS\n");
            state->_cr0 &= ~(uint64)CR0_TS;
            if (!vcpu->is_fpu_used) {
                vcpu->is_fpu_used = 1;
            }
            if ((vmcs_err = load_vmcs(vcpu, &flags))) {
                hax_panic_vcpu(vcpu, "load_vmcs failed while CLTS: %x\n",
                               vmcs_err);
                hax_panic_log(vcpu);
                return HAX_RESUME;
            }
            vmwrite_cr(vcpu);
            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                hax_panic_vcpu(vcpu, "put_vmcs failed while CLTS: %x\n",
                               vmcs_err);
                hax_panic_log(vcpu);
            }
            break;
        }
        case 3: { // LMSW
            hax_info("LMSW\n");
            state->_cr0 = (state->_cr0 & ~0xfULL) |
                          (vmx(vcpu, exit_qualification).cr.lmsw_source & 0xf);
            if ((vmcs_err = load_vmcs(vcpu, &flags))) {
                hax_panic_vcpu(vcpu, "load_vmcs failed while LMSW %x\n",
                               vmcs_err);
                hax_panic_log(vcpu);
                return HAX_RESUME;
            }
            vmwrite_cr(vcpu);
            if ((vmcs_err = put_vmcs(vcpu, &flags))) {
                hax_panic_vcpu(vcpu, "put_vmcs failed while LMSW %x\n",
                               vmcs_err);
                hax_panic_log(vcpu);
            }
            break;
        }
        default: {
            hax_error("Unsupported Control Register access type.\n");
            break;
        }
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

static int exit_dr_access(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    uint64 *dr;
    struct vcpu_state_t *state = vcpu->state;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    // Generally detect
    if (state->_dr7 & DR7_GD) {
        state->_dr7 &= ~(uint64)DR7_GD;
        state->_dr6 |= DR6_BD;
        vmwrite(vcpu, GUEST_DR7, state->_dr7);
        // Priority 4 fault
        hax_inject_exception(vcpu, EXC_DEBUG, NO_ERROR_CODE);
        return HAX_RESUME;
    }

    switch (vmx(vcpu, exit_qualification.dr.dreg)) {
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
                hax_inject_exception(vcpu, EXC_UNDEFINED_OPCODE, NO_ERROR_CODE);
                return HAX_RESUME;
            }
            // Fall through
        }
        case 6: {
            dr = &state->_dr6;
            break;
        }
        case 5: {
            if (state->_cr4 & CR4_DE) {
                hax_inject_exception(vcpu, EXC_UNDEFINED_OPCODE, NO_ERROR_CODE);
                return HAX_RESUME;
            }
            // Fall through
        }
        default: {
            dr = &state->_dr7;
            break;
        }
    }

    if (vmx(vcpu, exit_qualification.dr.direction)) {
        // MOV DR -> GPR
        state->_regs[vmx(vcpu, exit_qualification).dr.gpr] = *dr;
    } else {
        // MOV DR <- GPR
        *dr = state->_regs[vmx(vcpu, exit_qualification).dr.gpr];
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

static int handle_string_io(struct vcpu_t *vcpu, exit_qualification_t *qual,
                            struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint real_size, count, required_size;
    vaddr_t start, rindex;

    htun->io._flags = 1;

    count = qual->io.rep ? state->_rcx : 1;
    required_size = count * htun->io._size;

    if (required_size <= IOS_MAX_BUFFER) {
        real_size = count * htun->io._size;
        htun->io._count = count;
    } else {
        real_size = IOS_MAX_BUFFER;
        htun->io._count = IOS_MAX_BUFFER / htun->io._size;
    }

    rindex = qual->io.direction == HAX_IO_OUT ? state->_rsi : state->_rdi;

    if (state->_rflags & EFLAGS_DF) {
        start = rindex - real_size + htun->io._size;
        htun->io._df = 1;
    } else {
        start = rindex;
        htun->io._df = 0;
    }

    htun->io._vaddr = start;

    // For UG platform and real mode
    if (hax->ug_enable_flag && !(vcpu->state->_cr0 & CR0_PE)) {
        if (qual->io.direction == HAX_IO_OUT) {
            htun->io._vaddr += state->_ds.selector * 16;
        } else {
            htun->io._vaddr += state->_es.selector * 16;
        }
        start = htun->io._vaddr;
    }

    if (qual->io.direction == HAX_IO_OUT) {
        if (!vcpu_read_guest_virtual(vcpu, start, vcpu->io_buf, IOS_MAX_BUFFER,
                                     real_size, 0))
            return HAX_RESUME;
    } else {
        // HACK: Just ensure the buffer is mapped in the kernel.
        if (!vcpu_write_guest_virtual(vcpu, start, IOS_MAX_BUFFER, vcpu->io_buf,
                                      real_size, 0))
            return HAX_RESUME;
    }

    if (required_size <= IOS_MAX_BUFFER) {
        state->_rcx = 0;
        advance_rip(vcpu);
    } else {
        state->_rcx -= IOS_MAX_BUFFER / htun->io._size;
    }

    if (state->_rflags & EFLAGS_DF) {
        rindex -= real_size ;
    } else {
        rindex += real_size;
    }

    if (qual->io.direction == HAX_IO_OUT) {
        state->_rsi = rindex;
    } else {
        state->_rdi = rindex;
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
                *((uint8 *)vcpu->io_buf) = state->_al;
                break;
            }
            case 2: {
                *((uint16 *)vcpu->io_buf) = state->_ax;
                break;
            }
            case 4: {
                *((uint32 *)vcpu->io_buf) = state->_eax;
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

    hax_debug("exit_io_access port %x, size %d\n", htun->io._port,
              htun->io._size);

    if (qual->io.string)
        return handle_string_io(vcpu, qual, htun);

    return handle_io(vcpu, qual, htun);
}

static int exit_msr_read(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32 msr = state->_ecx;
    uint64 val;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    if (!handle_msr_read(vcpu, msr, &val)) {
        state->_rax = val & 0xffffffff;
        state->_rdx = (val >> 32) & 0xffffffff;
    } else {
        hax_inject_exception(vcpu, EXC_GENERAL_PROTECTION, 0);
        return HAX_RESUME;
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

static int exit_msr_write(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32 msr = state->_ecx;
    uint64 val = (uint64)(state->_edx) << 32 | state->_eax;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    if (handle_msr_write(vcpu, msr, val)) {
        hax_inject_exception(vcpu, EXC_GENERAL_PROTECTION, 0);
        return HAX_RESUME;
    }

    advance_rip(vcpu);
    return HAX_RESUME;
}

/*
 * Returns 0 if handled, else returns 1
 * According to the caller, return 1 will cause GP to guest
 */
static int misc_msr_read(struct vcpu_t *vcpu, uint32 msr, uint64 *val)
{
    mtrr_var_t *v;

    if (msr >= IA32_MTRR_PHYSBASE0 && msr <= IA32_MTRR_PHYSMASK9) {
        assert((msr >> 1 & 0x7f) < NUM_VARIABLE_MTRRS);
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

static int handle_msr_read(struct vcpu_t *vcpu, uint32 msr, uint64 *val)
{
    int index, r = 0;
    struct vcpu_state_t *state = vcpu->state;
    struct gstate *gstate = &vcpu->gstate;

    switch (msr) {
        case IA32_TSC: {
            *val = vcpu->tsc_offset + rdtsc();
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
            if (!(state->_cr4 & CR4_PAE) && (state->_cr0 & CR0_PG)) {
                r = 1;
            } else {
                *val = state->_efer;
            }
            break;
        }
        case IA32_STAR:
        case IA32_LSTAR:
        case IA32_CSTAR:
        case IA32_SF_MASK:
        case IA32_KERNEL_GS_BASE: {
            for (index = 0; index < NR_GMSR; index++) {
                if ((uint32)gstate->gmsr[index].entry == msr) {
                    *val = gstate->gmsr[index].value;
                    break;
                }
                *val = 0;
            }
            break;
        }
        case IA32_FS_BASE: {
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
            *val = vcpu->cpuid_features_flag_mask;
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
            hax_info("handle_msr_read: IA32_PERF_CAPABILITIES\n");
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
            hax_info("handle_msr_read: IA32_PMC%u value=0x%llx\n",
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
            hax_info("handle_msr_read: IA32_PERFEVTSEL%u value=0x%llx\n",
                     msr - IA32_PERFEVTSEL0, *val);
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

    if ((state->_cr0 & CR0_PG) && (state->_efer & IA32_EFER_LME)) {
        state->_efer |= IA32_EFER_LMA;

        vmwrite(vcpu, VMX_ENTRY_CONTROLS, vmread(vcpu, VMX_ENTRY_CONTROLS) |
                ENTRY_CONTROL_LONG_MODE_GUEST);
    } else {
        state->_efer &= ~IA32_EFER_LMA;
    }

    if (vmx(vcpu, entry_ctls) & ENTRY_CONTROL_LOAD_EFER) {
        uint32 guest_efer = state->_efer;

        if (vtlb_active(vcpu)) {
            guest_efer |= IA32_EFER_XD;
        }

        vmwrite(vcpu, GUEST_EFER, guest_efer);
    }
}

static int misc_msr_write(struct vcpu_t *vcpu, uint32 msr, uint64 val)
{
    mtrr_var_t *v;

    if (msr >= IA32_MTRR_PHYSBASE0 && msr <= IA32_MTRR_PHYSMASK9) {
        assert((msr >> 1 & 0x7f) < NUM_VARIABLE_MTRRS);
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

static int handle_msr_write(struct vcpu_t *vcpu, uint32 msr, uint64 val)
{
    int index, r = 0;
    struct vcpu_state_t *state = vcpu->state;
    struct gstate *gstate = &vcpu->gstate;

    switch (msr) {
        case IA32_TSC: {
            vcpu->tsc_offset = val - rdtsc();
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
            vcpu->cpuid_features_flag_mask = val;
            break;
        }
        case IA32_EFER: {
            hax_info("Guest writing to EFER[%u]: 0x%x -> 0x%llx, _cr0=0x%llx,"
                     " _cr4=0x%llx\n", vcpu->vcpu_id, state->_efer, val,
                     state->_cr0, state->_cr4);
            if ((state->_cr0 & CR0_PG) && !(state->_cr4 & CR4_PAE)) {
                state->_efer = 0;
            } else {
                state->_efer = val;
            }
            if (!(ia32_rdmsr(IA32_EFER) & IA32_EFER_LMA) &&
                (state->_efer & IA32_EFER_LME)) {
                hax_panic_vcpu(
                        vcpu, "64-bit guest is not allowed on 32-bit host.\n");
            } else if ((state->_efer & IA32_EFER_LME) && vtlb_active(vcpu)) {
                hax_panic_vcpu(vcpu, "64-bit guest is not allowed on core 2 "
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
                if ((uint32)gmsr_list[index] == msr) {
                    gstate->gmsr[index].value = val;
                    gstate->gmsr[index].entry = msr;
                    break;
                }
            }
            break;
        }
        case IA32_FS_BASE: {
            vmwrite(vcpu, GUEST_FS_BASE, val);
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
            vcpu->cr_pat = val;
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
                hax_info("handle_msr_write: IA32_PMC%u value=0x%llx\n",
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
                hax_info("handle_msr_write: IA32_PERFEVTSEL%u value=0x%llx\n",
                         msr - IA32_PERFEVTSEL0, val);
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
    hax_panic_vcpu(vcpu, "vcpu->tr:%x\n", vcpu->state->_tr.ar);
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

static int exit_ept_misconfiguration(struct vcpu_t *vcpu,
                                     struct hax_tunnel *htun)
{
    paddr_t gpa;
#ifdef CONFIG_HAX_EPT2
    int ret;
#endif  // CONFIG_HAX_EPT2

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
#ifdef CONFIG_HAX_EPT2
    gpa = vmread(vcpu, VM_EXIT_INFO_GUEST_PHYSICAL_ADDRESS);
    ret = ept_handle_misconfiguration(&vcpu->vm->gpa_space, &vcpu->vm->ept_tree,
                                      gpa);
    if (ret > 0) {
        // The misconfigured entries have been fixed
        return HAX_RESUME;
    }
#endif  // CONFIG_HAX_EPT2

    hax_panic_vcpu(vcpu, "%s: Unexpected EPT misconfiguration: gpa=0x%llx\n",
                   __func__, gpa);
    dump_vmcs(vcpu);
    return HAX_RESUME;
}

static int exit_ept_violation(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    exit_qualification_t *qual = &vmx(vcpu, exit_qualification);
    paddr_t gpa;
    struct decode dec;
    int ret = 0;
    uint64 fault_gfn;

    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;

    if (qual->ept.gla1 == 0 && qual->ept.gla2 == 1) {
        hax_panic_vcpu(vcpu, "Incorrect EPT seting\n");
        dump_vmcs(vcpu);
        return HAX_RESUME;
    }

    gpa = vmread(vcpu, VM_EXIT_INFO_GUEST_PHYSICAL_ADDRESS);
    dec.gpa = gpa;

#ifdef CONFIG_HAX_EPT2
    ret = ept_handle_access_violation(&vcpu->vm->gpa_space, &vcpu->vm->ept_tree,
                                      *qual, gpa, &fault_gfn);
    if (ret == -EPERM) {
        htun->gpaprot.access = (qual->raw >> 3) & 7;
        htun->gpaprot.gpa = fault_gfn << PG_ORDER_4K;
        htun->_exit_status = HAX_EXIT_GPAPROT;
        return HAX_EXIT;
    }
    if (ret == -EACCES) {
        /*
         * For some reason, during boot-up, Chrome OS guests make hundreds of
         * attempts to write to GPAs close to 4GB, which are mapped into BIOS
         * (read-only) and thus result in EPT violations.
         * TODO: Handle this case properly.
         */
        hax_warning("%s: Treating unsupported EPT violation cause as MMIO.\n",
                    __func__);
        goto mmio_handler;
    }
    if (ret < 0) {
        hax_panic_vcpu(vcpu, "%s: ept_handle_access_violation() returned %d.\n",
                       __func__, ret);
        dump_vmcs(vcpu);
        return HAX_RESUME;
    }
    if (ret > 0) {
        // The EPT violation is due to a RAM/ROM access and has been handled
        return HAX_RESUME;
    }
    // ret == 0: The EPT violation is due to MMIO
mmio_handler:
#endif

    ret = vcpu_simple_decode(vcpu, &dec);
    if (ret < 0) {
        // vcpu_simple_decode() has called hax_panic_vcpu()
        return HAX_RESUME;
    } else if (ret > 0) {
        // Let the device model do the emulation
        hax_warning("exit_ept_violation: Setting exit status to "
                    "HAX_EXIT_MMIO.\n");
        htun->_exit_status = HAX_EXIT_MMIO;
        htun->mmio.gla = gpa;
    } else {
        if (hax_setup_fastmmio(vcpu, htun, &dec)) {
            // hax_setup_fastmmio() has called hax_panic_vcpu()
            return HAX_RESUME;
        }
    }
    return HAX_EXIT;
}

static void handle_mem_fault(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    hax_warning("handle_mem_fault: Setting exit status to HAX_EXIT_MMIO.\n");
    htun->_exit_status = HAX_EXIT_MMIO;
}

static int null_handler(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    htun->_exit_reason = vmx(vcpu, exit_reason).basic_reason;
    hax_panic_vcpu(vcpu, "Unhandled vmx vmexit reason:%d\n",
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
    uint32 vmcs_err = 0;

    if (state->_rsp != ustate->_rsp) {
        rsp_dirty = 1;
    }

    for (i = 0; i < 16; i++) {
        state->_regs[i] = ustate->_regs[i];
    }

    if ((vmcs_err = load_vmcs(vcpu, &flags))) {
        hax_panic_vcpu(vcpu, "load_vmcs failed on vcpu_set_regs: %x\n",
                       vmcs_err);
        hax_panic_log(vcpu);
        return -EFAULT;
    }

    if (state->_rip != ustate->_rip) {
        state->_rip = ustate->_rip;
        vmwrite(vcpu, GUEST_RIP, state->_rip);
    }
    if (state->_rflags != ustate->_rflags) {
        state->_rflags = ustate->_rflags;
        vmwrite(vcpu, GUEST_RFLAGS, state->_rflags);
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

    UPDATE_VCPU_STATE(_dr0, dr_dirty);
    UPDATE_VCPU_STATE(_dr1, dr_dirty);
    UPDATE_VCPU_STATE(_dr2, dr_dirty);
    UPDATE_VCPU_STATE(_dr3, dr_dirty);
    UPDATE_VCPU_STATE(_dr6, dr_dirty);
    UPDATE_VCPU_STATE(_dr7, dr_dirty);

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
        hax_panic_vcpu(vcpu, "put_vmcs failed on vcpu_set_regs: %x\n",
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

int vcpu_get_msr(struct vcpu_t *vcpu, uint64 entry, uint64 *val)
{
    return handle_msr_read(vcpu, entry, val);
}

int vcpu_set_msr(struct vcpu_t *vcpu, uint64 entry, uint64 val)
{
    return handle_msr_write(vcpu, entry, val);
}

static void vcpu_dump(struct vcpu_t *vcpu, uint32 mask, const char *caption)
{
    vcpu_vmread_all(vcpu);
    vcpu_state_dump(vcpu);
}

static void vcpu_state_dump(struct vcpu_t *vcpu)
{
    hax_debug(
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
            (uint32)vcpu->state->_efer);
}

int vcpu_interrupt(struct vcpu_t *vcpu, uint8 vector)
{
    hax_set_pending_intr(vcpu, vector);
    return 1;
}

// Simply to cause vmexit to vcpu, if any vcpu is running on this physical CPU
static void _vcpu_take_off(void *unused)
{
    return;
}

// Pause the vcpu, wait till vcpu is not running, or back to QEMU
int vcpu_pause(struct vcpu_t *vcpu)
{
    if (!vcpu)
        return -1;

    vcpu->paused = 1;
    smp_mb();
    if (vcpu->is_running) {
        smp_call_function(&cpu_online_map, _vcpu_take_off, NULL);
    }

    return 0;
}

int vcpu_takeoff(struct vcpu_t *vcpu)
{
    int cpu_id;
    cpumap_t targets;

    // Don't change the sequence unless you are sure
    if (vcpu->is_running) {
        cpu_id = vcpu->cpu_id;
        assert(cpu_id != hax_cpuid());
        targets = cpu2cpumap(cpu_id);
        // If not considering Windows XP, definitely we don't need this
        smp_call_function(&targets, _vcpu_take_off, NULL);
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
    vcpu->paniced = 1;
}

static int vcpu_set_apic_base(struct vcpu_t *vcpu, uint64 val)
{
    struct gstate *gstate = &vcpu->gstate;

    if (val & ~APIC_BASE_MASK) {
        hax_error("Try to set reserved bits of IA32_APIC_BASE MSR and failed "
                  "to set APIC base msr to 0x%llx.\n", val);
        return -EINVAL;
    }

    if ((val & APIC_BASE_ADDR_MASK) != APIC_BASE_DEFAULT_ADDR) {
        hax_error("APIC base cannot be relocated to 0x%llx.\n",\
                  val & APIC_BASE_ADDR_MASK);
        return -EINVAL;
    }

    if (!(val & APIC_BASE_ENABLE)) {
        hax_warning("APIC is disabled for vCPU %u.\n", vcpu->vcpu_id);
    }

    if (val & APIC_BASE_BSP) {
        if (vcpu_is_bsp(vcpu)) {
            hax_info("vCPU %u is set to bootstrap processor.\n", vcpu->vcpu_id);
        } else {
            hax_error("Bootstrap processor is vCPU %u and cannot changed to "
                      "vCPU %u.\n", vcpu->vm->bsp_vcpu_id, vcpu->vcpu_id);
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
