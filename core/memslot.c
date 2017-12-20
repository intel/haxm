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

#include "../include/hax.h"
#include "include/memory.h"
#include "include/paging.h"

#define MEMSLOT_PROCESSING 0x01
#define MEMSLOT_TO_INSERT  0x02

#define MEMSLOT_ALLOC(m) if (((m) = memslot_alloc(m)) == NULL) return -ENOMEM
#define SAFE_CALL(f) if ((f) != NULL) (f)

enum callback {
    MAPPING_ADDED = 1,
    MAPPING_REMOVED,
    MAPPING_CHANGED
};

enum route {
    MAPPING_DIFF_TYPE,
    MAPPING_SAME_TYPE,
    MAPPING_INVALID
};

typedef int (*MEMSLOT_PROCESS)(hax_memslot *, hax_memslot *, uint8 *);

typedef struct memslot_mapping {
    uint8 callback;
    uint64 start_gfn;
    uint64 npages;
    uint64 old_uva;
    uint8 old_flags;
    uint64 new_uva;
    uint8 new_flags;
} memslot_mapping;

static hax_memslot * memslot_alloc(hax_memslot *dest);
static void memslot_free(void **memslot, uint32 size);
static void memslot_copy(hax_memslot *dest, hax_memslot *src);
static void memslot_insert_before(hax_memslot *dest, hax_memslot *src);
static void memslot_insert_after(hax_memslot *dest, hax_memslot *src);
static void memslot_insert_head(hax_memslot *dest, hax_gpa_space *gpa_space);
static void memslot_delete(hax_memslot *dest);
static void memslot_union(hax_memslot *dest, hax_memslot *src);
static void memslot_overlap_front(hax_memslot *dest, hax_memslot *src);
static void memslot_overlap_rear(hax_memslot *dest, hax_memslot *src);
static hax_memslot * memslot_append_rest(hax_memslot *dest, hax_memslot *src);
static bool memslot_is_valid(uint8 flags);
static bool memslot_is_same_type(hax_memslot *dest, hax_memslot *src);
static bool memslot_is_inner(hax_memslot *dest, hax_memslot *src,
                             hax_gpa_space *gpa_space);
static int memslot_process_start_diff_type(hax_memslot *dest, hax_memslot *src,
                                           uint8 *state);
static int memslot_process_start_same_type(hax_memslot *dest, hax_memslot *src,
                                           uint8 *state);
static int memslot_process_start_invalid(hax_memslot *dest, hax_memslot *src,
                                         uint8 *state);
static int memslot_process_end_diff_type(hax_memslot *dest, hax_memslot *src,
                                         uint8 *state);
static int memslot_process_end_same_type(hax_memslot *dest, hax_memslot *src,
                                         uint8 *state);
static int memslot_process_end_invalid(hax_memslot *dest, hax_memslot *src,
                                       uint8 *state);
static void mapping_broadcast(hax_list_head *listener_list,
                              memslot_mapping *mapping, hax_memslot *dest,
                              hax_list_head *memslot_list);
static void mapping_calc_change(memslot_mapping *mapping, hax_memslot *src,
                                bool is_valid, bool is_terminal,
                                bool is_changed, memslot_mapping *hole,
                                memslot_mapping *slot);
static void mapping_enqueue(hax_list_head *memslot_list, hax_memslot *dest);
static void mapping_intersect(memslot_mapping *dest, memslot_mapping *src);
static bool mapping_is_changed(hax_memslot *dest, hax_memslot *src,
                               bool is_valid, bool is_terminal);
static void mapping_notify_listeners(hax_list_head *listener_list,
                                     memslot_mapping *hole,
                                     memslot_mapping *slot);

static MEMSLOT_PROCESS memslot_process_start[] = {
    memslot_process_start_diff_type,
    memslot_process_start_same_type,
    memslot_process_start_invalid
};

static MEMSLOT_PROCESS memslot_process_end[] = {
    memslot_process_end_diff_type,
    memslot_process_end_same_type,
    memslot_process_end_invalid
};

int memslot_init_list(hax_gpa_space *gpa_space)
{
    if (gpa_space == NULL)
        return -EINVAL;

    hax_init_list_head(&gpa_space->memslot_list);

    return 0;
}

void memslot_free_list(hax_gpa_space *gpa_space)
{
    hax_memslot *memslot = NULL, *n = NULL;

    if (gpa_space == NULL)
        return;

    hax_list_entry_for_each_safe(memslot, n, &gpa_space->memslot_list,
                                 hax_memslot, entry) {
        memslot_delete(memslot);
    }
}

void memslot_dump_list(hax_gpa_space *gpa_space)
{
    hax_memslot *memslot = NULL;
    int i = 0;

    hax_info("memslot dump begins:\n");
    hax_list_entry_for_each(memslot, &gpa_space->memslot_list, hax_memslot,
                            entry) {
        hax_info("memory slot [%d]: base_gfn = 0x%016llx, npages = 0x%llx, "
                 "uva = 0x%016llx, flags = 0x%02x "
                 "(block_base_uva = 0x%016llx, offset_within_block = 0x%llx)\n",
                 i++, memslot->base_gfn, memslot->npages,
                 memslot->block->base_uva + memslot->offset_within_block,
                 memslot->flags, memslot->block->base_uva,
                 memslot->offset_within_block);
    }
    hax_info("memslot dump ends!\n");
}

int memslot_set_mapping(hax_gpa_space *gpa_space, uint64 start_gfn,
                        uint64 npages, uint64 uva, uint8 flags)
{
    hax_memslot memslot, *src = NULL, *dest = &memslot, *n = NULL;
    hax_ramblock *block = NULL;
    memslot_mapping mapping;
    hax_list_head memslot_list;
    int ret = 0;
    bool is_valid = false, is_found = false;
    uint8 route = 0, state = 0;

    hax_info("%s: start_gfn=0x%llx, npages=0x%llx, uva=0x%llx, flags=0x%x\n",
             __func__, start_gfn, npages, uva, flags);

    if ((gpa_space == NULL) || (npages == 0))
        return -EINVAL;

    is_valid = memslot_is_valid(flags);
    if (is_valid) {
        block = ramblock_find(&gpa_space->ramblock_list, uva, NULL);

        if (block == NULL) {
            hax_error("%s: Failed to find uva=0x%llx in RAM block\n", __func__,
                      uva);
            return -EINVAL;
        }
    }

    mapping.start_gfn = start_gfn;
    mapping.npages    = npages;
    mapping.new_uva   = uva;
    mapping.new_flags = flags;

    dest->base_gfn = start_gfn;
    dest->npages   = npages;
    dest->block    = block;
    dest->offset_within_block = (dest->block != NULL)
                                ? uva - dest->block->base_uva : 0;
    dest->flags    = flags;

    if (hax_list_empty(&gpa_space->memslot_list)) {
        if (is_valid) {
            MEMSLOT_ALLOC(dest);
            memslot_insert_head(dest, gpa_space);

            mapping_broadcast(&gpa_space->listener_list, &mapping, dest, NULL);
        }

        return 0;
    }

    hax_init_list_head(&memslot_list);

    hax_list_entry_for_each_safe(src, n, &gpa_space->memslot_list, hax_memslot,
                                 entry) {
        if (!is_found) {
            if (dest->base_gfn > src->base_gfn + src->npages) {
                if (src->entry.next != &gpa_space->memslot_list)
                    continue;

                if (is_valid) {
                    MEMSLOT_ALLOC(dest);
                    memslot_insert_after(dest, src);
                }
                break;
            }

            if (dest->base_gfn < src->base_gfn + src->npages) {
                mapping_enqueue(&memslot_list, src);
            }

            if (dest->base_gfn + dest->npages >= src->base_gfn + src->npages) {
                state |= MEMSLOT_PROCESSING;
            }

            route = is_valid ? memslot_is_same_type(dest, src)
                    : MAPPING_INVALID;
            ret = memslot_process_start[route](dest, src, &state);
            if (ret != 0)
                return ret;

            is_found = true;
        } else {
            if (dest->base_gfn + dest->npages < src->base_gfn)
                break;

            if (dest->base_gfn + dest->npages > src->base_gfn) {
                mapping_enqueue(&memslot_list, src);
            }

            if (memslot_is_inner(dest, src, gpa_space)) {
                memslot_delete(src);
                continue;
            }

            route = is_valid ? memslot_is_same_type(dest, src)
                    : MAPPING_INVALID;
            ret = memslot_process_end[route](dest, src, &state);
            if (ret != 0)
                return ret;

            state = 0;
        }

        if (!(state & MEMSLOT_PROCESSING))
            break;
    }

    if (state & MEMSLOT_TO_INSERT) {
        MEMSLOT_ALLOC(dest);
        memslot_insert_before(dest, src);
    }

    mapping_broadcast(&gpa_space->listener_list, &mapping, dest,
                      &memslot_list);

    return 0;
}

hax_memslot * memslot_find(hax_gpa_space *gpa_space, uint64 gfn)
{
    hax_memslot *memslot = NULL;

    if (gpa_space == NULL)
        return NULL;

    hax_list_entry_for_each(memslot, &gpa_space->memslot_list, hax_memslot,
                            entry) {
        if (memslot->base_gfn > gfn)
            break;

        if (gfn < memslot->base_gfn + memslot->npages)
            return memslot;
    }

    return NULL;
}

static inline hax_memslot * memslot_alloc(hax_memslot *memslot)
{
    hax_memslot *ptr = NULL;

    ptr = (hax_memslot *)hax_vmalloc(sizeof(hax_memslot), HAX_MEM_NONPAGE);

    if (ptr == NULL)
        return NULL;

    ptr->base_gfn            = memslot->base_gfn;
    ptr->npages              = memslot->npages;
    ptr->block               = memslot->block;
    ptr->offset_within_block = memslot->offset_within_block;
    ptr->flags               = memslot->flags;
    hax_init_list_head(&ptr->entry);

    return ptr;
}

static inline void memslot_free(void **va, uint32 size)
{
    if ((va == NULL) || (*va == NULL))
        return;

    hax_vfree(*va, size);
    *va = NULL;
}

static inline void memslot_copy(hax_memslot *dest, hax_memslot *src)
{
    dest->base_gfn            = src->base_gfn;
    dest->npages              = src->npages;
    dest->block               = src->block;
    dest->offset_within_block = src->offset_within_block;
    dest->flags               = src->flags;
}

static inline void memslot_insert_before(hax_memslot *dest, hax_memslot *src)
{
    hax_list_insert_before(&dest->entry, &src->entry);
}

static inline void memslot_insert_after(hax_memslot *dest, hax_memslot *src)
{
    hax_list_insert_after(&dest->entry, &src->entry);
}

static inline void memslot_insert_head(hax_memslot *dest,
                                       hax_gpa_space *gpa_space)
{
    hax_list_insert_after(&dest->entry, &gpa_space->memslot_list);
}

static inline void memslot_delete(hax_memslot *dest)
{
    hax_list_del(&dest->entry);
    memslot_free((void **)&dest, sizeof(hax_memslot));
}

static inline void memslot_union(hax_memslot *dest, hax_memslot *src)
{
    src->offset_within_block = min(src->offset_within_block,
                                   dest->offset_within_block);
    src->npages = max(src->base_gfn + src->npages,
                      dest->base_gfn + dest->npages)
                  - min(src->base_gfn, dest->base_gfn);
    src->base_gfn = min(src->base_gfn, dest->base_gfn);
}

static inline void memslot_overlap_front(hax_memslot *dest, hax_memslot *src)
{
    src->offset_within_block += (dest->base_gfn + dest->npages - src->base_gfn)
                                << PG_ORDER_4K;
    src->npages = src->base_gfn + src->npages - dest->base_gfn - dest->npages;
    src->base_gfn = dest->base_gfn + dest->npages;
}

static inline void memslot_overlap_rear(hax_memslot *dest, hax_memslot *src)
{
    src->npages = dest->base_gfn - src->base_gfn;
}

static inline hax_memslot * memslot_append_rest(hax_memslot *dest,
                                                hax_memslot *src)
{
    hax_memslot *rest = NULL;

    rest = (hax_memslot *)hax_vmalloc(sizeof(hax_memslot), HAX_MEM_NONPAGE);

    if (rest == NULL)
        return NULL;

    rest->base_gfn            = dest->base_gfn + dest->npages;
    rest->npages              = src->base_gfn + src->npages - dest->base_gfn
                                - dest->npages;
    rest->block               = src->block;
    rest->offset_within_block = src->offset_within_block + ((dest->base_gfn
                                + dest->npages - src->base_gfn) << PG_ORDER_4K);
    rest->flags               = src->flags;
    hax_init_list_head(&rest->entry);

    return rest;
}

static inline bool memslot_is_valid(uint8 flags)
{
    return (flags & HAX_MEMSLOT_INVALID) != HAX_MEMSLOT_INVALID;
}

static inline bool memslot_is_same_type(hax_memslot *dest, hax_memslot *src)
{
    return (dest->block == src->block) &&
           (dest->offset_within_block - src->offset_within_block ==
            (dest->base_gfn - src->base_gfn) << PG_ORDER_4K) &&
           ((dest->flags & HAX_MEMSLOT_READONLY) ==
            (src->flags & HAX_MEMSLOT_READONLY));
}

static bool memslot_is_inner(hax_memslot *dest, hax_memslot *src,
                             hax_gpa_space *gpa_space)
{
    hax_memslot *next = NULL;

    return (dest->base_gfn + dest->npages > src->base_gfn + src->npages) ||
           ((dest->base_gfn + dest->npages == src->base_gfn + src->npages) &&
            (src->entry.next != &gpa_space->memslot_list) &&
            (next = hax_list_entry(entry, hax_memslot, src->entry.next),
             (dest->base_gfn + dest->npages == next->base_gfn)) &&
            memslot_is_same_type(dest, next));
}

// =================================================
//
//               |_______|
//               |_______|
//       ____    |       |
// (1)  |____|   |       |
//       ________|       |
// (2)  |________|       |
//       ____________    |
// (3)  |____________|   |
//       ________________|
// (4)  |________________|              ---->
//       ____________________
// (5)  |____________________|          ---->
//               |___    |
// (6)           |___|   |
//               |_______|
// (7)           |_______|              ---->
//               |___________
// (8)           |___________|          ---->
//               |   __  |
// (9)           |  |__| |
//               |   ____|
// (10)          |  |____|              ---->
//               |   ________
// (11)          |  |________|          ---->
//               |       |________
// (12)          |       |________|     ---->
//               |       |    ________
// (13)          |       |   |________|
//               |       |
//
// Figure 1: Memory slot process start (primary node)

static int memslot_process_start_diff_type(hax_memslot *dest, hax_memslot *src,
                                           uint8 *state)
{
    if (dest->base_gfn + dest->npages <= src->base_gfn) {
        // (1)(2)
        MEMSLOT_ALLOC(dest);
        memslot_insert_before(dest, src);
    } else if (dest->base_gfn <= src->base_gfn) {
        // (3)(4)(5)(6)(7)(8)
        if (dest->base_gfn + dest->npages < src->base_gfn + src->npages) {
            // (3)(6)
            MEMSLOT_ALLOC(dest);
            memslot_insert_before(dest, src);
            memslot_overlap_front(dest, src);
        } else {
            // (4)(5)(7)(8)
            memslot_union(dest, src);
            src->flags = dest->flags;
        }
    } else {
        // (9)(10)(11)(12)
        if (dest->base_gfn + dest->npages >= src->base_gfn + src->npages) {
            // (10)(11)(12)
            *state |= MEMSLOT_TO_INSERT;
        }
        if (dest->base_gfn < src->base_gfn + src->npages) {
            // (9)(10)(11)
            if (dest->base_gfn + dest->npages < src->base_gfn + src->npages) {
                // (9)
                MEMSLOT_ALLOC(dest);
                memslot_insert_after(dest, src);
                memslot_insert_after(memslot_append_rest(dest, src), dest);
            }
            memslot_overlap_rear(dest, src);
        }
    }

    return 0;
}

static int memslot_process_start_same_type(hax_memslot *dest, hax_memslot *src,
                                           uint8 *state)
{
    if (dest->base_gfn + dest->npages < src->base_gfn) {
        // (1)
        MEMSLOT_ALLOC(dest);
        memslot_insert_before(dest, src);
        return 0;
    }
    if ((dest->base_gfn >= src->base_gfn) &&
        (dest->base_gfn + dest->npages <= src->base_gfn + src->npages))
        // (6)(7)(9)(10)
        return 0;

    // (2)(3)(4)(5)(8)(11)(12)
    memslot_union(dest, src);

    return 0;
}

static int memslot_process_start_invalid(hax_memslot *dest, hax_memslot *src,
                                         uint8 *state)
{
    if ((dest->base_gfn + dest->npages <= src->base_gfn) ||
        (dest->base_gfn == src->base_gfn + src->npages))
        // (1)(2)(12)
        return 0;

    if (dest->base_gfn <= src->base_gfn) {
        // (3)(4)(5)(6)(7)(8)
        if (dest->base_gfn + dest->npages < src->base_gfn + src->npages) {
            // (3)(6)
            memslot_overlap_front(dest, src);
        } else {
            // (4)(5)(7)(8)
            memslot_delete(src);
        }
    } else {
        // (9)(10)(11)
        if (dest->base_gfn + dest->npages < src->base_gfn + src->npages) {
            // (9)
            memslot_insert_after(memslot_append_rest(dest, src), src);
        }
        memslot_overlap_rear(dest, src);
    }

    return 0;
}

// =================================================
//
//                                E       E'
//               |_______|    |_______|_______
//               |_______|    |_______|_______|
//               |       |    |       |
//       _____________________|       |
// [1]  |_____________________|       |
//               |       |    |       |
//       _________________________    |
// [2]  |_________________________|   |
//               |       |    |       |
//       _____________________________|
// [3]  |_____________________________|
//               |       |    |       |
//
// Figure 2: Memory slot process end (last node)
// ________
//
// * When mapping [3] has the same type with slot E' (if existing), the last
//   node should be the slot E', rather than E (see algorithm implemented in
//   function memslot_is_inner).

static int memslot_process_end_diff_type(hax_memslot *dest, hax_memslot *src,
                                         uint8 *state)
{
    if (dest->base_gfn + dest->npages < src->base_gfn + src->npages) {
        // [1][2]
        if (dest->base_gfn + dest->npages > src->base_gfn) {
            // [2]
            memslot_overlap_front(dest, src);
        }
        if (*state & MEMSLOT_TO_INSERT) {
            MEMSLOT_ALLOC(dest);
            memslot_insert_before(dest, src);
        }
    } else {
        // [3]
        if (*state & MEMSLOT_TO_INSERT) {
            memslot_copy(src, dest);
        } else {
            memslot_delete(src);
        }
    }

    return 0;
}

static int memslot_process_end_same_type(hax_memslot *dest, hax_memslot *src,
                                         uint8 *state)
{
    hax_memslot *prev = NULL;

    if ((dest->base_gfn + dest->npages == src->base_gfn + src->npages) &&
            !(*state & MEMSLOT_TO_INSERT)) {
        // [3] && Existed
        memslot_delete(src);
        return 0;
    }

    // [1][2] || ([3] && TO_INSERT)
    prev = hax_list_entry(entry, hax_memslot, src->entry.prev);
    memslot_union(memslot_is_same_type(dest, prev) ? prev : dest, src);

    if (!(*state & MEMSLOT_TO_INSERT)) {
        memslot_delete(prev);
    }

    return 0;
}

static int memslot_process_end_invalid(hax_memslot *dest, hax_memslot *src,
                                       uint8 *state)
{
    if (dest->base_gfn + dest->npages == src->base_gfn)
        // [1]
        return 0;

    // [2][3]
    if (dest->base_gfn + dest->npages < src->base_gfn + src->npages) {
        // [2]
        memslot_overlap_front(dest, src);
    } else {
        // [3]
        memslot_delete(src);
    }

    return 0;
}

static void mapping_broadcast(hax_list_head *listener_list,
                              memslot_mapping *mapping, hax_memslot *dest,
                              hax_list_head *memslot_list)
{
    hax_memslot *src = NULL, *m = NULL, *begin = NULL, *end = NULL;
    memslot_mapping hole, slot;
    bool is_valid = false, is_terminal = false, is_changed = false;

    if (hax_list_empty(listener_list))
        return;

    if (hax_list_empty(memslot_list)) {
        mapping->callback = MAPPING_ADDED;
        mapping_notify_listeners(listener_list, mapping, NULL);
        return;
    }

    is_valid = memslot_is_valid(mapping->new_flags);
    begin = hax_list_entry(entry, hax_memslot, memslot_list->next);
    end = hax_list_entry(entry, hax_memslot, memslot_list->prev);

    hole.start_gfn = mapping->start_gfn;

    hax_list_entry_for_each_safe(src, m, memslot_list, hax_memslot, entry) {
        is_terminal = ((src == begin) || (src == end));
        is_changed = mapping_is_changed(dest, src, is_valid, is_terminal);

        mapping_calc_change(mapping, src, is_valid, is_terminal, is_changed,
                            &hole, &slot);
        mapping_notify_listeners(listener_list, &hole, &slot);
        hole.start_gfn = src->base_gfn + src->npages;

        memslot_delete(src);
    }

    if (is_valid && (hole.start_gfn < mapping->start_gfn + mapping->npages)) {
        hole.callback  = MAPPING_ADDED;
        hole.npages    = mapping->start_gfn + mapping->npages - hole.start_gfn;
        hole.new_uva   = mapping->new_uva + ((hole.start_gfn
                         - mapping->start_gfn) << PG_ORDER_4K);
        hole.new_flags = mapping->new_flags;

        mapping_notify_listeners(listener_list, &hole, NULL);
    }
}

static void mapping_calc_change(memslot_mapping *mapping, hax_memslot *src,
                                bool is_valid, bool is_terminal,
                                bool is_changed, memslot_mapping *hole,
                                memslot_mapping *slot)
{
    if (is_valid && (src->base_gfn > hole->start_gfn)) {
        hole->callback  = MAPPING_ADDED;
        hole->npages    = src->base_gfn - hole->start_gfn;
        hole->new_uva   = mapping->new_uva;
        hole->new_flags = mapping->new_flags;

        if (is_terminal) {
            mapping_intersect(mapping, hole);
        }
    } else {
        hole->callback = 0;
    }

    if (is_changed) {
        slot->callback  = is_valid ? MAPPING_CHANGED : MAPPING_REMOVED;
        slot->start_gfn = src->base_gfn;
        slot->npages    = src->npages;
        slot->old_uva   = src->block->base_uva + src->offset_within_block
                          + ((mapping->start_gfn > src->base_gfn)
                          ? (mapping->start_gfn - src->base_gfn) << PG_ORDER_4K
                          : 0);
        slot->old_flags = src->flags;

        if (is_valid) {
            slot->new_uva   = mapping->new_uva
                              + ((mapping->start_gfn < src->base_gfn)
                              ? (src->base_gfn - mapping->start_gfn)
                              << PG_ORDER_4K : 0);
            slot->new_flags = mapping->new_flags;
        }
        if (is_terminal) {
            mapping_intersect(mapping, slot);
        }
    } else {
        slot->callback = 0;
    }
}

static inline void mapping_enqueue(hax_list_head *memslot_list,
                                   hax_memslot *dest)
{
    hax_memslot *ptr = NULL;

    if (dest == NULL)
        return;

    ptr = (hax_memslot *)hax_vmalloc(sizeof(hax_memslot), HAX_MEM_NONPAGE);

    if (ptr == NULL)
        return;

    *ptr = *dest;
    hax_list_insert_before(&ptr->entry, memslot_list);
}

static inline void mapping_intersect(memslot_mapping *dest,
                                     memslot_mapping *src)
{
    src->npages = min(src->start_gfn + src->npages,
                      dest->start_gfn + dest->npages)
                  - max(src->start_gfn, dest->start_gfn);
    src->start_gfn = max(src->start_gfn, dest->start_gfn);
}

static inline bool mapping_is_changed(hax_memslot *dest, hax_memslot *src,
                                      bool is_valid, bool is_terminal)
{
    return ((is_valid && !memslot_is_same_type(dest, src)) || !is_valid) &&
           ((is_terminal && ((dest->base_gfn + dest->npages > src->base_gfn) &&
            (dest->base_gfn < src->base_gfn + src->npages))) || !is_terminal);
}

static void mapping_notify_listeners(hax_list_head *listener_list,
                                     memslot_mapping *hole,
                                     memslot_mapping *slot)
{
    hax_gpa_space_listener *listener = NULL;

    hax_list_entry_for_each(listener, listener_list, hax_gpa_space_listener,
                            entry) {
        if ((hole != NULL) && (hole->callback == MAPPING_ADDED)) {
            SAFE_CALL(listener->mapping_added)(listener,
                                               hole->start_gfn,
                                               hole->npages,
                                               hole->new_uva,
                                               hole->new_flags);
        }

        if (slot == NULL)
            continue;

        switch (slot->callback) {
            case MAPPING_REMOVED: {
                SAFE_CALL(listener->mapping_removed)(listener,
                                                     slot->start_gfn,
                                                     slot->npages,
                                                     slot->old_uva,
                                                     slot->old_flags);
                break;
            }
            case MAPPING_CHANGED: {
                SAFE_CALL(listener->mapping_changed)(listener,
                                                     slot->start_gfn,
                                                     slot->npages,
                                                     slot->old_uva,
                                                     slot->old_flags,
                                                     slot->new_uva,
                                                     slot->new_flags);
                break;
            }
            default: {
                break;
            }
        }
    }
}
