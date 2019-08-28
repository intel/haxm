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

#include "include/ept2.h"

#include "../include/hax.h"
#include "include/paging.h"
#include "../include/hax_host_mem.h"

void ept_handle_mapping_removed(hax_gpa_space_listener *listener,
                                uint64_t start_gfn, uint64_t npages, uint64_t uva,
                                uint8_t flags)
{
    bool is_rom = flags & HAX_MEMSLOT_READONLY;
    hax_ept_tree *tree;
    int ret;

    hax_log(HAX_LOGI, "%s: %s=>MMIO: start_gfn=0x%llx, npages=0x%llx, "
            "uva=0x%llx\n", __func__, is_rom ? "ROM" : "RAM", start_gfn,
            npages, uva);
    hax_assert(listener != NULL);
    tree = (hax_ept_tree *) listener->opaque;
    ret = ept_tree_invalidate_entries(tree, start_gfn, npages);
    hax_log(HAX_LOGI, "%s: Invalidated %d PTEs\n", __func__, ret);
}

void ept_handle_mapping_changed(hax_gpa_space_listener *listener,
                                uint64_t start_gfn, uint64_t npages,
                                uint64_t old_uva, uint8_t old_flags,
                                uint64_t new_uva, uint8_t new_flags)
{
    bool was_rom = old_flags & HAX_MEMSLOT_READONLY;
    bool is_rom = new_flags & HAX_MEMSLOT_READONLY;
    hax_ept_tree *tree;
    int ret;

    hax_log(HAX_LOGI, "%s: %s=>%s: start_gfn=0x%llx, npages=0x%llx, "
            "old_uva=0x%llx, new_uva=0x%llx\n", __func__,
            was_rom ? "ROM" : "RAM", is_rom ? "ROM" : "RAM", start_gfn,
            npages, old_uva, new_uva);
    hax_assert(listener != NULL);
    tree = (hax_ept_tree *) listener->opaque;
    ret = ept_tree_invalidate_entries(tree, start_gfn, npages);
    hax_log(HAX_LOGI, "%s: Invalidated %d PTEs\n", __func__, ret);
}

int ept_handle_access_violation(hax_gpa_space *gpa_space, hax_ept_tree *tree,
                                exit_qualification_t qual, uint64_t gpa,
                                uint64_t *fault_gfn)
{
    uint combined_perm;
    uint64_t gfn;
    hax_memslot *slot;
    bool is_rom;
    hax_ramblock *block;
    hax_chunk *chunk;
    uint64_t offset_within_slot, offset_within_block, offset_within_chunk;
    uint64_t chunk_offset_low, chunk_offset_high, slot_offset_high;
    uint64_t start_gpa, size;
    int ret;

    gfn = gpa >> PG_ORDER_4K;
    hax_assert(gpa_space != NULL);
    slot = memslot_find(gpa_space, gfn);
    if (!slot) {
        // The faulting GPA is reserved for MMIO
        hax_log(HAX_LOGD, "%s: gpa=0x%llx is reserved for MMIO\n",
                __func__, gpa);
        return 0;
    }

    // Extract bits 5..3 from Exit Qualification
    combined_perm = (uint) ((qual.raw >> 3) & 7);
    if (combined_perm != HAX_EPT_PERM_NONE) {
        if ((qual.raw & HAX_EPT_ACC_W) && !(combined_perm & HAX_EPT_PERM_W) &&
            (slot->flags == HAX_MEMSLOT_READONLY)) {
            // Handle a write to ROM/ROM device as MMIO
            hax_log(HAX_LOGD, "%s: write to a read-only gpa=0x%llx\n",
                    __func__, gpa);
            return 0;
        }
        // See IA SDM Vol. 3C 27.2.1 Table 27-7, especially note 2
        hax_log(HAX_LOGE, "%s: Cannot handle the case where the PTE "
                "corresponding to the faulting GPA is present: qual=0x%llx, "
                "gpa=0x%llx\n", __func__, qual.raw, gpa);
        return -EACCES;
    }

    // Ideally we should call gpa_space_is_page_protected() and ask user space
    // to unprotect just the host virtual page that |gfn| maps to. But since we
    // pin host RAM one chunk (rather than one page) at a time, if the chunk
    // that |gfn| maps to contains any other host virtual page that is protected
    // (by means of a VirtualProtect() or mprotect() call from user space), we
    // will not be able to pin the chunk when we handle the next EPT violation
    // caused by the same |gfn|.
    // For now, we ask user space to unprotect all host virtual pages in the
    // chunk, so our next hax_pin_user_pages() call will not fail. This is a
    // dirty hack.
    // TODO: Make chunks more flexible, so we can pin host RAM in finer
    // granularity (as small as one page) and hide chunks from user space.
    if (gpa_space_is_chunk_protected(gpa_space, gfn, fault_gfn))
        return -EFAULT;

    // The faulting GPA maps to RAM/ROM
    is_rom = slot->flags & HAX_MEMSLOT_READONLY;
    offset_within_slot = gpa - (slot->base_gfn << PG_ORDER_4K);
    hax_assert(offset_within_slot < (slot->npages << PG_ORDER_4K));
    block = slot->block;
    hax_assert(block != NULL);
    offset_within_block = slot->offset_within_block + offset_within_slot;
    hax_assert(offset_within_block < block->size);
    chunk = ramblock_get_chunk(block, offset_within_block, true);
    if (!chunk) {
        hax_log(HAX_LOGE, "%s: Failed to grab the RAM chunk for %s gpa=0x%llx:"
                " slot.base_gfn=0x%llx, slot.offset_within_block=0x%llx,"
                " offset_within_slot=0x%llx, block.base_uva=0x%llx,"
                " block.size=0x%llx\n", __func__, is_rom ? "ROM" : "RAM", gpa,
                slot->base_gfn, slot->offset_within_block, offset_within_slot,
                block->base_uva, block->size);
        return -ENOMEM;
    }

    // Compute the union of the UVA ranges covered by |slot| and |chunk|
    chunk_offset_low = chunk->base_uva - block->base_uva;
    start_gpa = slot->base_gfn << PG_ORDER_4K;
    if (chunk_offset_low > slot->offset_within_block) {
        start_gpa += chunk_offset_low - slot->offset_within_block;
        offset_within_chunk = 0;
    } else {
        offset_within_chunk = slot->offset_within_block - chunk_offset_low;
    }
    chunk_offset_high = chunk_offset_low + chunk->size;
    slot_offset_high = slot->offset_within_block +
                       (slot->npages << PG_ORDER_4K);
    size = chunk->size - offset_within_chunk;
    if (chunk_offset_high > slot_offset_high) {
        size -= chunk_offset_high - slot_offset_high;
    }
    ret = ept_tree_create_entries(tree, start_gpa >> PG_ORDER_4K,
                                  size >> PG_ORDER_4K, chunk,
                                  offset_within_chunk, slot->flags);
    if (ret < 0) {
        hax_log(HAX_LOGE, "%s: Failed to create PTEs for GFN range: ret=%d, "
                "gpa=0x%llx, start_gfn=0x%llx, npages=%llu\n", __func__, ret,
                gpa, start_gpa >> PG_ORDER_4K, size >> PG_ORDER_4K);
        return ret;
    }
    hax_log(HAX_LOGD, "%s: Created %d PTEs for GFN range: gpa=0x%llx, "
            "start_gfn=0x%llx, npages=%llu\n", __func__, ret, gpa,
            start_gpa >> PG_ORDER_4K, size >> PG_ORDER_4K);
    return 1;
}

typedef struct epte_fixer_bundle {
    hax_memslot *slot;
    int misconfigured_count;
    int error_count;
} epte_fixer_bundle;

static void fix_epte(hax_ept_tree *tree, uint64_t gfn, int level, hax_epte *epte,
                     void *opaque)
{
    hax_epte old_epte, new_epte;
    epte_fixer_bundle *bundle;

    hax_assert(epte != NULL);
    old_epte = *epte;
    new_epte = old_epte;
    hax_assert(opaque != NULL);
    bundle = (epte_fixer_bundle *) opaque;

    if (old_epte.perm == HAX_EPT_PERM_NONE) {
        // Entries that are not present are never checked by hardware for
        // misconfigurations
        if (old_epte.value) {
            hax_log(HAX_LOGW, "%s: Entry is not present but some bits are set:"
                    " value=0x%llx, level=%d, gfn=0x%llx\n", __func__,
                    old_epte.value, level, gfn);
        }
        return;
    }

    if (level == HAX_EPT_LEVEL_PT && !bundle->slot) {
        // The GFN is reserved for MMIO, so the EPT leaf entry that maps it
        // should be zeroed out (i.e. not present)
        new_epte.value = 0;
    } else {
        uint64_t w_bit = HAX_EPT_PERM_RWX ^ HAX_EPT_PERM_RX;
        uint64_t preserved_bits;

        // Set bits 2..0 (permissions)
        new_epte.value |= HAX_EPT_PERM_RWX;
        if (level == HAX_EPT_LEVEL_PT) {
            if (bundle->slot &&
                (bundle->slot->flags & HAX_MEMSLOT_READONLY)) {
                // Clear bit 1 (Writable)
                new_epte.value &= ~w_bit;
            }
            // Set bits 5..3 (EPT MT) to 6 (WB)
            new_epte.value |= HAX_EPT_MEMTYPE_WB << 3;
        }

        // Preserve bits 2..0 (permissions)
        preserved_bits = HAX_EPT_PERM_RWX;
        // Preserve bits (MAXPHYADDR - 1)..12 (PFN), assuming MAXPHYADDR == 36,
        // i.e. HPAs are at most 36 bits long
        // TODO: Use CPUID to obtain the true MAXPHYADDR
        preserved_bits |= ((1ULL << (36 - 12)) - 1) << 12;
        // Preserve bit 8 (Accessed)
        preserved_bits |= 1 << 8;
        if (level == HAX_EPT_LEVEL_PT) {
            // Preserve bits 5..3 (EPT MT)
            preserved_bits |= 0x7 << 3;
        }

        // Clear all reserved bits
        new_epte.value &= preserved_bits;
    }
    hax_log(HAX_LOGW, "%s: gfn=0x%llx, level=%d, value=0x%llx, "
            "new_value=0x%llx\n", __func__, gfn, level, old_epte.value,
            new_epte.value);
    if (epte->value != new_epte.value) {
        bundle->misconfigured_count++;
        if (epte->pfn != new_epte.pfn) {
            // An invalid PFN cannot be fixed easily
            bundle->error_count++;
        } else if (!hax_cmpxchg64(old_epte.value, new_epte.value,
                                  &epte->value)) {
            // *epte != old_epte, probably because another thread has changed
            // this EPT entry, so just assume the entry has been fixed
            hax_log(HAX_LOGW, "%s: Entry has changed: current_value=0x%llx\n",
                    __func__, epte->value);
        }
    }
}

// EPT misconfigurations should never happen, because we do not misconfigure
// any EPT entry on purpose. However, they do happen, at least for one user
// (ASUS N550JK laptop, Core i7-4700HQ, Windows 10):
//  https://issuetracker.google.com/issues/66854191
// Before root-causing the bug, the best we can do is try to identify and
// fix any misconfigured EPT entries in the EPT misconfiguration handler, and
// log more information for further debugging.
int ept_handle_misconfiguration(hax_gpa_space *gpa_space, hax_ept_tree *tree,
                                uint64_t gpa)
{
    uint64_t gfn;
    epte_fixer_bundle bundle = { NULL, 0, 0 };

    gfn = gpa >> PG_ORDER_4K;
    hax_assert(gpa_space != NULL);
    bundle.slot = memslot_find(gpa_space, gfn);
    if (!bundle.slot) {
        // The GPA being accessed is reserved for MMIO
        hax_log(HAX_LOGW, "%s: gpa=0x%llx is reserved for MMIO\n",
                __func__, gpa);
    }

    ept_tree_walk(tree, gfn, fix_epte, &bundle);
    if (bundle.error_count) {
        hax_log(HAX_LOGE, "%s: Failed to fix %d/%d misconfigured entries for"
                " gpa=0x%llx\n", __func__, bundle.error_count,
                bundle.misconfigured_count, gpa);
        return -bundle.error_count;
    }
    hax_log(HAX_LOGW, "%s: Fixed %d misconfigured entries for gpa=0x%llx\n",
            __func__, bundle.misconfigured_count, gpa);
    return bundle.misconfigured_count;
}
