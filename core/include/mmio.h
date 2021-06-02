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

#ifndef HAX_CORE_MMIO_H_
#define HAX_CORE_MMIO_H_

#include "vcpu.h"

// Reads the given number of bytes from guest RAM (using a GVA) into the given
// buffer. This function is supposed to be called by the MMIO handler to obtain
// the instruction being executed by the given vCPU, which has generated an EPT
// violation. Its implementation should make use of the per-vCPU MMIO fetch
// cache.
// |vcpu|  The vCPU executing the MMIO instruction.
// |gva|   The GVA pointing to the start of the MMIO instruction in guest RAM.
// |buf|   The buffer to copy the bytes to.
// |len|   The number of bytes to copy. Must not exceed the maximum length of
//         any valid IA instruction.
// Returns 0 on success, or one of the following error codes:
// -ENOMEM: Memory allocation/mapping error.

int mmio_fetch_instruction(struct vcpu_t *vcpu, uint64_t gva, uint8_t *buf,
                           int len);

// Translates guest virtual address to guest physical address.
// |vcpu|    Pointer to the vCPU
// |va|      Guest virtual address
// |access|  Access descriptor (read/write, user/supervisor)
// |pa|      Guest physical address
// |len|     Number of bytes for which translation is valid
// |update|  Update access and dirty bits of guest structures
// Returns 0 if translation is successful, 0x80000000 OR'ed with the exception
// number otherwise.

uint vcpu_translate(struct vcpu_t *vcpu, hax_vaddr_t va, uint access,
                    hax_paddr_t *pa, uint64_t *len, bool update);

// Reads guest-linear memory.
// If flag is 0, this read is on behalf of the guest. This function updates the
// access/dirty bits in the guest page tables and injects a page fault if there
// is an error. In this case, the return value is true for success, false if a
// page fault was injected.
// If flag is 1, this function updates the access/dirty bits in the guest page
// tables but does not inject a page fault if there is an error. Instead, it
// returns the number of bytes read.
// If flag is 2, the memory read is for internal use. It does not update the
// guest page tables. It returns the number of bytes read.

uint32_t vcpu_read_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr,
                                 void *dst, uint32_t dst_buflen, uint32_t size,
                                 uint flag);

// Writes guest-linear memory.
// If flag is 0, this memory write is on behalf of the guest. This function
// updates the access/dirty bits in the guest page tables and injects a page
// fault if there is an error. In this case, the return value is true for
// success, false if a page fault was injected.
// If flag is 1, it updates the access/dirty bits in the guest page tables but
// does not inject a page fault if there is an error. Instead, it returns the
// number of bytes written.
// A flag value of 2 is implemented, but not used. It does not update the guest
// page tables. It returns the number of bytes written.

uint32_t vcpu_write_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr,
                                  uint32_t dst_buflen, const void *src,
                                  uint32_t size, uint flag);

#endif  // HAX_CORE_MMIO_H_
