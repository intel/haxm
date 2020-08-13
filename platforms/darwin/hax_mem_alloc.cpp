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

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "com_intel_hax.h"

#define HAX_ALLOC_CHECK_FAIL NULL

#define HAX_ALLOC_CHECK                                                 \
    if (flags == 0)                                                     \
        flags |= HAX_MEM_NONPAGE;                                       \
    if ((flags & (HAX_MEM_NONPAGE | HAX_MEM_PAGABLE)) ==                \
        (HAX_MEM_NONPAGE | HAX_MEM_PAGABLE)) {                          \
        hax_log(HAX_LOGW, "Confilic flags for pageable\n");       \
        return HAX_ALLOC_CHECK_FAIL;                                    \
    }                                                                   \
    if (flags & HAX_MEM_NONBLOCK) {                                     \
        hax_log(HAX_LOGE, "No nonblock allocation in mac now\n"); \
        return HAX_ALLOC_CHECK_FAIL;                                    \
    }

#define HAX_CACHE_ALIGNMENT 0x10

/* XXX init to be 0? */
extern "C" void * hax_vmalloc(uint32_t size, uint32_t flags)
{
    void *buf = NULL;
    HAX_ALLOC_CHECK

    if (size == 0)
        return NULL;

    if (flags & HAX_MEM_PAGABLE) {
        buf = IOMallocPageable(size, HAX_CACHE_ALIGNMENT);
    } else if (flags & HAX_MEM_NONPAGE) {
        buf = IOMalloc(size);
    } else {
        return NULL;
    }

    if (buf == NULL)
        return NULL;

    memset(buf, 0, size);

    return buf;
}

extern "C" void * hax_vmalloc_aligned(uint32_t size, uint32_t flags,
                                      uint32_t alignment)
{
    void *buf = NULL;
    HAX_ALLOC_CHECK

    if (flags & HAX_MEM_PAGABLE) {
        buf = IOMallocPageable(size, alignment);
    } else if (flags & HAX_MEM_NONPAGE) {
        buf = IOMallocAligned(size, alignment);
    } else {
        return NULL;
    }

    if (buf == NULL)
        return NULL;

    memset(buf, 0, size);

    return buf;
}

#undef HAX_ALLOC_CHECK_FAIL
#define HAX_ALLOC_CHECK_FAIL
extern "C" void hax_vfree_flags(void *va, uint32_t size, uint32_t flags)
{
    HAX_ALLOC_CHECK

    if (flags & HAX_MEM_PAGABLE)
        return IOFreePageable(va, size);

    if (flags & HAX_MEM_NONPAGE)
        return IOFree(va, size);
}

extern "C" void hax_vfree(void *va, uint32_t size)
{
    uint32_t flags = HAX_MEM_NONPAGE;

    hax_vfree_flags(va, size, flags);
}

extern "C" void hax_vfree_aligned(void *va, uint32_t size, uint32_t alignment,
                                  uint32_t flags)
{
    HAX_ALLOC_CHECK

    if (flags & HAX_MEM_PAGABLE) {
        IOFreePageable(va, size);
        return;
    }

    if (flags & HAX_MEM_NONPAGE)
        return IOFreeAligned(va, size);
}

struct hax_link_list _vmap_list;
hax_spinlock *vmap_lock;
struct _hax_vmap_entry {
    struct hax_link_list list;
    IOMemoryDescriptor *md;
    IOMemoryMap *mm;
    void *va;
    uint32_t size;
};

extern "C" void hax_vunmap(void *addr, uint32_t size)
{
    unsigned long va = (unsigned long)addr;
    struct _hax_vmap_entry *entry, *n;

    hax_spin_lock(vmap_lock);
    hax_list_entry_for_each_safe(entry, n, &_vmap_list, struct _hax_vmap_entry,
                                 list) {
        if ((entry->va == (void *)va) && (entry->size == size)) {
            struct IOMemoryDescriptor *md = entry->md;
            struct IOMemoryMap *mm = entry->mm;
            hax_list_del(&entry->list);
            hax_spin_unlock(vmap_lock);
            md->complete();
            mm->release();
            md->release();
            hax_vfree(entry, sizeof(struct _hax_vmap_entry));
            return;
        }
    }
    hax_spin_unlock(vmap_lock);

    printf("Failed to find the virtual address %lx\n", va);
}

extern "C" hax_pa_t hax_pa(void *va)
{
    uint64_t pa;
    struct IOMemoryDescriptor *bmd;

    /*
     * Is 0x1 as length be correct method?
     * But at least it works well on testing
     */
    bmd = IOMemoryDescriptor::withAddress(va, 0x1, kIODirectionNone);
    if (!bmd) {
        /*
         * We need to handle better here. For example, crash QEMU and exit the
         * module.
         */
        printf("NULL bmd in get_pa");
        return -1;
    }
    pa = bmd->getPhysicalSegment(0, 0, kIOMemoryMapperNone);
    bmd->release();
    return pa;
}

/*
 * vmap flag is meaningless at least in current implementation since we always
 * map it. This should be acceptable considering the 4G kernel space even on
 * 32-bit kernel.
 */
extern "C" struct hax_page * hax_alloc_pages(int order, uint32_t flags,
                                             bool vmap)
{
    struct hax_page *ppage = NULL;
    struct IOBufferMemoryDescriptor *md = NULL;
    IOOptionBits fOptions = 0;

    ppage = (struct hax_page *)hax_vmalloc(sizeof(struct hax_page), 0);
    if (!ppage)
        return NULL;

    fOptions = kIODirectionIn | kIODirectionOut | kIOMemoryKernelUserShared |
               kIOMemoryPhysicallyContiguous;

    fOptions |= kIOMemoryMapperNone;

    if (flags & HAX_MEM_LOW_4G) {
        md = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
                kernel_task, fOptions, page_size << order, (0x1ULL << 32) - 1);
    } else {
        md = IOBufferMemoryDescriptor::inTaskWithOptions(
                kernel_task, fOptions, page_size << order, page_size);
    }
    if (!md)
        goto error;

    ppage->order = order;
    ppage->bmd = md;
    ppage->flags = 0;
    ppage->kva = md->getBytesNoCopy();
    ppage->pa = md->getPhysicalSegment(0, 0, kIOMemoryMapperNone);
    return ppage;

error:
    if (ppage)
        hax_vfree(ppage, sizeof(struct hax_page));
    return NULL;
}

extern "C" void hax_free_pages(struct hax_page *pages)
{
    if (!pages)
        return;
    if (pages->flags) {
        if (pages->map)
            pages->map->release();
        if (pages->md)
            pages->md->release();
    } else {
        if (pages->bmd)
            pages->bmd->release();
    }

    hax_vfree(pages, sizeof(struct hax_page));
}

extern "C" void * hax_map_page(struct hax_page *page)
{
    return page->kva;
}

/* On Mac, it is always mapped */
extern "C" void hax_unmap_page(struct hax_page *page)
{
    return;
}

extern "C" hax_pfn_t hax_page2pfn(struct hax_page *page)
{
    if (!page)
        return 0;
    return page->pa >> page_shift;
}

extern "C" void hax_clear_page(struct hax_page *page)
{
    memset((void *)page->kva, 0, 1 << page_shift);
}

extern "C" void hax_set_page(struct hax_page *page)
{
    memset((void *)page->kva, 0xff, 1 << page_shift);
}

/* Initialize memory allocation related structures */
extern "C" int hax_malloc_init(void)
{
    /* vmap related */
    hax_init_list_head(&_vmap_list);
    vmap_lock = hax_spinlock_alloc_init();
    if (!vmap_lock) {
        hax_log(HAX_LOGE, "%s: Failed to allocate VMAP lock\n", __func__);
        return -ENOMEM;
    }

    return 0;
}

extern "C" void hax_malloc_exit(void)
{
    hax_spinlock_free(vmap_lock);
}
