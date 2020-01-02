/*
 * Copyright (c) 2018 Kryptos Logic
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

#include "../../include/hax.h"

#include <linux/mm.h>
#include <linux/slab.h>

void * hax_vmalloc(uint32_t size, uint32_t flags)
{
    void *ptr;
    (void)flags;

    if (size == 0)
        return NULL;

    // NOTE: Flags ignored. Linux allows only non-pageable memory in kernel.
    ptr = kzalloc(size, GFP_KERNEL);
    return ptr;
}

void hax_vfree_flags(void *va, uint32_t size, uint32_t flags)
{
    (void)size;
    (void)flags;

    // NOTE: Flags ignored. Linux allows only non-pageable memory in kernel.
    kfree(va);
}

void hax_vfree(void *va, uint32_t size)
{
    hax_vfree_flags(va, size, 0);
}

void hax_vfree_aligned(void *va, uint32_t size, uint32_t alignment,
                       uint32_t flags)
{
    hax_vfree_flags(va, size, flags);
}

void hax_vunmap(void *addr, uint32_t size)
{
    return iounmap(addr);
}

hax_pa_t hax_pa(void *va)
{
    return virt_to_phys(va);
}

struct hax_page * hax_alloc_pages(int order, uint32_t flags, bool vmap)
{
    struct hax_page *ppage;
    struct page *page;
    gfp_t gfp_mask;

    ppage = kmalloc(sizeof(struct hax_page), GFP_KERNEL);
    if (!ppage)
        return NULL;

    gfp_mask = GFP_KERNEL;
    // TODO: Support HAX_MEM_LOW_4G
    if (flags & HAX_MEM_LOW_4G) {
        hax_log(HAX_LOGW, "%s: HAX_MEM_LOW_4G is ignored\n", __func__);
    }

    page = alloc_pages(GFP_KERNEL, order);
    if (!page) {
        kfree(ppage);
        return NULL;
    }

    ppage->page = page;
    ppage->pa = page_to_phys(page);
    ppage->kva = page_address(page);
    ppage->flags = flags;
    ppage->order = order;
    return ppage;
}

void hax_free_pages(struct hax_page *pages)
{
    if (!pages)
        return;

    free_pages((unsigned long)pages->kva, pages->order);
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
