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

/*
 * Per the experimental, create IOBuffereMemoryDescriptor with wired big-chunk
 * memory in user space address space failed (pageable allocation works), but
 * worked if the descriptor is created in kernel stack.
 * So we simply create all guest RAMs through descriptor in kernel stack and map
 * it to QEMU.
 * Considering the big kernel address space even in 32-bit mac, hope this works.
 */
#include <mach/mach_types.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <sys/errno.h>
#include "com_intel_hax.h"

int hax_setup_vcpumem(struct hax_vcpu_mem *mem, uint64_t uva, uint32_t size,
                      int flags)
{
    struct darwin_vcpu_mem *hinfo;
    struct IOMemoryDescriptor *md = NULL;
    struct IOMemoryMap *mm = NULL;
    struct IOBufferMemoryDescriptor *bmd = NULL;
    IOReturn result;
    IOOptionBits options;

    if (!mem)
        return -EINVAL;

    hinfo = (struct darwin_vcpu_mem *)hax_vmalloc(
            sizeof(struct darwin_vcpu_mem), 0);
    if (!hinfo)
        return -ENOMEM;

    /* The VA is valid and allocated in advance by user space */
    options = kIODirectionIn | kIODirectionOut | kIOMemoryKernelUserShared |
              kIOMemoryMapperNone;

    if (flags & HAX_VCPUMEM_VALIDVA) {
        md = IOMemoryDescriptor::withAddressRange(uva, size, options,
                                                  current_task());
        if (!md) {
            hax_log(HAX_LOGE, "Failed to create mapping for %llx\n", uva);
            goto error;
        }

        result = md->prepare();
        if (result != KERN_SUCCESS) {
            hax_log(HAX_LOGE, "Failed to prepare\n");
            goto error;
        }

        mm = md->createMappingInTask(kernel_task, 0, kIOMapAnywhere, 0, size);
        if (!mm) {
            hax_log(HAX_LOGE, "Failed to map into kernel\n");
            md->complete();
            goto error;
        }
        mem->uva = uva;
        mem->kva = (void *)mm->getVirtualAddress();
    } else {
        /*
         * BMD init in user space task is pageable, so have to init it in kernel
         * firstly.
         */
        bmd = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, options,
                                                          size, page_size);
        if (!bmd) {
            printf("Failed to alloate tunnel info\n");
            goto error;
        }

        mm = bmd->createMappingInTask(current_task(), 0, kIOMapAnywhere, 0,
                                      size);
        if (!mm) {
            printf("Failed to map tunnel to user\n");
            goto error;
        }

        mem->kva = bmd->getBytesNoCopy();
        mem->uva = mm->getAddress();
    }
    mem->size = size;
    hinfo->bmd = bmd;
    hinfo->md = md;
    hinfo->umap = mm;
    mem->hinfo = hinfo;

    return 0;
error:
    if (md)
        md->release();
    if (mm)
        mm->release();
    if (bmd)
        bmd->release();
    if (hinfo)
        hax_vfree(hinfo, sizeof(struct darwin_vcpu_mem));
    return -1;
}

extern "C" int hax_clear_vcpumem(struct hax_vcpu_mem *mem)
{
    struct darwin_vcpu_mem *hinfo;

    if (!mem || !mem->hinfo)
        return -EINVAL;
    hinfo = (struct darwin_vcpu_mem *)mem->hinfo;
    if (hinfo->umap)
        hinfo->umap->release();
    if (hinfo->md) {
        hinfo->md->complete();
        hinfo->md->release();
    }
    if (hinfo->bmd)
        hinfo->bmd->release();
    hax_vfree(hinfo, sizeof(struct darwin_vcpu_mem));
    mem->hinfo = NULL;
    return 0;
}

/* In darwin, we depend on boot code to set the limit */
extern "C" uint64_t hax_get_memory_threshold(void) {
    // Since there is no memory cap, just return a sufficiently large value
    return 1ULL << 48;  // PHYSADDR_MAX + 1
}
