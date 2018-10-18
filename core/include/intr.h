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

#ifndef HAX_CORE_INTR_H_
#define HAX_CORE_INTR_H_

#include "../../include/hax.h"
#include "vcpu.h"

#define NO_ERROR_CODE ~0U

#define INTR_INFO_VALID_MASK  (1U << 31)
#define INTR_INFO_VECTOR_MASK (0xff)
#define INTR_INFO_TYPE_MASK   (0x700)

#define is_extern_interrupt(vec) (vec > 31)

void hax_set_pending_intr(struct vcpu_t *vcpu, uint8_t vector);
uint hax_intr_is_blocked(struct vcpu_t *vcpu);
void hax_handle_idt_vectoring(struct vcpu_t *vcpu);
void vcpu_inject_intr(struct vcpu_t *vcpu, struct hax_tunnel *htun);
void hax_inject_exception(struct vcpu_t *vcpu, uint8_t vector, uint32_t error_code);
/*
 * Get highest pending interrupt vector
 * Return HAX_INVALID_INTR_VECTOR when no pending
 */
#define HAX_INVALID_INTR_VECTOR 0x100
uint32_t vcpu_get_pending_intrs(struct vcpu_t *vcpu);
static int hax_valid_vector(uint32_t vector)
{
    return (vector <= 0xff);
}

#endif  // HAX_CORE_INTR_H_
