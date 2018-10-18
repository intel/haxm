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

#ifndef HAX_HOST_MEM_H_
#define HAX_HOST_MEM_H_

#include "hax_types.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Ensures all host page frames backing the virtual pages in the given UVA range
// are allocated and pinned in host RAM.
// |start_uva|: The start of the UVA range. Should be page-aligned.
// |size|: The size of the UVA range, in bytes. Should be page-aligned.
// |memdesc|: A buffer to store a host-specific memory descriptor that describes
//            the allocated page frames, which may be discontiguous in HPA
//            space. It should later be freed by hax_unpin_user_pages().
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |memdesc| is NULL, or the UVA range given by
//          |start_uva| and |size| is not valid.
// -ENOMEM: Memory allocation error.
// -EFAULT: Host OS failing to create the memory descriptor.
int hax_pin_user_pages(uint64_t start_uva, uint64_t size,
                       hax_memdesc_user *memdesc);

// Frees all host page frames previously pinned by hax_pin_user_pages().
// |memdesc|: The host-specific memory descriptor previously populated by
//            hax_pin_user_pages().
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |memdesc| is NULL.
int hax_unpin_user_pages(hax_memdesc_user *memdesc);

// Returns the PFN of the host page frame backing a specific virtual page in the
// UVA range described by the given |hax_memdesc_user|.
// |memdesc|: A |hax_memdesc_user| previously populated by hax_pin_user_pages().
//            Describes the UVA range whose corresponding host page frames are
//            to be unpinned.
// |uva_offset|: The offset, in bytes, of the virtual page (or any byte within
//               it) in the UVA range described by |memdesc|.
// Returns INVALID_PFN on error.
uint64_t hax_get_pfn_user(hax_memdesc_user *memdesc, uint64_t uva_offset);

// Maps the given subrange of the UVA range described by the given
// |hax_memdesc_user| into KVA space, stores the mapping in the given buffer,
// and returns the start KVA. The caller must destroy the mapping after use by
// calling hax_unmap_user_pages().
// |memdesc|: A memory descriptor that was previously populated by
//            hax_pin_user_pages().
// |uva_offset|: The offset, in bytes, of the subrange within the UVA range
//               described by |memdesc|. Should be page-aligned.
// |size|: The size of the UVA subrange, in bytes. Should be page-aligned.
// |kmap|: A buffer to store a host-specific KVA mapping descriptor.
// Returns NULL on error.
void * hax_map_user_pages(hax_memdesc_user *memdesc, uint64_t uva_offset,
                          uint64_t size, hax_kmap_user *kmap);

// Destroys the given KVA mapping previously created by hax_map_user_pages().
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |kmap| is NULL.
int hax_unmap_user_pages(hax_kmap_user *kmap);

// Allocates one host page frame that is pinned in RAM.
// |flags|: Indicates special requirements for the allocation, if any. Valid
//          flags are listed below.
// |memdesc|: A buffer to store a host-specific memory descriptor that describes
//            the allocated page frame. It should later be freed by
//            hax_free_page_frame().
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |memdesc| is NULL.
// -ENOMEM: Memory allocation error.
int hax_alloc_page_frame(uint8_t flags, hax_memdesc_phys *memdesc);

// Indicates that the allocated page frame should be initialized with zeroes
#define HAX_PAGE_ALLOC_ZEROED   0x01
// Indicates that the HPA of the allocated page frame should be < 2^32
#define HAX_PAGE_ALLOC_BELOW_4G 0x02

// Frees a host page frame previously allocated by hax_alloc_page_frame().
// |memdesc|: The host-specific memory descriptor previously populated by
//            hax_alloc_page_frame().
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |memdesc| is NULL.
int hax_free_page_frame(hax_memdesc_phys *memdesc);

// Returns the PFN of the host page frame described by the given
// |hax_memdesc_phys|, which was previously populated by hax_alloc_page_frame().
// Returns INVALID_PFN on error.
uint64_t hax_get_pfn_phys(hax_memdesc_phys *memdesc);

// Returns the KVA of the host page frame described by the given
// |hax_memdesc_phys|, which was previously populated by hax_alloc_page_frame().
// The caller does not need to unmap the returned KVA after use.
// Returns NULL on error.
void * hax_get_kva_phys(hax_memdesc_phys *memdesc);

// Maps the given PFN into KVA space, stores the mapping in the given buffer,
// and returns the start KVA. The caller must destroy the mapping after use by
// calling hax_unmap_page_frame().
// |pfn|: pfn: The PFN to map.
// |kmap|: A buffer to store a host-specific KVA mapping descriptor.
// Returns NULL on error.
void * hax_map_page_frame(uint64_t pfn, hax_kmap_phys *kmap);

// Destroys the given KVA mapping previously created by hax_map_page_frame().
// Returns 0 on success (including the case where |kmap| is neither NULL nor a
// valid KVA mapping descriptor), or one of the following error codes:
// -EINVAL: Invalid input, e.g. |kmap| is NULL.
int hax_unmap_page_frame(hax_kmap_phys *kmap);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // HAX_HOST_MEM_H_

