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

#include "include/memory.h"

#include "../include/hax.h"
#include "include/paging.h"
#include "../include/hax_host_mem.h"

int gpa_space_init(hax_gpa_space *gpa_space)
{
    int ret = 0;

    if (!gpa_space) {
        hax_log(HAX_LOGE, "gpa_space_init: param gpa_space is null!\n");
        return -EINVAL;
    }

    // Initialize ramblock list
    ret = ramblock_init_list(&gpa_space->ramblock_list);
    if (ret != 0)
        return ret;

    // Initialize memslot list
    ret = memslot_init_list(gpa_space);
    if (ret != 0)
        return ret;

    // Initialize listener list
    hax_init_list_head(&gpa_space->listener_list);

    return ret;
}

// Returns the protection bitmap size in bytes, or 0 on error.
static uint gpa_space_prot_bitmap_size(uint64_t npages)
{
    uint bitmap_size;

    if (npages >> 31) {
        // Require |npages| to be < 2^31, which is reasonable, because 2^31
        // pages implies a huge guest RAM size (8TB).
        return 0;
    }

    bitmap_size = ((uint)npages + 7) / 8;
    bitmap_size += 8;
    return bitmap_size;
}

void gpa_space_free(hax_gpa_space *gpa_space)
{
    hax_gpa_space_listener *listener, *tmp;
    if (!gpa_space) {
        hax_log(HAX_LOGE, "gpa_space_free: invalid param!\n");
        return;
    }

    memslot_free_list(gpa_space);
    ramblock_free_list(&gpa_space->ramblock_list);

    // Clear listener_list.
    hax_list_entry_for_each_safe(listener, tmp, &gpa_space->listener_list,
                                 hax_gpa_space_listener, entry) {
        hax_list_del(&listener->entry);
    }
    if (gpa_space->prot.bitmap)
        hax_vfree(gpa_space->prot.bitmap,
                  gpa_space_prot_bitmap_size(gpa_space->prot.end_gfn));
}

void gpa_space_add_listener(hax_gpa_space *gpa_space,
                            hax_gpa_space_listener *listener)
{
    if (!gpa_space || !listener) {
        hax_log(HAX_LOGE, "gpa_space_add_listener: invalid param!\n");
        return;
    }

    listener->gpa_space = gpa_space;
    hax_list_add(&listener->entry, &gpa_space->listener_list);
}

void gpa_space_remove_listener(hax_gpa_space *gpa_space,
                               hax_gpa_space_listener *listener)
{
    if (!gpa_space || !listener) {
        hax_log(HAX_LOGE, "gpa_space_remove_listener: invalid param!\n");
        return;
    }

    listener->gpa_space = NULL;
    hax_list_del(&listener->entry);
}

// Maps the given GPA range to KVA space, and returns the number of bytes
// actually mapped, which may be smaller than |len|, or a negative error code.
// If successful, the caller will be able to read the buffer starting at |*buf|
// (or write to it if |*writable| is true), with a size equal to the return
// value. When it is done with the buffer, it must destroy |kmap| by calling
// hax_unmap_user_pages().
static int gpa_space_map_range(hax_gpa_space *gpa_space, uint64_t start_gpa,
                               int len, uint8_t **buf, hax_kmap_user *kmap,
                               bool *writable)
{
    uint64_t gfn;
    uint delta, size, npages;
    hax_memslot *slot;
    hax_ramblock *block;
    uint64_t offset_within_block, offset_within_chunk;
    hax_chunk *chunk;
    void *kva;

    if (len < 0) {
        hax_log(HAX_LOGE, "%s: len=%d < 0\n", __func__, len);
        return -EINVAL;
    }
    if (!len) {
        // Assuming buf != NULL
        *buf = NULL;
        return 0;
    }

    gfn = start_gpa >> PG_ORDER_4K;
    delta = (uint) (start_gpa - (gfn << PG_ORDER_4K));
    size = (uint) len + delta;
    npages = (size + PAGE_SIZE_4K - 1) >> PG_ORDER_4K;
    slot = memslot_find(gpa_space, gfn);
    if (!slot) {
        hax_log(HAX_LOGE, "%s: start_gpa=0x%llx is reserved for MMIO\n",
                __func__, start_gpa);
        return -EINVAL;
    }
    if (writable) {
        *writable = !(slot->flags & HAX_MEMSLOT_READONLY);
    }
    if (gfn + npages > slot->base_gfn + slot->npages) {
        hax_log(HAX_LOGW, "%s: GPA range spans more than one memslot:"
                " start_gpa=0x%llx, len=%d, slot_base_gfn=0x%llx,"
                " slot_npages=%llu, gfn=0x%llx, npages=%u\n", __func__,
                start_gpa, len, slot->base_gfn, slot->npages, gfn, npages);
        npages = (uint) (slot->base_gfn + slot->npages - gfn);
        size = npages << PG_ORDER_4K;
    }

    block = slot->block;
    offset_within_block = ((gfn - slot->base_gfn) << PG_ORDER_4K) +
                          slot->offset_within_block;
    chunk = ramblock_get_chunk(block, offset_within_block, true);
    if (!chunk) {
        hax_log(HAX_LOGE, "%s: ramblock_get_chunk() failed: start_gpa=0x%llx\n",
                __func__, start_gpa);
        return -ENOMEM;
    }
    offset_within_chunk = offset_within_block - (chunk->base_uva -
                          block->base_uva);
    if (offset_within_chunk + size > chunk->size) {
        hax_log(HAX_LOGW, "%s: GPA range spans more than one chunk: "
                "start_gpa=0x%llx, len=%d, offset_within_chunk=0x%llx, "
                "size=0x%x, chunk_size=0x%llx\n", __func__, start_gpa, len,
                offset_within_chunk, size, chunk->size);
        size = (uint) (chunk->size - offset_within_chunk);
    }

    // Assuming kmap != NULL
    kva = hax_map_user_pages(&chunk->memdesc, offset_within_chunk, size, kmap);
    if (!kva) {
        hax_log(HAX_LOGE, "%s: hax_map_user_pages() failed: start_gpa=0x%llx,"
                " len=%d\n", __func__, start_gpa, len);
        return -ENOMEM;
    }
    // Assuming buf != NULL
    *buf = (uint8_t *) kva + delta;
    return (int) (size - delta);
}

int gpa_space_read_data(hax_gpa_space *gpa_space, uint64_t start_gpa, int len,
                        uint8_t *data)
{
    uint8_t *buf;
    hax_kmap_user kmap;
    int ret, nbytes;

    if (!data) {
        hax_log(HAX_LOGE, "%s: data == NULL\n", __func__);
        return -EINVAL;
    }

    ret = gpa_space_map_range(gpa_space, start_gpa, len, &buf, &kmap, NULL);
    if (ret < 0) {
        hax_log(HAX_LOGE, "%s: gpa_space_map_range() failed: start_gpa=0x%llx,"
                " len=%d\n", __func__, start_gpa, len);
        return ret;
    }

    nbytes = ret;
    if (nbytes < len) {
        hax_log(HAX_LOGW, "%s: Not enough bytes readable from guest RAM: "
                "nbytes=%d, start_gpa=0x%llx, len=%d\n", __func__, nbytes,
                start_gpa, len);
        if (!nbytes) {
            return 0;
        }
    }
    memcpy_s(data, nbytes, buf, nbytes);
    ret = hax_unmap_user_pages(&kmap);
    if (ret) {
        hax_log(HAX_LOGW, "%s: hax_unmap_user_pages() failed: ret=%d\n",
                __func__, ret);
        // This is not a fatal error, so ignore it
    }
    return nbytes;
}

int gpa_space_write_data(hax_gpa_space *gpa_space, uint64_t start_gpa, int len,
                         uint8_t *data)
{
    uint8_t *buf;
    hax_kmap_user kmap;
    bool writable = false;
    int ret, nbytes;

    if (!data) {
        hax_log(HAX_LOGE, "%s: data == NULL\n", __func__);
        return -EINVAL;
    }

    ret = gpa_space_map_range(gpa_space, start_gpa, len, &buf, &kmap,
                              &writable);
    if (ret < 0) {
        hax_log(HAX_LOGE, "%s: gpa_space_map_range() failed: start_gpa=0x%llx,"
                " len=%d\n", __func__, start_gpa, len);
        return ret;
    }
    if (!writable) {
        hax_log(HAX_LOGE, "%s: Cannot write to ROM: start_gpa=0x%llx, len=%d\n",
                __func__, start_gpa, len);
        return -EACCES;
    }

    nbytes = ret;
    if (nbytes < len) {
        hax_log(HAX_LOGW, "%s: Not enough bytes writable to guest RAM: "
                "nbytes=%d, start_gpa=0x%llx, len=%d\n", __func__, nbytes,
                start_gpa, len);
        if (!nbytes) {
            return 0;
        }
    }
    memcpy_s(buf, nbytes, data, nbytes);
    ret = hax_unmap_user_pages(&kmap);
    if (ret) {
        hax_log(HAX_LOGW, "%s: hax_unmap_user_pages() failed: ret=%d\n",
                __func__, ret);
        // This is not a fatal error, so ignore it
    }
    return nbytes;
}

void * gpa_space_map_page(hax_gpa_space *gpa_space, uint64_t gfn,
                          hax_kmap_user *kmap, bool *writable)
{
    uint8_t *buf;
    int ret;
    void *kva;

    hax_assert(gpa_space != NULL);
    hax_assert(kmap != NULL);
    ret = gpa_space_map_range(gpa_space, gfn << PG_ORDER_4K, PAGE_SIZE_4K, &buf,
                              kmap, writable);
    if (ret < PAGE_SIZE_4K) {
        hax_log(HAX_LOGE, "%s: gpa_space_map_range() returned %d\n",
                __func__, ret);
        return NULL;
    }
    kva = (void *) buf;
    hax_assert(kva != NULL);
    return kva;
}

void gpa_space_unmap_page(hax_gpa_space *gpa_space, hax_kmap_user *kmap)
{
    int ret;

    hax_assert(kmap != NULL);
    ret = hax_unmap_user_pages(kmap);
    if (ret) {
        hax_log(HAX_LOGW, "%s: hax_unmap_user_pages() returned %d\n",
                __func__, ret);
    }
}

uint64_t gpa_space_get_pfn(hax_gpa_space *gpa_space, uint64_t gfn, uint8_t *flags)
{
    hax_memslot *slot;
    hax_ramblock *block;
    hax_chunk *chunk;
    uint64_t pfn;
    uint64_t offset_within_slot, offset_within_block, offset_within_chunk;

    hax_assert(gpa_space != NULL);

    slot = memslot_find(gpa_space, gfn);
    if (!slot) {
        // The gfn is reserved for MMIO
        hax_log(HAX_LOGD, "%s: gfn=0x%llx is reserved for MMIO\n",
                __func__, gfn);
        if (flags) {
            *flags = HAX_MEMSLOT_INVALID;
        }
        return INVALID_PFN;
    }

    if (flags) {
        *flags = slot->flags;
    }

    offset_within_slot = (gfn - slot->base_gfn) << PG_ORDER_4K;
    hax_assert(offset_within_slot < (slot->npages << PG_ORDER_4K));
    block = slot->block;
    hax_assert(block != NULL);
    offset_within_block = slot->offset_within_block + offset_within_slot;
    hax_assert(offset_within_block < block->size);
    chunk = ramblock_get_chunk(block, offset_within_block, true);
    if (!chunk) {
        hax_log(HAX_LOGE, "%s: Failed to grab the RAM chunk for %s gfn=0x%llx:"
                " slot.base_gfn=0x%llx, slot.offset_within_block=0x%llx,"
                " offset_within_slot=0x%llx, block.base_uva=0x%llx,"
                " block.size=0x%llx\n", __func__,
                (slot->flags & HAX_MEMSLOT_READONLY) ? "ROM" : "RAM", gfn,
                slot->base_gfn, slot->offset_within_block, offset_within_slot,
                block->base_uva, block->size);
        return INVALID_PFN;
    }

    offset_within_chunk = offset_within_block -
                          (chunk->base_uva - block->base_uva);
    hax_assert(offset_within_chunk < chunk->size);
    pfn = hax_get_pfn_user(&chunk->memdesc, offset_within_chunk);
    hax_assert(pfn != INVALID_PFN);

    return pfn;
}

int gpa_space_adjust_prot_bitmap(hax_gpa_space *gpa_space, uint64_t end_gfn)
{
    hax_gpa_prot *pb = &gpa_space->prot;
    uint new_size;
    uint8_t *bmold = pb->bitmap, *bmnew = NULL;

    /* Bitmap size only grows until it is destroyed */
    if (end_gfn <= pb->end_gfn)
        return 0;

    hax_log(HAX_LOGI, "%s: end_gfn 0x%llx -> 0x%llx\n", __func__,
            pb->end_gfn, end_gfn);
    new_size = gpa_space_prot_bitmap_size(end_gfn);
    if (!new_size) {
        hax_log(HAX_LOGE, "%s: end_gfn=0x%llx is too big\n", __func__, end_gfn);
        return -EINVAL;
    }
    bmnew = hax_vmalloc(new_size, HAX_MEM_NONPAGE);
    if (!bmnew) {
        hax_log(HAX_LOGE, "%s: Not enough memory for new protection bitmap\n",
                __func__);
        return -ENOMEM;
    }
    pb->bitmap = bmnew;
    if (bmold) {
        uint old_size = gpa_space_prot_bitmap_size(pb->end_gfn);
        hax_assert(old_size != 0);
        memcpy(bmnew, bmold, old_size);
        hax_vfree(bmold, old_size);
    }
    pb->end_gfn = end_gfn;
    return 0;
}

// Sets or clears consecutive bits in a byte.
static inline void set_bits_in_byte(uint8_t *byte, int start, int nbits, bool set)
{
    uint mask;

    hax_assert(byte != NULL);
    hax_assert(start >= 0 && start < 8);
    hax_assert(nbits >= 0 && start + nbits <= 8);

    mask = ((1 << nbits) - 1) << start;
    if (set) {
        *byte = (uint8_t)(*byte | mask);
    } else {
        mask = ~mask & 0xff;
        *byte = (uint8_t)(*byte & mask);
    }
}

// Sets or clears consecutive bits in a bitmap.
static void set_bit_block(uint8_t *bitmap, uint64_t start, uint64_t nbits, bool set)
{
    // TODO: Is it safe to use 64-bit array indices on 32-bit hosts?
    uint64_t first_byte_index = start / 8;
    uint64_t last_byte_index = (start + nbits - 1) / 8;
    uint64_t i;
    int first_bit_index = (int)(start % 8);
    int last_bit_index = (int)((start + nbits - 1) % 8);

    if (first_byte_index == last_byte_index) {
        set_bits_in_byte(&bitmap[first_byte_index], first_bit_index, (int)nbits,
                         set);
        return;
    }

    set_bits_in_byte(&bitmap[first_byte_index], first_bit_index,
                     8 - first_bit_index, set);
    for (i = first_byte_index + 1; i < last_byte_index; i++) {
        bitmap[i] = set ? 0xff : 0;
    }
    set_bits_in_byte(&bitmap[last_byte_index], 0, last_bit_index + 1, set);
}

bool gpa_space_is_page_protected(struct hax_gpa_space *gpa_space, uint64_t gfn)
{
    struct hax_gpa_prot *pbm = &gpa_space->prot;

    if (!pbm)
        return false;

    if (gfn >= pbm->end_gfn)
        return false;

    // Since gfn < pbm->end_gfn < 2^31 (cf. gpa_space_prot_bitmap_size()), it's
    // safe to convert it to int.
    return hax_test_bit((int)gfn, (uint64_t *)pbm->bitmap);
}

bool gpa_space_is_chunk_protected(struct hax_gpa_space *gpa_space, uint64_t gfn,
                                  uint64_t *fault_gfn)
{
#define HAX_CHUNK_NR_PAGES (HAX_CHUNK_SIZE / PAGE_SIZE_4K)
    // FIXME: Chunks are created in HVA space (by dividing a RAM block), not in
    // GPA space. So rounding a GPA down to the nearest HAX_CHUNK_SIZE boundary
    // is not guaranteed to yield a GPA that maps to the same chunk.
    uint64_t start_gfn = gfn / HAX_CHUNK_NR_PAGES * HAX_CHUNK_NR_PAGES;
    uint64_t temp_gfn = start_gfn;

    for (; temp_gfn < start_gfn + HAX_CHUNK_NR_PAGES; temp_gfn++)
        if (gpa_space_is_page_protected(gpa_space, temp_gfn)) {
            *fault_gfn = temp_gfn;
            return true;
        }

    return false;
}

int gpa_space_protect_range(struct hax_gpa_space *gpa_space,
                            uint64_t start_gpa, uint64_t len, uint32_t flags)
{
    uint perm = (uint)(flags & HAX_RAM_PERM_MASK);
    uint64_t first_gfn, last_gfn, npages;
    hax_gpa_space_listener *listener;

    if (perm == HAX_RAM_PERM_NONE) {
        hax_log(HAX_LOGI, "%s: Restricting access to GPA range 0x%llx.."
                "0x%llx\n", __func__, start_gpa, start_gpa + len);
    } else {
        hax_log(HAX_LOGD, "%s: start_gpa=0x%llx, len=0x%llx, flags=%x\n",
                __func__, start_gpa, len, flags);
    }

    if (len == 0) {
        hax_log(HAX_LOGE, "%s: len = 0\n", __func__);
        return -EINVAL;
    }

    // Find-grained protection (R/W/X) is not supported yet
    if (perm != HAX_RAM_PERM_NONE && perm != HAX_RAM_PERM_RWX) {
        hax_log(HAX_LOGE, "%s: Unsupported flags=%d\n", __func__, flags);
        return -EINVAL;
    }

    first_gfn = start_gpa >> PG_ORDER_4K;
    last_gfn = (start_gpa + len - 1) >> PG_ORDER_4K;
    if (last_gfn >= gpa_space->prot.end_gfn) {
        hax_log(HAX_LOGE, "%s: GPA range exceeds protection bitmap, "
                "start_gpa=0x%llx, len=0x%llx, flags=0x%x, end_gfn=0x%llx\n",
                __func__, start_gpa, len, flags, gpa_space->prot.end_gfn);
        return -EINVAL;
    }
    npages = last_gfn - first_gfn + 1;

    // TODO: Properly handle concurrent accesses to the protection bitmap,
    // since gpa_space_protect_range() and gpa_space_is_page_protected() may be
    // called by multiple vCPU threads simultaneously.
    set_bit_block(gpa_space->prot.bitmap, first_gfn, npages,
                  perm == HAX_RAM_PERM_NONE);

    if (perm == HAX_RAM_PERM_RWX) {
        goto done;
    }

    // perm == HAX_RAM_PERM_NONE requires invalidating the EPT entries that map
    // the GPAs being protected. But ept_tree_invalidate_entries() needs a
    // |hax_ept_tree| pointer, so invoke it through the GPA space listener
    // interface instead.
    hax_list_entry_for_each(listener, &gpa_space->listener_list,
                            hax_gpa_space_listener, entry) {
        if (listener->mapping_removed) {
            // The last 2 parameters, |uva| and |flags|, are not important, so
            // just use dummy values.
            listener->mapping_removed(listener, first_gfn, npages, 0, 0);
        }
    }

done:
    return 0;
}
