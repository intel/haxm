/*
 * Copyright (c) 2018 Kamil Rytarowski
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <uvm/uvm.h>

#include "../../include/hax.h"

struct hax_vcpu_mem_hinfo_t {
    struct uvm_object *uao;
    int flags;
};

int hax_clear_vcpumem(struct hax_vcpu_mem *mem)
{
    struct hax_vcpu_mem_hinfo_t *hinfo;
    struct vm_map *map;
    vaddr_t uva, kva;
    vsize_t size;

    if (!mem)
        return -EINVAL;

    hinfo = mem->hinfo;

    uva = mem->uva;
    kva = (vaddr_t)mem->kva;
    size = mem->size;

    if (!ISSET(hinfo->flags, HAX_VCPUMEM_VALIDVA)) {
        map = &curproc->p_vmspace->vm_map;
        uvm_unmap(map, uva, uva + size);
    }

    uvm_unmap(kernel_map, kva, kva + size);

    if (!ISSET(hinfo->flags, HAX_VCPUMEM_VALIDVA)) {
        uao_detach(hinfo->uao);
    }

    kmem_free(hinfo, sizeof(struct hax_vcpu_mem_hinfo_t));

    return 0;
}

int hax_setup_vcpumem(struct hax_vcpu_mem *mem, uint64_t uva, uint32_t size,
                      int flags)
{
    struct proc *p;
    struct uvm_object *uao;
    struct vm_map *map;
    int err;
    struct hax_vcpu_mem_hinfo_t *hinfo = NULL;
    vaddr_t kva, kva2;
    vaddr_t va, end_va;
    paddr_t pa;
    unsigned offset;

    if (!mem || !size)
        return -EINVAL;

    offset = uva & PAGE_MASK;
    size = round_page(size + offset);
    uva = trunc_page(uva);

    hinfo = kmem_zalloc(sizeof(struct hax_vcpu_mem_hinfo_t), KM_SLEEP);
    hinfo->flags = flags;

    p = curproc;
    map = &p->p_vmspace->vm_map;

    if (!ISSET(flags, HAX_VCPUMEM_VALIDVA)) {
        // Map to user
        uao = uao_create(size, 0);
        uao_reference(uao);
        va = p->p_emul->e_vm_default_addr(p, (vaddr_t)p->p_vmspace->vm_daddr, size, map->flags & VM_MAP_TOPDOWN);
        err = uvm_map(map, &va, size, uao, 0, 0,
                      UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW, UVM_INH_NONE,
                                  UVM_ADV_RANDOM, 0));
        uao_reference(uao);
        if (err) {
            hax_log(HAX_LOGE, "Failed to map into user\n");
            uao_detach(uao);
            kmem_free(hinfo, sizeof(struct hax_vcpu_mem_hinfo_t));
            return -ENOMEM;
        }
        hinfo->uao = uao;
        uva = va;
    }

    err = uvm_map_extract(map, uva, size, kernel_map, &kva,  UVM_EXTRACT_QREF | UVM_EXTRACT_CONTIG | UVM_EXTRACT_FIXPROT);
    if (err) {
        hax_log(HAX_LOGE, "Failed to map into kernel\n");
        if (!ISSET(flags, HAX_VCPUMEM_VALIDVA)) {
            uvm_unmap(map, uva, uva + size);
            uao_detach(uao);
            kmem_free(hinfo, sizeof(struct hax_vcpu_mem_hinfo_t));
        }
    }

    mem->uva = uva;
    mem->kva = (void *)kva;
    mem->hinfo = hinfo;
    mem->size = size;
    return 0;
}

uint64_t hax_get_memory_threshold(void)
{
    // Since there is no memory cap, just return a sufficiently large value
    return 1ULL << 48;  // PHYSADDR_MAX + 1
}
