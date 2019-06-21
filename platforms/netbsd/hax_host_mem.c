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

#define __HAVE_DIRECT_MAP

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <uvm/uvm.h>
#include <machine/pmap.h>

#include "../../include/hax_host_mem.h"
#include "../../core/include/paging.h"

int hax_pin_user_pages(uint64_t start_uva, uint64_t size, hax_memdesc_user *memdesc)
{
    if (start_uva & PAGE_MASK) {
        hax_log(HAX_LOGE, "Failed 'start_uva & ~PAGE_MASK', start_uva=%llx\n",
                start_uva);
        return -EINVAL;
    }
    if (!size) {
        hax_log(HAX_LOGE, "Failed '!size'\n");
        return -EINVAL;
    }

    uvm_vslock(curproc->p_vmspace, (void *)start_uva, size, VM_PROT_READ | VM_PROT_WRITE);

    memdesc->uva = start_uva;
    memdesc->size = size;
    return 0;
}

int hax_unpin_user_pages(hax_memdesc_user *memdesc)
{
    vsize_t size;
    vaddr_t uva;

    if (!memdesc)
        return -EINVAL;
    if (!memdesc->size)
        return -EINVAL;
    if (!memdesc->uva)
        return -EINVAL;

    size = memdesc->size;
    uva = memdesc->uva;

    uvm_vsunlock(curproc->p_vmspace, (void *)uva, size);

    return 0;
}

uint64_t hax_get_pfn_user(hax_memdesc_user *memdesc, uint64_t uva_offset)
{
    struct vm_map *map;
    vsize_t size;
    vaddr_t uva;
    paddr_t pa;

    if (!memdesc)
        return -EINVAL;
    if (!memdesc->size)
        return -EINVAL;
    if (!memdesc->uva)
        return -EINVAL;

    size = memdesc->size;
    uva = memdesc->uva;

    if (uva_offset > size)
        return -EINVAL;

    map = &curproc->p_vmspace->vm_map;

    if (!pmap_extract(map->pmap, uva + uva_offset, &pa))
        return -EINVAL;

    return (pa >> PAGE_SHIFT);
}

void * hax_map_user_pages(hax_memdesc_user *memdesc, uint64_t uva_offset,
                          uint64_t size, hax_kmap_user *kmap)
{
    struct vm_map *map;
    struct vm_page *page;
    vaddr_t uva, va, va2, end_va;
    vaddr_t kva;
    paddr_t pa;
    int err;

    if (!memdesc)
        return NULL;
    if (!memdesc->size)
        return NULL;
    if (!memdesc->uva)
        return NULL;
    if (!kmap)
        return NULL;
    if (!size)
        return NULL;
    if (size + uva_offset > memdesc->size)
        return NULL;

    uva = trunc_page(memdesc->uva + uva_offset);
    size = round_page(size);

    map = &curproc->p_vmspace->vm_map;

    kva = uvm_km_alloc(kernel_map, size, PAGE_SIZE, UVM_KMF_VAONLY|UVM_KMF_WAITVA);

    for (va = uva, end_va = uva + size, va2 = kva; va < end_va; va += PAGE_SIZE, va2 += PAGE_SIZE) {
        if (!pmap_extract(map->pmap, va, &pa))
            break;
        pmap_kenter_pa(va2, pa, VM_PROT_READ | VM_PROT_WRITE, PMAP_WIRED);
    }
    pmap_update(pmap_kernel());

    kmap->kva = kva;
    kmap->size = size;
    return (void *)kva;
}

int hax_unmap_user_pages(hax_kmap_user *kmap)
{
    vaddr_t kva;
    vsize_t size;

    if (!kmap)
        return -EINVAL;
    if (!kmap->kva)
        return -EINVAL;
    if (!kmap->size)
        return -EINVAL;

    kva = kmap->kva;
    size = kmap->size;

    pmap_kremove(kva, size);
    pmap_update(pmap_kernel());

    uvm_km_free(kernel_map, kva, size, UVM_KMF_VAONLY);

    return 0;
}

int hax_alloc_page_frame(uint8_t flags, hax_memdesc_phys *memdesc)
{
    if (!memdesc)
        return -EINVAL;

    // TODO: Support HAX_PAGE_ALLOC_BELOW_4G
    if (flags & HAX_PAGE_ALLOC_BELOW_4G) {
        hax_log(HAX_LOGW, "%s: HAX_PAGE_ALLOC_BELOW_4G is ignored\n", __func__);
    }

    memdesc->page = uvm_pagealloc(NULL, 0, NULL, ISSET(flags, HAX_PAGE_ALLOC_ZEROED) ? UVM_PGA_ZERO : 0);

    return 0;
}

int hax_free_page_frame(hax_memdesc_phys *memdesc)
{
    if (!memdesc)
        return -EINVAL;
    if (!memdesc->page)
        return -EINVAL;

    uvm_pagefree(memdesc->page);

    memdesc->page = NULL;

    return 0;
}

uint64_t hax_get_pfn_phys(hax_memdesc_phys *memdesc)
{
    if (!memdesc)
        return INVALID_PFN;
    if (!memdesc->page)
        return INVALID_PFN;

    return VM_PAGE_TO_PHYS(memdesc->page) >> PAGE_SHIFT;
}

void * hax_get_kva_phys(hax_memdesc_phys *memdesc)
{
    if (!memdesc)
        return NULL;
    if (!memdesc->page)
        return NULL;

    return (void *)(PMAP_MAP_POOLPAGE(VM_PAGE_TO_PHYS(memdesc->page)));
}

void * hax_map_page_frame(uint64_t pfn, hax_kmap_phys *kmap)
{
    vaddr_t kva;
    paddr_t pa;
    struct vm_page *ppage;

    if (!kmap)
        return NULL;

    kva = uvm_km_alloc(kernel_map, PAGE_SIZE, PAGE_SIZE, UVM_KMF_VAONLY|UVM_KMF_WAITVA);

    pa = pfn << PAGE_SHIFT;

    pmap_kenter_pa(kva, pa, VM_PROT_READ | VM_PROT_WRITE, PMAP_WIRED);
    pmap_update(pmap_kernel());

    kmap->kva = kva;
    return (void *)kva;
}

int hax_unmap_page_frame(hax_kmap_phys *kmap)
{
    if (!kmap)
        return -EINVAL;
    if (!kmap->kva)
        return -EINVAL;

    pmap_kremove(kmap->kva, PAGE_SIZE);
    pmap_update(pmap_kernel());

    uvm_km_free(kernel_map, kmap->kva, PAGE_SIZE, UVM_KMF_VAONLY);

    return 0;
}
