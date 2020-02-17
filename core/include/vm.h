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

#ifndef HAX_CORE_VM_H_
#define HAX_CORE_VM_H_

#include "vmx.h"
#include "segments.h"
#include "vcpu.h"
#include "../../include/hax.h"

#include "memory.h"
#include "ept2.h"

#define KERNEL_BASE                    0xC0000000

#define HOST_VIRTUAL_ADDR_LIMIT        0x1000000
#define HOST_VIRTUAL_ADDR_RECYCLE      0x800000
#define HVA_MAP_ARRAY_SIZE             0x800000

struct hax_p2m_entry {
    uint64_t hva;
    uint64_t hpa;
};

#define VM_SPARE_RAMSIZE       0x5800000

struct vm_t {
    hax_mutex vm_lock;
    hax_atomic_t ref_count;
#define VM_STATE_FLAGS_OPENED      0x1
#define VM_STATE_FLAGS_MEM_ALLOC   0x2
    uint64_t flags;
#define VM_FEATURES_FASTMMIO_BASIC 0x1
#define VM_FEATURES_FASTMMIO_EXTRA 0x2
    uint32_t features;
    int vm_id;
#define VPID_SEED_BITS 64
    uint8_t vpid_seed[VPID_SEED_BITS / 8];
    int fd;
    hax_list_head hvm_list;
    hax_list_head vcpu_list;
    uint16_t bsp_vcpu_id;
    void *vm_host;
    void *p2m_map[MAX_GMEM_G];
    hax_gpa_space gpa_space;
    hax_ept_tree ept_tree;
    hax_gpa_space_listener gpa_space_listener;
#ifdef HAX_ARCH_X86_32
    uint64_t hva_limit;
    uint64_t hva_index;
    uint64_t hva_index_1;
    struct hva_entry *hva_list;
    struct hva_entry *hva_list_1;
#endif
    uint64_t spare_ramsize;
    uint ram_entry_num;
    struct hax_vcpu_mem *ram_entry;
};

struct hva_entry {
    uint64_t gpfn;
    uint64_t hva;
    hax_paddr_t gcr3;
    bool is_kern;
    uint8_t level;
};

typedef struct vm_t hax_vm_t;

enum exit_status {
    HAX_EXIT_IO = 1,
    HAX_EXIT_MMIO,
    HAX_EXIT_REALMODE,
    HAX_EXIT_INTERRUPT,
    HAX_EXIT_UNKNOWN,
    HAX_EXIT_HLT,
    HAX_EXIT_STATECHANGE,
    HAX_EXIT_PAUSED,
    HAX_EXIT_FAST_MMIO,
    HAX_EXIT_PAGEFAULT,
    HAX_EXIT_DEBUG
};

enum run_flag {
    HAX_EXIT = 0,
    HAX_RESUME = 1
};

#define gpfn_to_g(gpfn) ((gpfn) >> 18)
#define gpfn_in_g(gpfn) ((gpfn) & 0x3ffff)
#define GPFN_MAP_ARRAY_SIZE (1 << 22)

uint64_t hax_gpfn_to_hpa(struct vm_t *vm, uint64_t gpfn);

struct vm_t *hax_create_vm(int *vm_id);
int hax_teardown_vm(struct vm_t *vm);

int _hax_teardown_vm(struct vm_t *vm);
void hax_teardown_vcpus(struct vm_t *vm);
int hax_destroy_host_interface(void);
int hax_vm_set_qemuversion(struct vm_t *vm, struct hax_qemu_version *ver);

uint64_t vm_get_eptp(struct vm_t *vm);

#endif // HAX_CORE_VM_H_
