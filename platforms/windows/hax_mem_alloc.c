/*
 * Copyright (c) 2011 Intel Corporation
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

#include <ntifs.h>
#include <string.h>

#include "hax_win.h"
#include "hax_entry.h"

#define HAX_MEM_TAG 'HMEM'

void * hax_vmalloc(uint32_t size, uint32_t flags)
{
    void *buf = NULL;

    // The behavior of ExAllocatePoolWithTag() is counter-intuitive when size
    // is 0. It doesn't return NULL as malloc() does.
    // Reference:
    // https://msdn.microsoft.com/en-us/library/windows/hardware/ff544520(v=vs.85).aspx
    if (size == 0)
        return NULL;

    if (flags == 0)
        flags = HAX_MEM_NONPAGE;

    if (flags & HAX_MEM_PAGABLE) {
        buf = ExAllocatePoolWithTag(PagedPool, size, HAX_MEM_TAG);
    } else if (flags & HAX_MEM_NONPAGE) {
        buf = ExAllocatePoolWithTag(NonPagedPool, size, HAX_MEM_TAG);
    } else {
        return NULL;
    }

    if (buf == NULL)
        return NULL;

    memset(buf, 0, size);

    return buf;
}

void hax_vfree_flags(void *va, uint32_t size, uint32_t flags)
{
    ExFreePoolWithTag(va, HAX_MEM_TAG);
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
    MmUnmapIoSpace(addr, size);
}

hax_pa_t hax_pa(void *va)
{
    return MmGetPhysicalAddress(va).QuadPart;
}

/* vmap is ignored in Windows, always map the pages allocaed from this API. */
struct hax_page * hax_alloc_pages(int order, uint32_t flags, bool vmap)
{
    struct hax_page *ppage = NULL;
    PMDL pmdl = NULL;
    uint64_t length = (1ULL << order) * PAGE_SIZE;
    PHYSICAL_ADDRESS high_addr, low_addr, skip_bytes;
#ifdef MDL_HAX_PAGE
    ULONG options;
#endif

    ppage = (struct hax_page *)hax_vmalloc(sizeof(struct hax_page), 0);
    if (!ppage)
        return NULL;

    if (flags & HAX_MEM_LOW_4G) {
        high_addr.LowPart = MAXULONG;
        high_addr.HighPart = 0;
    } else {
        high_addr.QuadPart = MAX_HOST_MEM_SIZE;
    }

    ppage->order = order;
    ppage->flags = flags;

#ifdef MDL_HAX_PAGE
    low_addr.QuadPart = 0;
    skip_bytes.QuadPart = 0;

    options = MM_ALLOCATE_FULLY_REQUIRED |
              MM_ALLOCATE_REQUIRE_CONTIGUOUS_CHUNKS |
              MM_DONT_ZERO_ALLOCATION;

    pmdl = MmAllocatePagesForMdlEx(low_addr, high_addr, skip_bytes, length,
                                   MmCached, options);

    if (!pmdl || MmGetMdlByteCount(pmdl) != length)
        goto error;

    ppage->kva = MmGetSystemAddressForMdlSafe(pmdl, NormalPagePriority);
    if (!ppage->kva)
        goto error;
    ppage->pmdl = pmdl;
    ppage->pa = ((uint64_t)(MmGetMdlPfnArray(pmdl)[0])) << 12;
#else
    /*
     * According to WDK, MmAllocateContiguousMemory always returns page-aligned
     * address.
     */
    ppage->kva = MmAllocateContiguousMemory(length, high_addr);
    if (!ppage->kva)
        goto error;
    memset(ppage->kva, 0, length);
    ppage->pa = MmGetPhysicalAddress(ppage->kva).QuadPart;
    ppage->pmdl = NULL;
#endif
    return ppage;

error:
    if (pmdl) {
        if (pmdl->MappedSystemVa) {
            MmUnmapLockedPages(pmdl->MappedSystemVa, pmdl);
        }
        MmFreePagesFromMdl(pmdl);
        ExFreePool(pmdl);
    } else if (ppage->kva) {
        MmFreeContiguousMemory(ppage->kva);
    }
    if (ppage)
        hax_vfree(ppage, sizeof(struct hax_page));

    return NULL;
}

void hax_free_pages(struct hax_page *pages)
{
    if (!pages)
        return;
    if (pages->pmdl) {
        if (pages->pmdl->MappedSystemVa) {
            MmUnmapLockedPages(pages->pmdl->MappedSystemVa, pages->pmdl);
        }
        MmFreePagesFromMdl(pages->pmdl);
        ExFreePool(pages->pmdl);
    } else if (pages->kva) {
        MmFreeContiguousMemory(pages->kva);
    }
    hax_vfree(pages, sizeof(struct hax_page));
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
    memset((void*)page->kva, 0, PAGE_SIZE);
}

void hax_set_page(struct hax_page *page)
{
    memset((void*)page->kva, 0xff, PAGE_SIZE);
}

/* Initialize memory allocation related structures */
int hax_malloc_init(void)
{
    return 0;
}

void hax_malloc_exit(void)
{
}
