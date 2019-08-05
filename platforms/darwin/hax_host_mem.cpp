/*
 * Copyright (c) 2017 Intel Corporation
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

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "../../include/hax.h"
#include "../../core/include/paging.h"

extern "C" int hax_pin_user_pages(uint64_t start_uva, uint64_t size,
                                  hax_memdesc_user *memdesc)
{
    IOOptionBits options;
    IOMemoryDescriptor *md;
    IOReturn ret;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }

    // TODO: Check whether [start_uva, start_uva + size) is a valid UVA range

    options = kIODirectionOutIn | kIOMemoryMapperNone;
    md = IOMemoryDescriptor::withAddressRange(start_uva, size, options,
                                              current_task());
    if (!md) {
        hax_log(HAX_LOGE, "%s: Failed to create memory descriptor for UVA "
                "range: start_uva=0x%llx, size=0x%llx\n", __func__, start_uva,
                size);
        return -EFAULT;
    }

    ret = md->prepare();
    if (ret != kIOReturnSuccess) {
        hax_log(HAX_LOGE, "%s: prepare() failed: ret=%d, start_uva=0x%llx, "
                "size=0x%llx\n", __func__, ret, start_uva, size);
        md->release();
        return -ENOMEM;
    }
    memdesc->md = md;
    return 0;
}

extern "C" int hax_unpin_user_pages(hax_memdesc_user *memdesc)
{
    IOReturn ret;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }
    if (!memdesc->md) {
        hax_log(HAX_LOGE, "%s: memdesc->md == NULL\n", __func__);
        return -EINVAL;
    }

    ret = memdesc->md->complete();
    if (ret != kIOReturnSuccess) {
        hax_log(HAX_LOGW, "%s: complete() failed: ret=%d\n", __func__, ret);
        // Still need to release the memory descriptor
    }
    memdesc->md->release();
    memdesc->md = NULL;
    return 0;
}

extern "C" uint64_t hax_get_pfn_user(hax_memdesc_user *memdesc, uint64_t uva_offset)
{
    addr64_t hpa;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return INVALID_PFN;
    }
    if (!memdesc->md) {
        hax_log(HAX_LOGE, "%s: memdesc->md == NULL\n", __func__);
        return INVALID_PFN;
    }

    hpa = memdesc->md->getPhysicalSegment(uva_offset, NULL);
    if (!hpa) {
        hax_log(HAX_LOGE, "%s: getPhysicalSegment() failed: "
                "uva_offset=0x%llx\n", __func__, uva_offset);
        return INVALID_PFN;
    }
    return hpa >> PG_ORDER_4K;
}

extern "C" void * hax_map_user_pages(hax_memdesc_user *memdesc,
                                     uint64_t uva_offset, uint64_t size,
                                     hax_kmap_user *kmap)
{
    IOByteCount base_size;
    uint64_t uva_offset_low, uva_offset_high;
    IOMemoryMap *mm;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return NULL;
    }
    if (!memdesc->md) {
        hax_log(HAX_LOGE, "%s: memdesc->md == NULL\n", __func__);
        return NULL;
    }
    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return NULL;
    }

    base_size = memdesc->md->getLength();
    // Align the lower bound of the UVA subrange to 4KB
    uva_offset_low = uva_offset & pgmask(PG_ORDER_4K);
    // Align the upper bound of the UVA subrange to 4KB
    uva_offset_high = (uva_offset + size + PAGE_SIZE_4K - 1) &
                      pgmask(PG_ORDER_4K);
    if (uva_offset_high > base_size) {
        hax_log(HAX_LOGE, "%s: Invalid UVA subrange: uva_offset=0x%llx, "
                "size=0x%llx, base_size=0x%llx\n", __func__, uva_offset, size,
                base_size);
        return NULL;
    }

    // Recalculate the size of the UVA subrange
    size = uva_offset_high - uva_offset_low;
    mm = memdesc->md->createMappingInTask(kernel_task, 0, kIOMapAnywhere,
                                          uva_offset_low, size);
    if (!mm) {
        hax_log(HAX_LOGE, "%s: Failed to create KVA mapping for UVA range:"
                " uva_offset_low=0x%llx, size=0x%llx\n", __func__,
                uva_offset_low, size);
        return NULL;
    }
    kmap->mm = mm;
    return (void *) mm->getVirtualAddress();
}

extern "C" int hax_unmap_user_pages(hax_kmap_user *kmap)
{
    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return -EINVAL;
    }
    if (!kmap->mm) {
        hax_log(HAX_LOGE, "%s: kmap->mm == NULL\n", __func__);
        return -EINVAL;
    }

    kmap->mm->release();
    kmap->mm = NULL;
    return 0;
}

extern "C" int hax_alloc_page_frame(uint8_t flags, hax_memdesc_phys *memdesc)
{
    IOOptionBits options;
    IOBufferMemoryDescriptor *bmd;

    // TODO: Support HAX_PAGE_ALLOC_BELOW_4G
    if (flags & HAX_PAGE_ALLOC_BELOW_4G) {
        hax_log(HAX_LOGW, "%s: HAX_PAGE_ALLOC_BELOW_4G is ignored\n", __func__);
    }
    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }

    options = kIODirectionOutIn | kIOMemoryMapperNone;
    if (flags & HAX_PAGE_ALLOC_ZEROED) {
        // Although not clear from documentation, kIOMemoryKernelUserShared (or
        // equivalently, kIOMemorySharingTypeMask) effectively causes the
        // allocated buffer to be filled with zeroes. See the implementation of
        // IOBufferMemoryDescriptor::initWithPhysicalMask().
        options |= kIOMemoryKernelUserShared;
        // Testing has shown that, without this option, the returned page frame
        // may sometimes contain non-zero bytes on macOS 10.10. Its usage is
        // strongly discouraged by official documentation, but since we are only
        // requesting one page frame, there should not be any significant
        // performance penalty.
        options |= kIOMemoryPhysicallyContiguous;
    }
    // This call may block
    bmd = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, options,
                                                      PAGE_SIZE_4K,
                                                      PAGE_SIZE_4K);
    if (!bmd) {
        hax_log(HAX_LOGE, "%s: Failed to allocate 4KB of wired memory\n",
                __func__);
        return -ENOMEM;
    }
    memdesc->bmd = bmd;
    return 0;
}

extern "C" int hax_free_page_frame(hax_memdesc_phys *memdesc)
{
    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }
    if (!memdesc->bmd) {
        hax_log(HAX_LOGE, "%s: memdesc->bmd == NULL\n", __func__);
        return -EINVAL;
    }

    memdesc->bmd->release();
    memdesc->bmd = NULL;
    return 0;
}

extern "C" uint64_t hax_get_pfn_phys(hax_memdesc_phys *memdesc)
{
    addr64_t hpa;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return INVALID_PFN;
    }
    if (!memdesc->bmd) {
        hax_log(HAX_LOGE, "%s: memdesc->bmd == NULL\n", __func__);
        return INVALID_PFN;
    }

    hpa = memdesc->bmd->getPhysicalSegment(0, NULL);
    if (!hpa) {
        hax_log(HAX_LOGE, "%s: getPhysicalSegment() failed\n", __func__);
        return INVALID_PFN;
    }
    return hpa >> PG_ORDER_4K;
}

extern "C" void * hax_get_kva_phys(hax_memdesc_phys *memdesc)
{
    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return NULL;
    }
    if (!memdesc->bmd) {
        hax_log(HAX_LOGE, "%s: memdesc->bmd == NULL\n", __func__);
        return NULL;
    }

    return memdesc->bmd->getBytesNoCopy();
}

extern "C" void * hax_map_page_frame(uint64_t pfn, hax_kmap_phys *kmap)
{
    IOMemoryDescriptor *md;
    IOMemoryMap *mm;

    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return NULL;
    }

    md = IOMemoryDescriptor::withPhysicalAddress(pfn << PG_ORDER_4K,
                                                 PAGE_SIZE_4K,
                                                 kIODirectionOutIn);
    if (!md) {
        hax_log(HAX_LOGE, "%s: Failed to create memory descriptor for "
                "pfn=0x%llx\n", __func__, pfn);
        return NULL;
    }
    mm = md->createMappingInTask(kernel_task, 0, kIOMapAnywhere);
    if (!mm) {
        hax_log(HAX_LOGE, "%s: Failed to create KVA mapping for pfn=0x%llx\n",
                __func__, pfn);
        md->release();
        return NULL;
    }
    kmap->mm = mm;
    return (void *) mm->getVirtualAddress();
}

extern "C" int hax_unmap_page_frame(hax_kmap_phys *kmap)
{
    IOMemoryDescriptor *md;

    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return -EINVAL;
    }
    if (!kmap->mm) {
        // This is a common use case, hence not treated as an error
        return 0;
    }

    md = kmap->mm->getMemoryDescriptor();
    kmap->mm->release();
    kmap->mm = NULL;
    if (!md) {
        hax_log(HAX_LOGW, "%s: getMemoryDescriptor() failed\n", __func__);
        return -EINVAL;
    }
    md->release();
    return 0;
}

