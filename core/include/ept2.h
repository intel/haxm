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

#ifndef HAX_CORE_EPT2_H_
#define HAX_CORE_EPT2_H_

#include "../../include/hax_types.h"
#include "../../include/hax_list.h"
#include "memory.h"
#include "vmx.h"

#define HAX_EPT_LEVEL_PML4 3
#define HAX_EPT_LEVEL_PDPT 2
#define HAX_EPT_LEVEL_PD   1
#define HAX_EPT_LEVEL_PT   0
#define HAX_EPT_LEVEL_MAX  HAX_EPT_LEVEL_PML4

#define HAX_EPT_TABLE_SHIFT 9
#define HAX_EPT_TABLE_SIZE  (1 << HAX_EPT_TABLE_SHIFT)

#define HAX_EPT_ACC_R 0x1
#define HAX_EPT_ACC_W 0x2
#define HAX_EPT_ACC_X 0x4

#define HAX_EPT_PERM_NONE 0x0
#define HAX_EPT_PERM_RWX  0x7
#define HAX_EPT_PERM_RX   0x5
#define HAX_EPT_PERM_W    0x2

#define HAX_EPT_MEMTYPE_UC 0x0
#define HAX_EPT_MEMTYPE_WB 0x6

typedef union hax_epte {
    uint64_t value;
    struct {
        uint64_t perm          : 3;   // bits 2..0
        uint64_t ept_mt        : 3;   // bits 5..3
        uint64_t ignore_pat_mt : 1;   // bit 6
        uint64_t is_large_page : 1;   // bit 7
        uint64_t accessed      : 1;   // bit 8
        uint64_t dirty         : 1;   // bit 9
        uint64_t ignored1      : 2;   // bits 11..10
        uint64_t pfn           : 40;  // bits 51..12
        uint64_t ignored2      : 11;  // bits 62..52
        uint64_t supress_ve    : 1;   // bit 63
    };
} hax_epte;

typedef union hax_eptp {
    uint64_t value;
    struct {
        uint64_t ept_mt       : 3;   // bits 2..0
        uint64_t max_level    : 3;   // bits 5..3
        uint64_t track_access : 1;   // bit 6
        uint64_t reserved1    : 5;   // bits 11..7
        uint64_t pfn          : 40;  // bits 51..12
        uint64_t reserved2    : 12;  // bits 63..52
    };
} hax_eptp;

typedef struct hax_ept_page {
    hax_memdesc_phys memdesc;
    // Turns this object into a list node
    hax_list_node entry;
} hax_ept_page;

typedef struct hax_ept_page_kmap {
    hax_ept_page *page;
    void *kva;
} hax_ept_page_kmap;

#define HAX_EPT_FREQ_PAGE_COUNT 10

typedef struct hax_ept_tree {
    hax_list_head page_list;
    hax_eptp eptp;
    hax_ept_page_kmap freq_pages[HAX_EPT_FREQ_PAGE_COUNT];
    bool invept_pending;
    hax_spinlock *lock;
    // TODO: pointer to vm_t?
} hax_ept_tree;

// Initializes the given |hax_ept_tree|. This includes allocating the root
// |hax_ept_page| (PML4 table), computing the EPTP, initializing the cache for
// frequently-used |hax_ept_page|s, etc.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |tree| is NULL.
// -ENOMEM: Memory allocation error.
int ept_tree_init(hax_ept_tree *tree);

// Frees up resources taken up by the given |hax_ept_tree|, including all the
// constituent |hax_ept_page|s.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |tree| is NULL.
int ept_tree_free(hax_ept_tree *tree);

// Acquires the lock of the given |hax_ept_tree|. A thread must make sure it has
// acquired the lock before modifying the |hax_ept_tree|.
void ept_tree_lock(hax_ept_tree *tree);

// Releases the lock of the given |hax_ept_tree|. The same thread that called
// ept_tree_lock() must release the lock when it has finished modifying the
// |hax_ept_tree|.
void ept_tree_unlock(hax_ept_tree *tree);

// Creates a leaf |hax_epte| that maps the given GFN to the given value (which
// includes the target PFN and mapping properties). Also creates any missing
// |hax_ept_page|s and non-leaf |hax_epte|s in the process.
// |tree|: The |hax_ept_tree| to modify.
// |gfn|: The GFN to create the |hax_epte| for. The leaf |hax_epte|
//        corresponding to this GFN should not be present.
// |value|: The value for the new leaf |hax_epte|. It should mark the |hax_epte|
//          as present.
// Returns 0 on success, or one of the following error codes:
// -EINVAL: Invalid input, e.g. |tree| is NULL, or |value| denotes a non-present
//          |hax_epte|.
// -EEXIST: The leaf |hax_epte| corresponding to |gfn| is already present, whose
//          value is different from |value|.
// -ENOMEM: Memory allocation/mapping error.
int ept_tree_create_entry(hax_ept_tree *tree, uint64_t gfn, hax_epte value);

// Creates leaf |hax_epte|s that map the given GFN range, using PFNs obtained
// from the given |hax_chunk| and the given mapping properties. Also creates any
// missing |hax_ept_page|s and non-leaf |hax_epte|s in the process.
// |tree|: The |hax_ept_tree| to modify. Must not be NULL.
// |start_gfn|: The start of the GFN range to map.
// |npages|: The number of pages covered by the GFN range. Must not be 0.
// |chunk|: The |hax_chunk| that covers all the host virtual pages (already
//          pinned in RAM) backing the guest page frames in the GFN range. Must
//          not be NULL.
// |offset_within_chunk|: The offset, in bytes, of the host virtual page backing
//                        the guest page frame at |start_gfn| within the UVA
//                        range covered by |chunk|. The UVA range defined by
//                        this offset and the size of |chunk| must cover no
//                        fewer than |npages| pages.
// |flags|: The mapping properties (e.g. read-only, etc.) applicable to the
//          entire GFN range.
// Returns the number of leaf |hax_epte|s created (i.e. changed from non-present
// to present), or one of the following error codes:
// -EEXIST: Any of the leaf |hax_epte|s corresponding to the GFN range is
//          already present and different from what would be created.
// -ENOMEM: Memory allocation/mapping error.
int ept_tree_create_entries(hax_ept_tree *tree, uint64_t start_gfn, uint64_t npages,
                            hax_chunk *chunk, uint64_t offset_within_chunk,
                            uint8_t flags);

// Invalidates all leaf |hax_epte|s corresponding to the given GFN range, i.e.
// marks them as not present. Also sets the |invept_pending| flag of the
// |hax_ept_tree| (but does not invoke INVEPT) if any of such |hax_epte|s was
// present.
// |tree|: The |hax_ept_tree| to modify.
// |start_gfn|: The start of the GFN range, whose corresponding |hax_epte|s are
//              to be invalidated.
// |npages|: The number of pages covered by the GFN range.
// Returns the number of leaf |hax_epte|s invalidated (i.e. changed from present
// to not present), or one of the following error codes:
// -EINVAL: Invalid input, e.g. |tree| is NULL.
// -ENOMEM: Memory mapping error.
int ept_tree_invalidate_entries(hax_ept_tree *tree, uint64_t start_gfn,
                                uint64_t npages);

// Returns the leaf |hax_epte| that maps the given GFN. If the leaf |hax_epte|
// does not exist, returns an all-zero |hax_epte|.
// Returns an invalid |hax_epte| on error.
hax_epte ept_tree_get_entry(hax_ept_tree *tree, uint64_t gfn);

// A visitor callback invoked by ept_tree_walk() on each |hax_epte| visited
// along the walk.
// |tree|: The |hax_ept_tree| that |epte| belongs to.
// |gfn|: The GFN used by ept_tree_walk().
// |level|: The level in |tree| that |epte| belongs to (one of the
//          |HAX_EPT_LEVEL_*| constants.
// |epte|: The |hax_epte| to visit.
// |opaque|: Additional data provided by the caller of ept_tree_walk().
typedef void (*epte_visitor)(hax_ept_tree *tree, uint64_t gfn, int level,
                             hax_epte *epte, void *opaque);

// Walks the given |hax_ept_tree| from the root as if the given GFN were being
// translated. Invokes the given callback on each |hax_epte| visited. Returns
// after visiting the leaf |hax_epte| or a |hax_epte| that is not present (or
// both).
// |tree|: The |hax_ept_tree| to walk.
// |gfn|: The GFN that defines the |hax_epte|s in |tree| to visit.
// |visit_epte|: The callback to be invoked on each |hax_epte| visited. Should
//               not be NULL.
// |opaque|: An arbitrary pointer passed as-is to |visit_current_epte|.
void ept_tree_walk(hax_ept_tree *tree, uint64_t gfn, epte_visitor visit_epte,
                   void *opaque);

// Handles a guest memory mapping change from RAM/ROM to MMIO. Used as a
// |hax_gpa_space_listener| callback.
// |listener|: The |hax_gpa_space_listener| that invoked this callback.
// |start_gfn|: The start of the GFN range whose mapping has changed.
// |npages|: The number of pages covered by the GFN range.
// |uva|: The old UVA to which |start_gfn| mapped before the change.
// |flags|: The old mapping properties for the GFN range, e.g. whether it was
//          mapped as read-only.
void ept_handle_mapping_removed(hax_gpa_space_listener *listener,
                                uint64_t start_gfn, uint64_t npages, uint64_t uva,
                                uint8_t flags);

// Handles a guest memory mapping change from RAM/ROM to RAM/ROM. Used as a
// |hax_gpa_space_listener| callback.
// |listener|: The |hax_gpa_space_listener| that invoked this callback.
// |start_gfn|: The start of the GFN range whose mapping has changed.
// |npages|: The number of pages covered by the GFN range.
// |old_uva|: The old UVA to which |start_gfn| mapped before the change.
// |old_flags|: The old mapping properties for the GFN range, e.g. whether it
//              was mapped as read-only.
// |new_uva|: The new UVA to which |start_gfn| maps after the change.
// |new_flags|: The new mapping properties for the GFN range, e.g. whether it is
//              mapped as read-only.
void ept_handle_mapping_changed(hax_gpa_space_listener *listener,
                                uint64_t start_gfn, uint64_t npages,
                                uint64_t old_uva, uint8_t old_flags,
                                uint64_t new_uva, uint8_t new_flags);

// Handles an EPT violation due to a guest RAM/ROM access.
// |gpa_space|: The |hax_gpa_space| of the guest.
// |tree|: The |hax_ept_tree| of the guest.
// |qual|: The VMCS Exit Qualification field that describes the EPT violation.
// |gpa|: The faulting GPA.
// Returns 1 if the faulting GPA is mapped to RAM/ROM and the fault is
// successfully handled, 0 if the faulting GPA is reserved for MMIO and the
// fault is not handled, or one of the following error codes:
// -EACCES: Unexpected cause of the EPT violation, i.e. the PTE mapping |gpa| is
//          present, but the access violates the permissions it allows.
// -ENOMEM: Memory allocation/mapping error.
int ept_handle_access_violation(hax_gpa_space *gpa_space, hax_ept_tree *tree,
                                exit_qualification_t qual, uint64_t gpa,
                                uint64_t *fault_gfn);

// Handles an EPT misconfiguration caught by hardware while it tries to
// translate a GPA.
// |gpa_space|: The |hax_gpa_space| of the guest.
// |tree|: The |hax_ept_tree| of the guest.
// |gpa|: The GPA being translated.
// Returns the number of misconfigured |hax_epte|s that have been identified and
// fixed, or a negative number if any misconfigured |hax_epte| cannot be fixed.
int ept_handle_misconfiguration(hax_gpa_space *gpa_space, hax_ept_tree *tree,
                                uint64_t gpa);

#endif  // HAX_CORE_EPT2_H_
