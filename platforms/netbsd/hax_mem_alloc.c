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

void * hax_vmalloc(uint32_t size, uint32_t flags)
{
    vaddr_t kva;
    uvm_flag_t flag;

    if (size == 0)
        return NULL;

#if 0
    if (flags & HAX_MEM_PAGABLE)
        flag = UVM_KMF_PAGEABLE;
    else if (flags & HAX_MEM_NONPAGE)
#endif
        flag = UVM_KMF_WIRED | UVM_KMF_ZERO;

    flag |= UVM_KMF_WAITVA;

    kva = uvm_km_alloc(kernel_map, size, PAGE_SIZE, flag);

    return (void *)kva;
}

void hax_vfree_flags(void *va, uint32_t size, uint32_t flags)
{
    uvm_flag_t flag;

#if 0
    if (flags & HAX_MEM_PAGABLE)
        flag = UVM_KMF_PAGEABLE;
    else if (flags & HAX_MEM_NONPAGE)
#endif
        flag = UVM_KMF_WIRED;

    uvm_km_free(kernel_map, (vaddr_t)va, size, flag);
}

void hax_vfree(void *va, uint32_t size)
{
    uint32_t flags = HAX_MEM_NONPAGE;

    hax_vfree_flags(va, size, flags);
}

void hax_vfree_aligned(void *va, uint32_t size, uint32_t alignment,
                       uint32_t flags)
{
    hax_vfree_flags(va, size, flags);
}

void hax_vunmap(void *addr, uint32_t size)
{
    unsigned long offset;
    vaddr_t kva = (vaddr_t)addr;

    offset = kva & PAGE_MASK;
    size = round_page(size + offset);
    kva = trunc_page(kva);

    pmap_kremove(kva, size);
    pmap_update(pmap_kernel());

    uvm_km_alloc(kernel_map, kva, size, UVM_KMF_VAONLY);
}

hax_pa_t hax_pa(void *va)
{
    bool success;
    paddr_t pa;

    success = pmap_extract(pmap_kernel(), (vaddr_t)va, &pa);

    KASSERT(success);

    return pa;
}

struct hax_page * hax_alloc_pages(int order, uint32_t flags, bool vmap)
{
    struct hax_page *ppage;
    struct vm_page *page;
    paddr_t pa;
    vaddr_t kva, va;
    size_t size;
    int rv;

    ppage = kmem_zalloc(sizeof(struct hax_page), KM_SLEEP);

    // TODO: Support HAX_MEM_LOW_4G
    if (flags & HAX_MEM_LOW_4G) {
        hax_log(HAX_LOGW, "%s: HAX_MEM_LOW_4G is ignored\n", __func__);
    }

    ppage->pglist = kmem_zalloc(sizeof(struct pglist), KM_SLEEP);

    size = PAGE_SIZE << order;

    rv = uvm_pglistalloc(size, 0, ~0UL, PAGE_SIZE, 0, ppage->pglist, 1, 1);
    if (rv) {
        kmem_free(ppage->pglist, sizeof(struct pglist));
        kmem_free(ppage, sizeof(struct hax_page));
        return NULL;
    }

    kva = uvm_km_alloc(kernel_map, size, PAGE_SIZE, UVM_KMF_VAONLY);
    if (kva == 0) {
        uvm_pglistfree(ppage->pglist);
        kmem_free(ppage->pglist, sizeof(struct pglist));
        kmem_free(ppage, sizeof(struct hax_page));
        return NULL;
    }

    va = kva;
    TAILQ_FOREACH(page, ppage->pglist, pageq.queue) {
        pa = VM_PAGE_TO_PHYS(page);
        pmap_kenter_pa(va, pa, VM_PROT_READ | VM_PROT_WRITE, PMAP_WRITE_BACK);
        va += PAGE_SIZE;
    }
    pmap_update(pmap_kernel());

    ppage->page = TAILQ_FIRST(ppage->pglist);
    ppage->pa = VM_PAGE_TO_PHYS(ppage->page);
    ppage->kva = (void *)kva;
    ppage->flags = flags;
    ppage->order = order;
    return ppage;
}

void hax_free_pages(struct hax_page *pages)
{
    size_t size;

    if (!pages)
        return;

    size = PAGE_SIZE << pages->order;

    pmap_kremove((vaddr_t)pages->kva, size);
    pmap_update(pmap_kernel());
    uvm_km_free(kernel_map, (vaddr_t)pages->kva, size, UVM_KMF_VAONLY);
    uvm_pglistfree(pages->pglist);
    kmem_free(pages->pglist, sizeof(struct pglist));
    kmem_free(pages, sizeof(struct hax_page));
}

void * hax_map_page(struct hax_page *page)
{
    if (!page)
        return NULL;

    return page->kva;
}

void hax_unmap_page(struct hax_page *page)
{
    return;
}

hax_pfn_t hax_page2pfn(struct hax_page *page)
{
    if (!page)
        return 0;

    return page->pa >> PAGE_SHIFT;
}

void hax_clear_page(struct hax_page *page)
{
    memset((void *)page->kva, 0, PAGE_SIZE);
}

void hax_set_page(struct hax_page *page)
{
    memset((void *)page->kva, 0xFF, PAGE_SIZE);
}

/* Initialize memory allocation related structures */
int hax_malloc_init(void)
{
    return 0;
}

void hax_malloc_exit(void)
{
}
