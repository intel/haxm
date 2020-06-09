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

//#include <ntddk.h>
#include <ntifs.h>
#include <string.h>

#include "hax_win.h"
#include "hax_entry.h"

static inline int hax_free_pmdl(PMDL pmdl, int flags)
{
    if (!pmdl)
        return 0;

    if (flags & HAX_VCPUMEM_VALIDVA) {
        MmUnlockPages(pmdl);
        IoFreeMdl(pmdl);
    } else {
        if (pmdl->MappedSystemVa)
            MmUnmapLockedPages(pmdl->MappedSystemVa, pmdl);
        if (pmdl->StartVa)
            MmUnmapLockedPages(pmdl->StartVa, pmdl);
        MmFreePagesFromMdl(pmdl);
        ExFreePool(pmdl);
    }
    return 0;
}

int hax_clear_vcpumem(struct hax_vcpu_mem *mem)
{
    struct windows_vcpu_mem *hinfo;

    if (!mem || !mem->hinfo)
        return 0;
    hinfo = (struct windows_vcpu_mem *)mem->hinfo;
    if (hinfo->pmdl)
        hax_free_pmdl(hinfo->pmdl,
                      hinfo->flags & HAX_VCPUMEM_VALIDVA);

    hax_vfree(mem->hinfo, sizeof(struct windows_vcpu_mem));
    mem->hinfo = NULL;
    return 0;
}

int hax_valid_uva(uint64_t uva, uint64_t size)
{
    return 1;
// FIXME: Is it still available to verify the address?
//    try {
//        ProbeForRead(&uva, size, PAGE_SIZE);
//    } except (EXCEPTION_EXECUTE_HANDLER) {
//        return 0;
//    }
//    return 1;
}

int hax_setup_vcpumem(struct hax_vcpu_mem *mem, uint64_t uva, uint32_t size,
                      int flags)
{
    struct windows_vcpu_mem *hinfo;
    PMDL pmdl = NULL;
    PHYSICAL_ADDRESS high_addr, low_addr, skip_bytes;

    if (!mem)
        return -EINVAL;

    hinfo = hax_vmalloc(sizeof(struct windows_vcpu_mem), 0);
    if (!hinfo)
        return -ENOMEM;

    hinfo->flags = flags;

    /* The VA is valid, usually it's allocated in qemu */
    if (flags & HAX_VCPUMEM_VALIDVA) {
        pmdl = IoAllocateMdl((void*)uva, size, FALSE, FALSE, NULL);
        if (!pmdl) {
            hax_log(HAX_LOGE, "Failed to allocate memory for va: %llx\n", uva);
            goto fail;
        }

        try {
            MmProbeAndLockPages(pmdl, UserMode, IoReadAccess|IoWriteAccess);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            hax_log(HAX_LOGE, "Failed to probe pages for guest's memory!\n");
            IoFreeMdl(pmdl);
            pmdl = NULL;
            goto fail;
        }
        mem->uva = uva;

#ifdef _WIN64
        mem->kva = MmGetSystemAddressForMdlSafe(pmdl, NormalPagePriority);
        if (!mem->kva) {
            hax_log(HAX_LOGE, "Failed to map the pmdl to system address!\n");
            goto fail;
        }
#else
        mem->kva = 0;
#endif
    } else {
        low_addr.QuadPart = 0;
        skip_bytes.QuadPart = 0;
        high_addr.QuadPart = MAX_HOST_MEM_SIZE;

#ifdef MDL_HAX_PAGE
        pmdl = MmAllocatePagesForMdlEx(low_addr, high_addr, skip_bytes, size,
                                       MmCached, MM_ALLOCATE_FULLY_REQUIRED);
#else
        pmdl = MmAllocatePagesForMdl(low_addr, high_addr, skip_bytes, size);
#endif

        if (!pmdl || MmGetMdlByteCount(pmdl) < size) {
            hax_log(HAX_LOGE, "Failed to alloate pmdl!\n");
            if (pmdl)
                hax_log(HAX_LOGD, "allocated size:%d, size:%d\n",
                        MmGetMdlByteCount(pmdl), size);
            goto fail;
        }

        mem->uva = (uint64_t)MmMapLockedPagesSpecifyCache(pmdl, UserMode,
                                                          MmCached, NULL,
                                                          FALSE,
                                                          NormalPagePriority);
        if (!mem->uva) {
            hax_log(HAX_LOGE, "Failed to map tunnel to user space\n");
            goto fail;
        }
        mem->kva = MmMapLockedPagesSpecifyCache(pmdl, KernelMode, MmCached,
                                                NULL, FALSE,
                                                NormalPagePriority);
        if (!mem->kva) {
            hax_log(HAX_LOGE, "Failed to map tunnel to kernel space\n");
            goto fail;
        }
        hax_log(HAX_LOGD, "kva %llx va %llx\n", (uint64_t)mem->kva, mem->uva);
    }
    mem->size = size;
    mem->hinfo = hinfo;
    hinfo->flags = flags;
    hinfo->pmdl = pmdl;
    return 0;

fail:
    if (pmdl)
        hax_free_pmdl(pmdl, flags);
    if (hinfo)
        hax_vfree(hinfo, sizeof(struct windows_vcpu_mem));
    return -1;
}

uint64_t hax_get_memory_threshold(void)
{
    // Since there is no memory cap, just return a sufficiently large value
    return 1ULL << 48;  // PHYSADDR_MAX + 1
}
