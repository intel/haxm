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

#include "../../include/hax_host_mem.h"
#include "../../core/include/paging.h"

#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

int hax_pin_user_pages(uint64_t start_uva, uint64_t size, hax_memdesc_user *memdesc)
{
    int nr_pages;
    int nr_pages_pinned;
    struct page **pages;

    if (start_uva & ~PAGE_MASK)
        return -EINVAL;
    if (size & ~PAGE_MASK)
        return -EINVAL;
    if (!size)
        return -EINVAL;
    
    nr_pages = ((size - 1) / PAGE_SIZE) + 1;
    pages = kmalloc(sizeof(struct page *) * nr_pages, GFP_KERNEL);
    if (!pages)
        return -ENOMEM;

    nr_pages_pinned = get_user_pages_fast(start_uva, nr_pages, 1, pages);
    if (nr_pages_pinned < 0) {
        kfree(pages);
        return -EFAULT;
    }
    memdesc->nr_pages = nr_pages_pinned;
    memdesc->pages = pages;
    return 0;
}

int hax_unpin_user_pages(hax_memdesc_user *memdesc)
{
    if (!memdesc)
        return -EINVAL;
    if (!memdesc->pages)
        return -EINVAL;
    
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,0)
    release_pages(memdesc->pages, memdesc->nr_pages, 1);
#else
    release_pages(memdesc->pages, memdesc->nr_pages);
#endif
    return 0;
}

uint64_t hax_get_pfn_user(hax_memdesc_user *memdesc, uint64_t uva_offset)
{
    int page_idx;

    page_idx = uva_offset / PAGE_SIZE;
    if (page_idx >= memdesc->nr_pages)
        return -EINVAL;

    return page_to_pfn(memdesc->pages[page_idx]);
}

void * hax_map_user_pages(hax_memdesc_user *memdesc, uint64_t uva_offset,
                          uint64_t size, hax_kmap_user *kmap)
{
    void *kva;
    int page_idx_start;
    int page_idx_stop;
    int subrange_pages_nr;
    struct page **subrange_pages;

    if (!memdesc || !kmap || size == 0)
        return NULL;

    page_idx_start = uva_offset / PAGE_SIZE;
    page_idx_stop = (uva_offset + size - 1) / PAGE_SIZE;
    if ((page_idx_start >= memdesc->nr_pages) ||
        (page_idx_stop >= memdesc->nr_pages))
        return NULL;

    subrange_pages_nr = page_idx_stop - page_idx_start + 1;
    subrange_pages = &memdesc->pages[page_idx_start];
    kva = vmap(subrange_pages, subrange_pages_nr, VM_MAP, PAGE_KERNEL);
    kmap->kva = kva;
    return kva;
}

int hax_unmap_user_pages(hax_kmap_user *kmap)
{
    if (!kmap)
        return -EINVAL;

    vunmap(kmap->kva);
    return 0;
}

int hax_alloc_page_frame(uint8_t flags, hax_memdesc_phys *memdesc)
{
    gfp_t gfp_flags;

    if (!memdesc)
        return -EINVAL;

    gfp_flags = GFP_KERNEL;
    if (flags & HAX_PAGE_ALLOC_ZEROED)
        gfp_flags |= __GFP_ZERO;

    // TODO: Support HAX_PAGE_ALLOC_BELOW_4G
    if (flags & HAX_PAGE_ALLOC_BELOW_4G) {
        hax_log(HAX_LOGW, "%s: HAX_PAGE_ALLOC_BELOW_4G is ignored\n", __func__);
    }

    memdesc->ppage = alloc_page(gfp_flags);
    if (!memdesc->ppage)
        return -ENOMEM;
    return 0;
}

int hax_free_page_frame(hax_memdesc_phys *memdesc)
{
    if (!memdesc || !memdesc->ppage)
        return -EINVAL;

    free_page((unsigned long)page_address(memdesc->ppage));
    return 0;
}

uint64_t hax_get_pfn_phys(hax_memdesc_phys *memdesc)
{
    if (!memdesc || !memdesc->ppage)
        return INVALID_PFN;

    return page_to_pfn(memdesc->ppage);
}

void * hax_get_kva_phys(hax_memdesc_phys *memdesc)
{
    if (!memdesc || !memdesc->ppage)
        return NULL;

    return page_address(memdesc->ppage);
}

void * hax_map_page_frame(uint64_t pfn, hax_kmap_phys *kmap)
{
    void *kva;
    struct page *ppage;

    if (!kmap)
        return NULL;

    ppage = pfn_to_page(pfn);
    kva = vmap(&ppage, 1, VM_MAP, PAGE_KERNEL);
    kmap->kva = kva;
    return kva;
}

int hax_unmap_page_frame(hax_kmap_phys *kmap)
{
    if (!kmap)
        return -EINVAL;

    vfree(kmap->kva);
    return 0;
}
