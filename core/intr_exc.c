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

#include "include/intr.h"
#include "include/vcpu.h"
#include "../include/hax.h"

/*
 * Get highest pending interrupt vector
 * return HAX_INVALID_INTR_VECTOR when no pending
 */
uint32 vcpu_get_pending_intrs(struct vcpu_t *vcpu)
{
    uint32 offset, vector;
    uint32 *intr_pending = vcpu->intr_pending;
    int i;

    if (!vcpu->nr_pending_intrs)
        return HAX_INVALID_INTR_VECTOR;

    for (i = 7; i >= 0; i--) {
        if (intr_pending[i]) {
            offset = __fls(intr_pending[i]);
            break;
        }
    }

    if (i < 0)
        return HAX_INVALID_INTR_VECTOR;

    vector = (uint8) (i * 32 + offset);
    return vector;
}

/* Set pending interrupts from userspace in the bitmap */
void hax_set_pending_intr(struct vcpu_t *vcpu, uint8 vector)
{
    uint32 *intr_pending = vcpu->intr_pending;
    uint8 offset = vector % 32;
    uint8 nr_word = vector / 32;

    if (intr_pending[nr_word] & (1 << offset)) {
        hax_debug("vector :%d is already pending.", vector);
        return;
    }
    intr_pending[nr_word] |= 1 << offset;
    ++vcpu->nr_pending_intrs;
}

/*
 * Clear the pending irqs after injection.
 */
static void vcpu_ack_intr(struct vcpu_t *vcpu, uint8 vector)
{
    uint32 *intr_pending = vcpu->intr_pending;
    uint8 offset = vector % 32;
    uint8 nr_word = vector / 32;

    ASSERT(intr_pending[nr_word] & (1 << offset));

    intr_pending[nr_word] &= ~(1 << offset);
    --vcpu->nr_pending_intrs;
}


/* Do the real injection operation for virtual external interrrupts
 * caller must ensure the vcpu is ready for accepting the interrupt
 */
static void hax_inject_intr(struct vcpu_t *vcpu, uint8 vector)
{
    uint32 intr_info;
    intr_info = (1 << 31) | vector;
    vmwrite(vcpu, VMX_ENTRY_INTERRUPT_INFO, intr_info);
    vcpu_ack_intr(vcpu, vector);
    vcpu->event_injected = 1;
}

/*
 * Enable interrupt window and give a chance to pick up
 * the pending interrupts in time
 */
static void hax_enable_intr_window(struct vcpu_t *vcpu)
{
    vmx(vcpu, pcpu_ctls) |= INTERRUPT_WINDOW_EXITING;
    vmwrite(vcpu, VMX_PRIMARY_PROCESSOR_CONTROLS, vmx(vcpu, pcpu_ctls));
}

/*
 * Check whether vcpu is ready for interrupt injection.
 * Maybe blocked by STI, MOV SS, Pending NMI etc.
 */
uint hax_intr_is_blocked(struct vcpu_t *vcpu)
{
    struct vcpu_state_t *state = vcpu->state;
    uint32 intr_status;

    if (!(state->_eflags & EFLAGS_IF))
        return 1;

    intr_status = vmread(vcpu, GUEST_INTERRUPTIBILITY);
    if (intr_status & 3)
        return 1;
    return 0;
}

/*
 * Handle IDT-vectoring for interrupt injection
 */
void hax_handle_idt_vectoring(struct vcpu_t *vcpu)
{
    uint8 vector;
    uint32 idt_vec = vmread(vcpu, VM_EXIT_INFO_IDT_VECTORING);

    if (idt_vec & 0x80000000) {
        if (!(idt_vec & 0x700)) {
            /* One ext interrupt is pending ? Re-inject it ? */
            vector = (uint8) (idt_vec & 0xff);
            hax_set_pending_intr(vcpu, vector);
            hax_debug("extern interrupt is vectoring....vector:%d\n", vector);
        } else {
            hax_debug("VM Exit @ IDT vectoring, type:%d, vector:%d,"
                      " error code:%llx\n",
                      (idt_vec & 0x700) >> 8, idt_vec & 0xff,
                      vmread(vcpu, VM_EXIT_INFO_IDT_VECTORING_ERROR_CODE));
        }
    }
}

/*
 * Checking the pending interrupts and inject one once ready
 */
void vcpu_inject_intr(struct vcpu_t *vcpu, struct hax_tunnel *htun)
{
    uint32 vector;
    uint32 intr_info;

    intr_info = vmread(vcpu, VMX_ENTRY_INTERRUPT_INFO);
    vector = vcpu_get_pending_intrs(vcpu);
    if (hax_valid_vector(vector) && !vcpu->event_injected &&
        !hax_intr_is_blocked(vcpu) && !(intr_info & (1 << 31)))
        hax_inject_intr(vcpu, vector);
    /* Check interrupt window's setting needed */
    vector = vcpu_get_pending_intrs(vcpu);
    if (hax_valid_vector(vector) || htun->request_interrupt_window)
        hax_enable_intr_window(vcpu);
}

/* According the to SDM to check whether the pending vector and injecting vector
 * can generate a double fault.
 */
static int is_double_fault(uint8 first_vec, uint8 second_vec)
{
    uint32 exc_bitmap1 = 0x7c01u;
    uint32 exc_bitmap2 = 0x3c01u;

    if (is_extern_interrupt(first_vec))
        return 0;

    if ((first_vec == VECTOR_PF && (exc_bitmap1 & (1u << second_vec))) ||
        ((exc_bitmap2 & (1u << first_vec)) && (exc_bitmap2 &
        (1u << second_vec))))
        return 1;
    return 0;
}

/*
 * Inject faults or exceptions to the virtual processor .
 */
void hax_inject_exception(struct vcpu_t *vcpu, uint8 vector, uint32 error_code)
{
    uint32 intr_info = 0;
    uint8 first_vec;
    uint32 vect_info = vmx(vcpu, exit_idt_vectoring);
    uint32 exit_instr_length = vmx(vcpu, exit_instr_length);

    if (vcpu->event_injected == 1)
        hax_debug("Event is injected already!!:\n");

    if (vect_info & INTR_INFO_VALID_MASK) {
        first_vec = (uint8) (vect_info & INTR_INFO_VECTOR_MASK);
        if (is_double_fault(first_vec, vector)) {
            intr_info = (1 << 31) | (1 << 11) | (EXCEPTION << 8)
                        | VECTOR_DF;
            error_code = 0;
        } else {
            intr_info = (1 << 31) | (EXCEPTION << 8) | vector;
        }
    } else {
        intr_info = (1 << 31) | (EXCEPTION << 8) | vector;
        if (error_code != NO_ERROR_CODE) {
            intr_info |= 1 << 11;
            if (vector == VECTOR_PF) {
                vcpu->vmcs_pending_entry_error_code = 1;
                vmx(vcpu, entry_exception_error_code) = error_code;
            } else {
                vmwrite(vcpu, VMX_ENTRY_EXCEPTION_ERROR_CODE, error_code);
            }
        }
    }

    if (vector == VECTOR_PF) {
        vcpu->vmcs_pending_entry_instr_length = 1;
        vmx(vcpu, entry_instr_length) = exit_instr_length;
        vcpu->vmcs_pending_entry_intr_info = 1;
        vmx(vcpu, entry_intr_info).raw = intr_info;
        vcpu->vmcs_pending = 1;
    } else {
        vmwrite(vcpu, VMX_ENTRY_INSTRUCTION_LENGTH, exit_instr_length);
        vmwrite(vcpu, VMX_ENTRY_INTERRUPT_INFO, intr_info);
    }

    hax_debug("Guest is injecting exception info:%x\n", intr_info);
    vcpu->event_injected = 1;
}

void hax_inject_page_fault(struct vcpu_t *vcpu, mword error_code)
{
    hax_inject_exception(vcpu, VECTOR_PF, error_code);
}
