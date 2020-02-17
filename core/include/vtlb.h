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

#ifndef HAX_CORE_VTLB_H_
#define HAX_CORE_VTLB_H_

#include "vcpu.h"

struct vcpu_t;

enum {
    TF_OK      = 0,
    TF_FAILED  = 0x80000000,    // Translation failed
    TF_GP2HP   = 0x40000000,    // GP->HP translation failed
    TF_PROTECT = 0x00000001,    // Fault due to protection
    TF_WRITE   = 0x00000002,    // Fault due to write
    TF_USER    = 0x00000004,    // Fault due to user mode
    TF_RSVD    = 0x00000008,    // Fault due to reserved bit violation
    TF_EXEC    = 0x00000010     // Fault due to exec protection
};

#define EXECUTION_DISABLE_MASK 0x8000000000000000ULL

#define PTE32_W_BIT_MASK    (1 << 1)
#define PTE32_USER_BIT_MASK (1 << 2)
#define PTE32_PWT_BIT_MASK  (1 << 3)
#define PTE32_PCD_BIT_MASK  (1 << 4)
#define PTE32_D_BIT_MASK    (1 << 6)
#define PTE32_PAT_BIT_MASK  (1 << 7)
#define PTE32_G_BIT_MASK    (1 << 8)

typedef enum mmu_mode {
    MMU_MODE_INVALID = 0,
    MMU_MODE_VTLB = 1,
    MMU_MODE_EPT = 2
} mmu_mode_t;

typedef uint32_t pagemode_t;

typedef struct vtlb {
    hax_vaddr_t va;
    hax_paddr_t ha;
    uint64_t flags;
    uint guest_order;
    uint order;
    uint access;
} vtlb_t;

#define KERNEL_ADDR_OFFSET 0xc0000000

#define igo_addr(addr) (addr >= KERNEL_ADDR_OFFSET)

typedef struct hax_mmu {
    mmu_mode_t mmu_mode;
    pagemode_t guest_mode;
    pagemode_t host_mode;
    struct hax_page *hpd_page;
    struct hax_page *pde_page;
    struct hax_page *pde_shadow_page;
    hax_paddr_t pdir;
    struct hax_link_list free_page_list;
    struct hax_link_list used_page_list;
    struct hax_link_list igo_page_list;
    bool clean;
    bool igo; /* Is global optimized */
} hax_mmu_t;

uint64_t vtlb_get_cr3(struct vcpu_t *vcpu);

void vcpu_invalidate_tlb(struct vcpu_t *vcpu, bool global);
void vcpu_invalidate_tlb_addr(struct vcpu_t *vcpu, hax_vaddr_t va);

uint vcpu_vtlb_alloc(struct vcpu_t *vcpu);
void vcpu_vtlb_free(struct vcpu_t *vcpu);

bool handle_vtlb(struct vcpu_t *vcpu);

uint vcpu_translate(struct vcpu_t *vcpu, hax_vaddr_t va, uint access, hax_paddr_t *pa,
                    uint64_t *len, bool update);

uint32_t vcpu_read_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr, void *dst,
                               uint32_t dst_buflen, uint32_t size, uint flag);
uint32_t vcpu_write_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr,
                                uint32_t dst_buflen, const void *src, uint32_t size,
                                uint flag);

/*
 * Reads the given number of bytes from guest RAM (using a GVA) into the given
 * buffer. This function is supposed to be called by the MMIO handler to obtain
 * the instruction being executed by the given vCPU, which has generated an EPT
 * violation. Its implementation should make use of the per-vCPU MMIO fetch
 * cache.
 * |vcpu|: The vCPU executing the MMIO instruction.
 * |gva|: The GVA pointing to the start of the MMIO instruction in guest RAM.
 * |buf|: The buffer to copy the bytes to.
 * |len|: The number of bytes to copy. Must not exceed the maximum length of any
 *        valid IA instruction.
 * Returns 0 on success, or one of the following error codes:
 * -ENOMEM: Memory allocation/mapping error.
 */
int mmio_fetch_instruction(struct vcpu_t *vcpu, uint64_t gva, uint8_t *buf,
                           int len);

void hax_inject_page_fault(struct vcpu_t *vcpu, mword error_code);

#endif  // HAX_CORE_VTLB_H_
