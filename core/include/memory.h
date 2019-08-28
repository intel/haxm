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

#ifndef HAX_CORE_MEMORY_H_
#define HAX_CORE_MEMORY_H_

#include "../../include/hax_types.h"
#include "../../include/hax_list.h"

#define HAX_CHUNK_SHIFT 21
#define HAX_CHUNK_SIZE  (1U << HAX_CHUNK_SHIFT)  // 2MB

typedef struct hax_chunk {
    hax_memdesc_user memdesc;
    uint64_t base_uva;
    // In bytes, page-aligned, == HAX_CHUNK_SIZE in most cases
    uint64_t size;
} hax_chunk;

typedef struct hax_ramblock {
    uint64_t base_uva;
    // In bytes, page-aligned
    uint64_t size;
    // hax_chunk *chunks[(size + HAX_CHUNK_SIZE - 1) / HAX_CHUNK_SIZE]
    hax_chunk **chunks;
    // One bit per chunk indicating whether the chunk has been (or is being)
    // allocated/pinned or not
    uint8_t *chunks_bitmap;
    // Reference count of this object
    int ref_count;
    // Whether this RAM block is associated with a stand-alone mapping
    bool is_standalone;
    // Turns this object into a list node
    hax_list_node entry;
} hax_ramblock;

typedef struct hax_memslot {
    // == base_gpa >> PG_ORDER_4K
    uint64_t base_gfn;
    // == size >> PG_ORDER_4K
    uint64_t npages;
    // Must not be NULL
    struct hax_ramblock *block;
    // In bytes. < block->size
    uint64_t offset_within_block;
    // Read-only, etc.
    uint32_t flags;
    // Turns this object into a list node
    hax_list_node entry;
} hax_memslot;

// Read-only mapping, == HAX_RAM_INFO_ROM in hax_interface.h
#define HAX_MEMSLOT_READONLY (1 << 0)
// Stand-alone mapping, == HAX_RAM_INFO_STANDALONE in hax_interface.h
#define HAX_MEMSLOT_STANDALONE (1 << 6)

// Unmapped, == HAX_RAM_INFO_INVALID in hax_interface.h
// Not to be used by hax_memslot::flags
#define HAX_MEMSLOT_INVALID (1 << 7)

typedef struct hax_gpa_prot {
    // A bitmap where each bit represents the protection status of a guest page
    // frame: 1 means protected (i.e. no access allowed), 0 not protected.
    // TODO: Support fine-grained protection (R/W/X).
    uint8_t *bitmap;
    // the first gfn not covered by the bitmap
    uint64_t end_gfn;
} hax_gpa_prot;

typedef struct hax_gpa_space {
    // TODO: Add a lock to prevent concurrent accesses to |ramblock_list| and
    // |memslot_list|
    hax_list_head ramblock_list;
    hax_list_head memslot_list;
    hax_list_head listener_list;
    hax_gpa_prot prot;
} hax_gpa_space;

typedef struct hax_gpa_space_listener hax_gpa_space_listener;
struct hax_gpa_space_listener {
    // For MMIO => RAM/ROM
    void (*mapping_added)(hax_gpa_space_listener *listener, uint64_t start_gfn,
                          uint64_t npages, uint64_t uva, uint8_t flags);
    // For RAM/ROM => MMIO
    void (*mapping_removed)(hax_gpa_space_listener *listener, uint64_t start_gfn,
                            uint64_t npages, uint64_t uva, uint8_t flags);
    // For RAM/ROM => RAM/ROM
    void (*mapping_changed)(hax_gpa_space_listener *listener, uint64_t start_gfn,
                            uint64_t npages, uint64_t old_uva, uint8_t old_flags,
                            uint64_t new_uva, uint8_t new_flags);
    hax_gpa_space *gpa_space;
    // Points to listener-specific data, e.g. a |hax_ept_tree|
    void *opaque;
    // Turns this object into a list node
    hax_list_node entry;
};

// Initializes the given list of |hax_ramblock|s.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |list| is NULL.
int ramblock_init_list(hax_list_head *list);

// Frees up resources taken up by the given list of |hax_ramblock|s, including
// the |hax_chunk|s managed by each |hax_ramblock|.
void ramblock_free_list(hax_list_head *list);

// Dumps the given list of |hax_ramblock|s for debugging.
void ramblock_dump_list(hax_list_head *list);

// Finds in the given sorted list of |hax_ramblock|s the one that contains the
// given UVA.
// |list|: The sorted list of |hax_ramblock|s to search in.
// |uva|: The UVA to search for.
// |start|: The list node from which to search. If NULL, defaults to the list
//          head.
// Returns a pointer to the |hax_ramblock| containing |uva|, or NULL if no such
// |hax_ramblock| exists.
hax_ramblock * ramblock_find(hax_list_head *list, uint64_t uva,
                             hax_list_node *start);

// Creates a |hax_ramblock| from the given UVA range and inserts it into the
// given sorted list of |hax_ramblock|s.
// |list|: The sorted list of |hax_ramblock|s to add the new |hax_ramblock| to.
// |base_uva|: The start of the UVA range.
// |size|: The size of the UVA range, in bytes. Should be page-aligned.
// |start|: The list node from which to search for the insertion point. If NULL,
//          defaults to the list head.
// |block|: A buffer to store a pointer to the new |hax_ramblock|. Can be NULL
//          if the caller does not want the new |hax_ramblock| to be returned.
// Returns 0 on success, with |*block| pointing to the new |hax_ramblock|; or
// one of the following error codes, with |*block| set to NULL:
// -EINVAL: Invalid input, e.g. the given UVA range overlaps with that of an
//          existing |hax_ramblock|.
// -ENOMEM: Memory allocation error.
int ramblock_add(hax_list_head *list, uint64_t base_uva, uint64_t size,
                 hax_list_node *start, hax_ramblock **block);

// Returns the |hax_chunk| at the given offset in the given |hax_ramblock|’s UVA
// range. Allocates the |hax_chunk| if it does not yet exist (i.e. has not been
// pinned in host RAM) and the caller so desires.
// |block|: The |hax_ramblock| in which to search for the desired |hax_chunk|.
// |uva_offset|: An offset, in bytes, within the UVA range of |block|. The
//               |hax_chunk| at this offset will be returned. Should be less
//               than |block->size|.
// |alloc|: If true, allocates the |hax_chunk| (pinning its UVA range in host
//          RAM) if it has not been allocated yet.
// Returns the |hax_chunk| at |uva_offset|, or NULL in one of the following
// cases:
// a) Invalid input, e.g. |block| is NULL, or |uva_offset| is invalid (i.e.
//    greater than or equal to |block->size|).
// b) The |hax_chunk| has not been allocated and |alloc| is false.
// c) The |hax_chunk| had not been allocated and |alloc| is true, but allocation
//    was not successful.
hax_chunk * ramblock_get_chunk(hax_ramblock *block, uint64_t uva_offset,
                               bool alloc);

// Increments the reference count of an existing RAM block. The reference count
// of a new RAM block created by ramblock_add() is initialized to 0. Whenever a
// new reference to a RAM block is made, this function must be called.
// |block|: A pointer to |hax_ramblock| being referenced.
void ramblock_ref(hax_ramblock *block);

// Decrements the reference count of the specified RAM block. Whenever a
// reference to a RAM block is removed, this function must be called. If the
// resulting reference count hits zero, removes the RAM block from the list it
// belongs to, and frees the RAM block along with all the resources allocated
// for it.
// |block|: A pointer to |hax_ramblock| being dereferenced.
void ramblock_deref(hax_ramblock *block);

// Initializes |hax_memslot|-related data structures in the given
// |hax_gpa_space|.
// For now, the only data structure to initialize is |memslot_list|. Later, we
// may add an array list of |hax_memslot|s.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |gpa_space| is NULL.
// -ENOMEM: Memory allocation error.
int memslot_init_list(hax_gpa_space *gpa_space);

// Frees up resources taken up by |hax_memslot|-related data structures in the
// given |hax_gpa_space|. Does not free the |hax_ramblock| associated with each
// |hax_memslot|.
void memslot_free_list(hax_gpa_space *gpa_space);

// Dumps the list of |hax_memslot|s in the given |hax_gpa_space| for debugging.
void memslot_dump_list(hax_gpa_space *gpa_space);

// Sets a new mapping for the given GFN range in the given |hax_gpa_space|, by
// updating the |hax_memslot|s that belong to the |hax_gpa_space|.
// |gpa_space|: The |hax_gpa_space| to apply the mapping to.
// |start_gfn|: The start of the GFN range.
// |npages|: The number of pages covered by the GFN range.
// |uva|: The UVA that |start_gpa| maps to.
// |flags|: The type of the mapping, i.e. RAM (|~HAX_MEMSLOT_READONLY|), ROM
//          (|HAX_MEMSLOT_READONLY|) or MMIO (|HAX_MEMSLOT_INVALID|).
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input.
// -ENOMEM: Memory allocation error.
int memslot_set_mapping(hax_gpa_space *gpa_space, uint64_t start_gfn,
                        uint64_t npages, uint64_t uva, uint32_t flags);

// Finds in the given |hax_gpa_space| the |hax_memslot| containing the given
// GFN.
// |gpa_space|: The |hax_gpa_space| to search in.
// |gfn|: The GFN to search for.
// Returns a pointer to the |hax_memslot| containing |gfn|, or NULL if no such
// |hax_memslot| exists (indicating that |gfn| is reserved for MMIO).
hax_memslot * memslot_find(hax_gpa_space *gpa_space, uint64_t gfn);

// Initializes the given |hax_gpa_space|.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |gpa_space| is NULL.
// -ENOMEM: Memory allocation error.
int gpa_space_init(hax_gpa_space *gpa_space);

// Frees up resources taken by the given |hax_gpa_space|.
void gpa_space_free(hax_gpa_space *gpa_space);

// Registers the given |hax_gpa_space_listener| with the given |hax_gpa_space|.
void gpa_space_add_listener(hax_gpa_space *gpa_space,
                            hax_gpa_space_listener *listener);

// Removes the given |hax_gpa_space_listener| from the given |hax_gpa_space|’s
// list of listeners.
void gpa_space_remove_listener(hax_gpa_space *gpa_space,
                               hax_gpa_space_listener *listener);

// Copies the given number of bytes from guest RAM/ROM into the given buffer.
// |gpa_space|: The |hax_gpa_space| of the guest.
// |start_gpa|: The start GPA from which to read data. |start_gpa| and |len|
//              together specify a GPA range that may span multiple guest page
//              frames, each of which must be mapped as either RAM or ROM.
// |len|: The number of bytes to copy.
// |data|: The destination buffer to copy the bytes into, whose size must be at
//         least |len| bytes.
// Returns the number of bytes actually copied, or one of the following error
// codes:
// -EINVAL: Invalid input, e.g. |data| is NULL, or the GPA range specified by
//          |start_gpa| and |len| touches an MMIO region.
// -ENOMEM: Unable to map the requested guest page frames into KVA space.
int gpa_space_read_data(hax_gpa_space *gpa_space, uint64_t start_gpa, int len,
                        uint8_t *data);

// Copies the given number of bytes from the given buffer to guest RAM.
// |gpa_space|: The |hax_gpa_space| of the guest.
// |start_gpa|: The start GPA to which to write data. |start_gpa| and |len|
//              together specify a GPA range that may span multiple guest page
//              frames, each of which must be mapped as RAM.
// |len|: The number of bytes to copy.
// |data|: The source buffer to copy the bytes from, whose size must be at least
//         |len| bytes.
// Returns the number of bytes actually copied, or one of the following error
// codes:
// -EINVAL: Invalid input, e.g. |data| is NULL, or the GPA range specified by
//          |start_gpa| and |len| touches a MMIO region.
// -ENOMEM: Unable to map the requested guest page frames into KVA space.
// -EACCES: The GPA range specified by |start_gpa| and |len| touches a ROM
//          region.
int gpa_space_write_data(hax_gpa_space *gpa_space, uint64_t start_gpa, int len,
                         uint8_t *data);

// Maps the given guest page frame into KVA space, stores the KVA mapping in the
// given buffer, and returns the KVA. The caller must destroy the KVA mapping
// after use by calling gpa_space_unmap_page().
// |gpa_space|: The GPA space of the guest.
// |gfn|: The GFN of the guest page frame to map.
// |kmap|: A buffer to store a host-specific KVA mapping descriptor. Must not be
//         NULL.
// |writable|: A buffer to store a Boolean value indicating whether the guest
//             page frame is writable (i.e. maps to RAM). Can be NULL if the
//             caller only wants to read from the page.
// Returns NULL on error.
void * gpa_space_map_page(hax_gpa_space *gpa_space, uint64_t gfn,
                          hax_kmap_user *kmap, bool *writable);

// Destroys the KVA mapping previously created by gpa_space_map_page().
void gpa_space_unmap_page(hax_gpa_space *gpa_space, hax_kmap_user *kmap);

// Returns the host PFN to which the given GPA maps.
// |gpa_space|: The GPA space of the guest.
// |gfn|: The GFN to convert.
// |flags|: A buffer to store the mapping properties of |gpa|, i.e. whether
//          |gpa| maps to RAM, ROM or MMIO. Can be NULL if the caller is not
//          interested in this information.
// Returns INVALID_PFN on error, including the case where |gfn| is reserved for
// MMIO.
uint64_t gpa_space_get_pfn(hax_gpa_space *gpa_space, uint64_t gfn, uint8_t *flags);

int gpa_space_protect_range(struct hax_gpa_space *gpa_space,
                            uint64_t start_gpa, uint64_t len, uint32_t flags);

// Adjust gpa protection bitmap size. Once a bigger gfn is met, allocate
// a new bitmap and copy the old bitmap contents.
// |gpa_space|: The GPA space of the guest.
// |end_gfn|: The first GFN not covered by the new bitmap.
int gpa_space_adjust_prot_bitmap(struct hax_gpa_space *gpa_space,
                                 uint64_t end_gfn);

bool gpa_space_is_page_protected(struct hax_gpa_space *gpa_space, uint64_t gfn);
bool gpa_space_is_chunk_protected(struct hax_gpa_space *gpa_space, uint64_t gfn,
                                  uint64_t *fault_gfn);

// Allocates a |hax_chunk| for the given UVA range, and pins the corresponding
// host page frames in RAM.
// |base_uva|: The start of the UVA range. Should be page-aligned.
// |size|: The size of the UVA range, in bytes. Should be page-aligned.
// |chunk|: A buffer to store a pointer to the new |hax_chunk|.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |chunk| is NULL, or the UVA range given by
//          |base_uva| and |size| is not valid.
// -ENOMEM: Memory allocation error.
int chunk_alloc(uint64_t base_uva, uint64_t size, hax_chunk **chunk);

// Frees up resources taken up by the given |hax_chunk|, which includes
// unpinning all host page frames backing it.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |chunk| is NULL.
int chunk_free(hax_chunk *chunk);

#endif  // HAX_CORE_MEMORY_H_
