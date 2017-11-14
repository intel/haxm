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
        hax_error("gpa_space_init: param gpa_space is null!\n");
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

void gpa_space_free(hax_gpa_space *gpa_space)
{
    hax_gpa_space_listener *listener, *tmp;
    if (!gpa_space) {
        hax_error("gpa_space_free: invalid param!\n");
        return;
    }

    memslot_free_list(gpa_space);
    ramblock_free_list(&gpa_space->ramblock_list);

    // Clear listener_list.
    hax_list_entry_for_each_safe(listener, tmp, &gpa_space->listener_list,
                                 hax_gpa_space_listener, entry) {
        hax_list_del(&listener->entry);
    }
}

void gpa_space_add_listener(hax_gpa_space *gpa_space,
                            hax_gpa_space_listener *listener)
{
    if (!gpa_space || !listener) {
        hax_error("gpa_space_add_listener: invalid param!\n");
        return;
    }

    listener->gpa_space = gpa_space;
    hax_list_add(&listener->entry, &gpa_space->listener_list);
}

void gpa_space_remove_listener(hax_gpa_space *gpa_space,
                               hax_gpa_space_listener *listener)
{
    if (!gpa_space || !listener) {
        hax_error("gpa_space_remove_listener: invalid param!\n");
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
static int gpa_space_map_range(hax_gpa_space *gpa_space, uint64 start_gpa,
                               int len, uint8 **buf, hax_kmap_user *kmap,
                               bool *writable)
{
    uint64 gfn;
    uint delta, size, npages;
    hax_memslot *slot;
    hax_ramblock *block;
    uint64 offset_within_block, offset_within_chunk;
    hax_chunk *chunk;
    void *kva;

    if (len < 0) {
        hax_error("%s: len=%d < 0\n", __func__, len);
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
        hax_error("%s: start_gpa=0x%llx is reserved for MMIO\n", __func__,
                  start_gpa);
        return -EINVAL;
    }
    if (writable) {
        *writable = !(slot->flags & HAX_MEMSLOT_READONLY);
    }
    if (gfn + npages > slot->base_gfn + slot->npages) {
        hax_warning("%s: GPA range spans more than one memslot:"
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
        hax_error("%s: ramblock_get_chunk() failed: start_gpa=0x%llx\n",
                  __func__, start_gpa);
        return -ENOMEM;
    }
    offset_within_chunk = offset_within_block - (chunk->base_uva -
                          block->base_uva);
    if (offset_within_chunk + size > chunk->size) {
        hax_warning("%s: GPA range spans more than one chunk: start_gpa=0x%llx,"
                    " len=%d, offset_within_chunk=0x%llx, size=0x%x,"
                    " chunk_size=0x%llx\n", __func__, start_gpa, len,
                    offset_within_chunk, size, chunk->size);
        size = (uint) (chunk->size - offset_within_chunk);
    }

    // Assuming kmap != NULL
    kva = hax_map_user_pages(&chunk->memdesc, offset_within_chunk, size, kmap);
    if (!kva) {
        hax_error("%s: hax_map_user_pages() failed: start_gpa=0x%llx, len=%d\n",
                  __func__, start_gpa, len);
        return -ENOMEM;
    }
    // Assuming buf != NULL
    *buf = (uint8 *) kva + delta;
    return (int) (size - delta);
}

int gpa_space_read_data(hax_gpa_space *gpa_space, uint64 start_gpa, int len,
                        uint8 *data)
{
    uint8 *buf;
    hax_kmap_user kmap;
    int ret, nbytes;

    if (!data) {
        hax_error("%s: data == NULL\n", __func__);
        return -EINVAL;
    }

    ret = gpa_space_map_range(gpa_space, start_gpa, len, &buf, &kmap, NULL);
    if (ret < 0) {
        hax_error("%s: gpa_space_map_range() failed: start_gpa=0x%llx,"
                  " len=%d\n", __func__, start_gpa, len);
        return ret;
    }

    nbytes = ret;
    if (nbytes < len) {
        hax_warning("%s: Not enough bytes readable from guest RAM: nbytes=%d,"
                    " start_gpa=0x%llx, len=%d\n", __func__, nbytes, start_gpa,
                    len);
        if (!nbytes) {
            return 0;
        }
    }
    memcpy_s(data, nbytes, buf, nbytes);
    ret = hax_unmap_user_pages(&kmap);
    if (ret) {
        hax_warning("%s: hax_unmap_user_pages() failed: ret=%d\n", __func__,
                    ret);
        // This is not a fatal error, so ignore it
    }
    return nbytes;
}

int gpa_space_write_data(hax_gpa_space *gpa_space, uint64 start_gpa, int len,
                         uint8 *data)
{
    uint8 *buf;
    hax_kmap_user kmap;
    bool writable;
    int ret, nbytes;

    if (!data) {
        hax_error("%s: data == NULL\n", __func__);
        return -EINVAL;
    }

    ret = gpa_space_map_range(gpa_space, start_gpa, len, &buf, &kmap,
                              &writable);
    if (ret < 0) {
        hax_error("%s: gpa_space_map_range() failed: start_gpa=0x%llx,"
                  " len=%d\n", __func__, start_gpa, len);
        return ret;
    }
    if (!writable) {
        hax_error("%s: Cannot write to ROM: start_gpa=0x%llx, len=%d\n",
                  __func__, start_gpa, len);
        return -EACCES;
    }

    nbytes = ret;
    if (nbytes < len) {
        hax_warning("%s: Not enough bytes writable to guest RAM: nbytes=%d,"
                    " start_gpa=0x%llx, len=%d\n", __func__, nbytes, start_gpa,
                    len);
        if (!nbytes) {
            return 0;
        }
    }
    memcpy_s(buf, nbytes, data, nbytes);
    ret = hax_unmap_user_pages(&kmap);
    if (ret) {
        hax_warning("%s: hax_unmap_user_pages() failed: ret=%d\n", __func__,
                    ret);
        // This is not a fatal error, so ignore it
    }
    return nbytes;
}

void * gpa_space_map_page(hax_gpa_space *gpa_space, uint64 gfn,
                          hax_kmap_user *kmap, bool *writable)
{
    uint8 *buf;
    int ret;
    void *kva;

    assert(gpa_space != NULL);
    assert(kmap != NULL);
    ret = gpa_space_map_range(gpa_space, gfn << PG_ORDER_4K, PAGE_SIZE_4K, &buf,
                              kmap, writable);
    if (ret < PAGE_SIZE_4K) {
        hax_error("%s: gpa_space_map_range() returned %d\n", __func__, ret);
        return NULL;
    }
    kva = (void *) buf;
    assert(kva != NULL);
    return kva;
}

void gpa_space_unmap_page(hax_gpa_space *gpa_space, hax_kmap_user *kmap)
{
    int ret;

    assert(kmap != NULL);
    ret = hax_unmap_user_pages(kmap);
    if (ret) {
        hax_warning("%s: hax_unmap_user_pages() returned %d\n", __func__, ret);
    }
}

uint64 gpa_space_get_pfn(hax_gpa_space *gpa_space, uint64 gfn, uint8 *flags)
{
    hax_memslot *slot;
    hax_ramblock *block;
    hax_chunk *chunk;
    uint64 pfn;
    uint64 offset_within_slot, offset_within_block, offset_within_chunk;

    assert(gpa_space != NULL);

    slot = memslot_find(gpa_space, gfn);
    if (!slot) {
        // The gfn is reserved for MMIO
        hax_debug("%s: gfn=0x%llx is reserved for MMIO\n", __func__, gfn);
        if (flags) {
            *flags = HAX_MEMSLOT_INVALID;
        }
        return INVALID_PFN;
    }

    if (flags) {
        *flags = slot->flags;
    }

    offset_within_slot = (gfn - slot->base_gfn) << PG_ORDER_4K;
    assert(offset_within_slot < (slot->npages << PG_ORDER_4K));
    block = slot->block;
    assert(block != NULL);
    offset_within_block = slot->offset_within_block + offset_within_slot;
    assert(offset_within_block < block->size);
    chunk = ramblock_get_chunk(block, offset_within_block, true);
    if (!chunk) {
        hax_error("%s: Failed to grab the RAM chunk for %s gfn=0x%llx:"
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
    assert(offset_within_chunk < chunk->size);
    pfn = hax_get_pfn_user(&chunk->memdesc, offset_within_chunk);
    assert(pfn != INVALID_PFN);

    return pfn;
}
