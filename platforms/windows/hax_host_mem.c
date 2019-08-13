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
#include "../../include/hax.h"
#include "../../core/include/paging.h"

int hax_pin_user_pages(uint64_t start_uva, uint64_t size, hax_memdesc_user *memdesc)
{
    PMDL pmdl = NULL;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }

    // TODO: Check whether [start_uva, start_uva + size) is a valid UVA range

    pmdl = IoAllocateMdl((PVOID)start_uva, size, FALSE, FALSE, NULL);
    if (!pmdl) {
        hax_log(HAX_LOGE, "Failed to allocate MDL for va: 0x%llx, "
                "size: 0x%llx.\n", start_uva, size);
        return -EFAULT;
    }

    try {
        MmProbeAndLockPages(pmdl, UserMode, IoWriteAccess);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        hax_log(HAX_LOGE, "Failed to probe pages for guest's memory! "
                "va: 0x%llx\n", start_uva);
        IoFreeMdl(pmdl);
        return -ENOMEM;
    }

    memdesc->pmdl = pmdl;
    return 0;
}

int hax_unpin_user_pages(hax_memdesc_user *memdesc)
{
    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }

    if (!memdesc->pmdl) {
        hax_log(HAX_LOGE, "%s: memdesc->pmdl == NULL\n", __func__);
        return -EINVAL;
    }

    MmUnlockPages(memdesc->pmdl);
    IoFreeMdl(memdesc->pmdl);
    memdesc->pmdl = NULL;

    return 0;
}

uint64_t hax_get_pfn_user(hax_memdesc_user *memdesc, uint64_t uva_offset)
{
    PMDL pmdl = NULL;
    PPFN_NUMBER ppfn = NULL;
    uint64_t len;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return INVALID_PFN;
    }

    if (!memdesc->pmdl) {
        hax_log(HAX_LOGE, "%s: memdesc->pmdl == NULL\n", __func__);
        return INVALID_PFN;
    }

    pmdl = memdesc->pmdl;

    len = MmGetMdlByteCount(pmdl);
    if (uva_offset >= len) {
        hax_log(HAX_LOGE, "The uva_offset 0x%llx exceeds the buffer "
                "length 0x%llx.\n", uva_offset, len);
        return INVALID_PFN;
    }

    ppfn = MmGetMdlPfnArray(pmdl);
    if (NULL == ppfn) {
        hax_log(HAX_LOGE, "Get MDL pfn array failed. uva_offset: 0x%llx.\n",
                uva_offset);
        return INVALID_PFN;
    }

    return (uint64_t)ppfn[uva_offset >> PG_ORDER_4K];
}

void * hax_map_user_pages(hax_memdesc_user *memdesc, uint64_t uva_offset,
                          uint64_t size, hax_kmap_user *kmap)
{
    ULONG base_size;
    uint64_t uva_offset_low, uva_offset_high;
    uint64_t base_uva, start_uva;
    PMDL pmdl;
    PVOID kva;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return NULL;
    }
    if (!memdesc->pmdl) {
        hax_log(HAX_LOGE, "%s: memdesc->pmdl == NULL\n", __func__);
        return NULL;
    }
    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return NULL;
    }

    // Size of the underlying UVA range
    base_size = MmGetMdlByteCount(memdesc->pmdl);
    // Align the lower bound of the UVA subrange to 4KB
    uva_offset_low = uva_offset & pgmask(PG_ORDER_4K);
    // Align the upper bound of the UVA subrange to 4KB
    uva_offset_high = (uva_offset + size + PAGE_SIZE_4K - 1) &
                      pgmask(PG_ORDER_4K);
    if (uva_offset_high > base_size) {
        hax_log(HAX_LOGE, "%s: Invalid UVA subrange: uva_offset=0x%llx, "
                "size=0x%llx, base_size=0x%llx\n", __func__, uva_offset,
                size, base_size);
        return NULL;
    }

    // Start of the underlying UVA range
    base_uva = (uint64_t)MmGetMdlVirtualAddress(memdesc->pmdl);
    // Start of the UVA subrange
    start_uva = base_uva + uva_offset_low;
    // Recalculate the size of the UVA subrange
    size = uva_offset_high - uva_offset_low;
    // Create a new MDL for the UVA subrange
    pmdl = IoAllocateMdl((PVOID)start_uva, size, FALSE, FALSE, NULL);
    if (!pmdl) {
        hax_log(HAX_LOGE, "%s: Failed to create MDL for UVA subrange: "
                "start_uva=0x%llx, size=0x%llx\n", __func__, start_uva, size);
        return NULL;
    }
    // Associate the new MDL with the existing MDL
    IoBuildPartialMdl(memdesc->pmdl, pmdl, (PVOID)start_uva, size);
    kva = MmGetSystemAddressForMdlSafe(pmdl, NormalPagePriority);
    if (!kva) {
        hax_log(HAX_LOGE, "%s: Failed to create KVA mapping for UVA subrange:"
                " start_uva=0x%llx, size=0x%llx\n", __func__, start_uva, size);
        IoFreeMdl(pmdl);
        return NULL;
    }
    kmap->pmdl = pmdl;
    return kva;
}

int hax_unmap_user_pages(hax_kmap_user *kmap)
{
    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return -EINVAL;
    }
    if (!kmap->pmdl) {
        hax_log(HAX_LOGE, "%s: kmap->pmdl == NULL\n", __func__);
        return -EINVAL;
    }

    // IoFreeMdl() also destroys any KVA mapping previously created by
    // MmGetSystemAddressForMdlSafe()
    IoFreeMdl(kmap->pmdl);
    return 0;
}

int hax_alloc_page_frame(uint8_t flags, hax_memdesc_phys *memdesc)
{
    PHYSICAL_ADDRESS low_addr, high_addr, skip_bytes;
    ULONG options;
    PMDL pmdl;

    // TODO: Support HAX_PAGE_ALLOC_BELOW_4G
    if (flags & HAX_PAGE_ALLOC_BELOW_4G) {
        hax_log(HAX_LOGW, "%s: HAX_PAGE_ALLOC_BELOW_4G is ignored\n", __func__);
    }
    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }

    low_addr.QuadPart = 0;
    high_addr.QuadPart = (int64_t)-1;
    skip_bytes.QuadPart = 0;
    // TODO: MM_ALLOCATE_NO_WAIT?
    options = MM_ALLOCATE_FULLY_REQUIRED;
    if (!(flags & HAX_PAGE_ALLOC_ZEROED)) {
        options |= MM_DONT_ZERO_ALLOCATION;
    }
    // This call may block
    pmdl = MmAllocatePagesForMdlEx(low_addr, high_addr, skip_bytes,
                                   PAGE_SIZE_4K, MmCached, options);
    if (!pmdl) {
        hax_log(HAX_LOGE, "%s: Failed to allocate 4KB of nonpaged memory\n", __func__);
        return -ENOMEM;
    }
    memdesc->pmdl = pmdl;
    return 0;
}

int hax_free_page_frame(hax_memdesc_phys *memdesc)
{
    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return -EINVAL;
    }
    if (!memdesc->pmdl) {
        hax_log(HAX_LOGE, "%s: memdesc->pmdl == NULL\n", __func__);
        return -EINVAL;
    }

    MmFreePagesFromMdl(memdesc->pmdl);
    ExFreePool(memdesc->pmdl);
    memdesc->pmdl = NULL;
    return 0;
}

uint64_t hax_get_pfn_phys(hax_memdesc_phys *memdesc)
{
    PPFN_NUMBER pfns;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return INVALID_PFN;
    }
    if (!memdesc->pmdl) {
        hax_log(HAX_LOGE, "%s: memdesc->pmdl == NULL\n", __func__);
        return INVALID_PFN;
    }

    pfns = MmGetMdlPfnArray(memdesc->pmdl);
    if (!pfns) {
        hax_log(HAX_LOGE, "%s: MmGetMdlPfnArray() failed\n", __func__);
        return INVALID_PFN;
    }
    return pfns[0];
}

void * hax_get_kva_phys(hax_memdesc_phys *memdesc)
{
    PVOID kva;

    if (!memdesc) {
        hax_log(HAX_LOGE, "%s: memdesc == NULL\n", __func__);
        return NULL;
    }
    if (!memdesc->pmdl) {
        hax_log(HAX_LOGE, "%s: memdesc->pmdl == NULL\n", __func__);
        return NULL;
    }

    kva = MmGetSystemAddressForMdlSafe(memdesc->pmdl, NormalPagePriority);
    if (!kva) {
        hax_log(HAX_LOGE, "%s: MmGetSystemAddressForMdlSafe() failed\n",
                __func__);
        return NULL;
    }
    return kva;
}

void * hax_map_page_frame(uint64_t pfn, hax_kmap_phys *kmap)
{
    PHYSICAL_ADDRESS addr;
    PVOID kva;

    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return NULL;
    }

    addr.QuadPart = (LONGLONG)(pfn << PG_ORDER_4K);
    kva = MmMapIoSpace(addr, PAGE_SIZE_4K, MmCached);
    kmap->kva = kva;
    return kva;
}

int hax_unmap_page_frame(hax_kmap_phys *kmap)
{
    if (!kmap) {
        hax_log(HAX_LOGE, "%s: kmap == NULL\n", __func__);
        return -EINVAL;
    }
    if (!kmap->kva) {
        // This is a common use case, hence not treated as an error
        return 0;
    }

    MmUnmapIoSpace(kmap->kva, PAGE_SIZE_4K);
    kmap->kva = NULL;
    return 0;
}
