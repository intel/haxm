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

static inline uint64_t ramblock_count_chunks(hax_ramblock *block)
{
    // Assuming block != NULL && block->size != 0
    return (block->size - 1) / HAX_CHUNK_SIZE + 1;
}

static inline uint64_t ramblock_count_bitmap_size(uint64_t nchunks)
{
    uint64_t chunks_bitmap_size;

    chunks_bitmap_size = (nchunks + 7) / 8;
    // add more 8 bytes to chunks_bitmap_size to avoid memory out of bounds
    // when accessing the tail of chunks_bitmap by pointer type other than
    // "uint8_t *". This fix the BSOD happened in hax_test_and_set_bit() when it
    // accesses the tail of chunks_bitmap.
    chunks_bitmap_size += 8;

    return chunks_bitmap_size;
}

static hax_ramblock * ramblock_alloc(uint64_t base_uva, uint64_t size)
{
    hax_ramblock *block;
    uint64_t nchunks;
    hax_chunk **chunks;
    uint64_t chunks_bitmap_size;
    uint8_t *chunks_bitmap;

    block = (hax_ramblock *) hax_vmalloc(sizeof(*block), 0);
    if (!block) {
        hax_log(HAX_LOGE, "%s: Failed to allocate ramblock for UVA range:"
                " base_uva=0x%llx, size=0x%llx\n", __func__, base_uva, size);
        return NULL;
    }

    block->base_uva = base_uva;
    block->size = size;
    nchunks = ramblock_count_chunks(block);
    chunks = (hax_chunk **) hax_vmalloc(nchunks * sizeof(*chunks), 0);
    if (!chunks) {
        hax_log(HAX_LOGE, "%s: Failed to allocate chunks array: nchunks=0x%llx,"
                " size=0x%llx\n", __func__, nchunks, size);
        hax_vfree(block, sizeof(*block));
        return NULL;
    }
    memset(chunks, 0, nchunks * sizeof(*chunks));
    block->chunks = chunks;

    chunks_bitmap_size = ramblock_count_bitmap_size(nchunks);
    chunks_bitmap = (uint8_t *) hax_vmalloc(chunks_bitmap_size, 0);
    if (!chunks_bitmap) {
        hax_log(HAX_LOGE, "%s: Failed to allocate chunks bitmap: "
                "nchunks=0x%llx, chunks_bitmap_size=0x%llx, size=0x%llx\n",
                __func__, nchunks, chunks_bitmap_size, size);
        hax_vfree(chunks, nchunks * sizeof(*chunks));
        hax_vfree(block, sizeof(*block));
        return NULL;
    }
    memset(chunks_bitmap, 0, chunks_bitmap_size);
    block->chunks_bitmap = chunks_bitmap;
    block->is_standalone = false;
    block->ref_count = 0;

    return block;
}

static void ramblock_free_chunks(hax_ramblock *block, bool destroy)
{
    hax_chunk **chunks;
    uint64_t nchunks, chunks_bitmap_size, i;
    uint64_t nbytes_used = 0;

    hax_assert(block != NULL);
    // Assuming block->chunks != NULL due to a previous ramblock_alloc() call
    chunks = block->chunks;
    nchunks = ramblock_count_chunks(block);
    chunks_bitmap_size = ramblock_count_bitmap_size(nchunks);
    hax_log(HAX_LOGI, "%s: Freeing <= %llu chunks, bitmap:\n",
            __func__, nchunks);
    for (i = 0; i < chunks_bitmap_size; i++) {
        hax_log(HAX_LOGI, "%s:   [%llu]=0x%02x\n", __func__, i,
                block->chunks_bitmap[i]);
    }
    for (i = 0; i < nchunks; i++) {
        hax_chunk *chunk = chunks[i];
        int ret;

        if (!chunk) {
            // Skip chunks that have not been allocated/pinned
            continue;
        }
        nbytes_used += chunk->size;

        // Three steps are needed to free a chunk:
        //  1) Call chunk_free()
        //  2) Clear chunks[i]
        //  3) Clear bit i of chunks_bitmap
        // But if they are carried out in this order, between 1) and 2), another
        // thread may call ramblock_get_chunk() and get a bad pointer. So
        // reverse the order to avoid that.
        if (hax_test_and_clear_bit((int) i, (uint64_t *) block->chunks_bitmap)) {
            // Bit i of chunks_bitmap was already clear
            hax_log(HAX_LOGW, "%s: chunks[%llu] existed but its bit in "
                    "chunks_bitmap was not set: size=0x%llx, "
                    "block.size=0x%llx\n", __func__, i, chunk->size,
                    block->size);
        }
        chunks[i] = NULL;
        ret = chunk_free(chunk);
        if (ret) {
            hax_log(HAX_LOGW, "%s: Failed to free chunk: i=%llu, "
                    "base_uva=0x%llx, size=0x%llx, ret=%d\n", __func__, i,
                    chunk->base_uva, chunk->size, ret);
        }
    }
    // Double check that there is no bit set in chunks_bitmap
    for (i = 0; i < chunks_bitmap_size; i++) {
        if (block->chunks_bitmap[i]) {
            hax_log(HAX_LOGW, "%s: chunks_bitmap[%llu]=0x%02x\n", __func__, i,
                    block->chunks_bitmap[i]);
            block->chunks_bitmap[i] = 0;
        }
    }

    hax_log(HAX_LOGI, "%s: All chunks freed: %lluKB total, %lluKB used\n",
            __func__, block->size / 1024, nbytes_used / 1024);

    if (!destroy)
        return;

    // Free the chunks bitmap
    hax_vfree(block->chunks_bitmap, chunks_bitmap_size);
    // Free the chunks array
    hax_vfree(chunks, nchunks * sizeof(*chunks));
}

static void ramblock_free(hax_ramblock *block)
{
    if (!block) {
        hax_log(HAX_LOGW, "%s: block == NULL\n", __func__);
        return;
    }

    ramblock_free_chunks(block, true);
    // Free the hax_ramblock object
    hax_vfree(block, sizeof(*block));
}

int ramblock_init_list(hax_list_head *list)
{
    if (!list) {
        hax_log(HAX_LOGE, "ramblock_init_list: list is null \n");
        return -EINVAL;
    }
    hax_log(HAX_LOGI, "ramblock_init_list\n");
    hax_init_list_head(list);

    return 0;
}

static void ramblock_remove(hax_ramblock *block)
{
    hax_assert(block != NULL);
    hax_log(HAX_LOGI, "%s: Removing RAM block: base_uva=0x%llx, size=0x%llx,"
            " is_standalone=%d, ref_count=%d\n", __func__, block->base_uva,
            block->size, block->is_standalone, block->ref_count);
    hax_list_del(&block->entry);
    ramblock_free(block);
}

void ramblock_free_list(hax_list_head *list)
{
    hax_ramblock *ramblock, *tmp;

    if (!list) {
        hax_log(HAX_LOGE, "ramblock_free_list: list is null \n");
        return;
    }

    hax_log(HAX_LOGI, "ramblock_free_list\n");
    hax_list_entry_for_each_safe(ramblock, tmp, list, hax_ramblock, entry) {
        ramblock_remove(ramblock);
    }
}

void ramblock_dump_list(hax_list_head *list)
{
    hax_ramblock *ramblock;
    int i = 0;

    hax_log(HAX_LOGI, "ramblock dump begin:\n");
    hax_list_entry_for_each(ramblock, list, hax_ramblock, entry) {
        hax_log(HAX_LOGI, "block %d (%p): base_uva 0x%llx, size 0x%llx, "
                "ref_count %d\n", i++, ramblock, ramblock->base_uva,
                ramblock->size, ramblock->ref_count);
    }
    hax_log(HAX_LOGI, "ramblock dump end!\n");
}

// TODO: parameter 'start' is ignored for now
hax_ramblock * ramblock_find(hax_list_head *list, uint64_t uva,
                             hax_list_node *start)
{
    hax_ramblock *ramblock;

    hax_list_entry_for_each(ramblock, list, hax_ramblock, entry) {
        if (ramblock->base_uva > uva)
            break;

        if (uva < ramblock->base_uva + ramblock->size) {
            hax_log(HAX_LOGD, "%s: (%p): base_uva 0x%llx, size 0x%llx, "
                    "ref_count %d\n", __func__, ramblock, ramblock->base_uva,
                    ramblock->size, ramblock->ref_count);

            ramblock_ref(ramblock);
            return ramblock;
        }
    }

    hax_log(HAX_LOGW, "can not find 0x%llx in ramblock list.\n", uva);
    return NULL;
}

// TODO: parameter 'start' is ignored for now
int ramblock_add(hax_list_head *list, uint64_t base_uva, uint64_t size,
                 hax_list_node *start, hax_ramblock **block)
{
    hax_ramblock *ramblock, *ramblock2;

    if (!list) {
        hax_log(HAX_LOGE, "invalid list: list head is null.\n");
        return -EINVAL;
    }

    ramblock = ramblock_alloc(base_uva, size);
    if (!ramblock) {
        return -ENOMEM;
    }

    hax_log(HAX_LOGI, "Adding block: base_uva 0x%llx, size 0x%llx\n",
            ramblock->base_uva, ramblock->size);

    if (hax_list_empty(list)) {
        // TODO: change hax_list_add to hax_list_insert_after
        hax_list_add(&ramblock->entry, list);
        goto add_finished;
    }

    hax_list_entry_for_each(ramblock2, list, hax_ramblock, entry) {
        if ((ramblock->base_uva + ramblock->size) <= ramblock2->base_uva) {
            // Insert before ramblock2
            hax_list_add(&ramblock->entry, ramblock2->entry.prev);
            break;
        } else if (ramblock->base_uva >=
                   (ramblock2->base_uva + ramblock2->size)) {
            if (ramblock2->entry.next == list) {
                // If ramblock2 is the last entry, then insert after ramblock2
                hax_list_add(&ramblock->entry, &ramblock2->entry);
                break;
            } else {
                continue;
            }
        } else {
            // If the program comes here, it denotes that there is overlap
            // between ramblock and ramblock2
            ramblock_free(ramblock);
            hax_log(HAX_LOGE, "New ramblock base_uva 0x%llx, size 0x%llx "
                    "overlaps with existing ramblock: base_uva 0x%llx, "
                    "size 0x%llx\n", base_uva, size, ramblock2->base_uva,
                    ramblock2->size);
            return -EINVAL;
        }
    }

add_finished:

    if (block) {
        *block = ramblock;
    }

    ramblock_dump_list(list);
    return 0;
}

hax_chunk * ramblock_get_chunk(hax_ramblock *block, uint64_t uva_offset,
                               bool alloc)
{
    uint64_t chunk_index;

    if (!block) {
        hax_log(HAX_LOGE, "%s: block == NULL\n", __func__);
        return NULL;
    }
    if (uva_offset >= block->size) {
        hax_log(HAX_LOGW, "%s: uva_offset=0x%llx >= block->size=0x%llx\n",
                __func__, uva_offset, block->size);
        return NULL;
    }

    chunk_index = uva_offset >> HAX_CHUNK_SHIFT;
    if (!alloc) {
        goto done;
    }

    // It should be safe to convert chunk_index to int, because even if
    //  block->size == 4GB && HAX_CHUNK_SIZE == 4KB
    // the number of chunks (2^20) will still be much less than INT_MAX
    if (!hax_test_and_set_bit((int) chunk_index,
                              (uint64_t *) block->chunks_bitmap)) {
        // The bit corresponding to this chunk was not set
        uint64_t uva_offset_low = chunk_index << HAX_CHUNK_SHIFT;
        uint64_t uva_offset_high = (chunk_index + 1) << HAX_CHUNK_SHIFT;
        uint64_t chunk_base_uva = block->base_uva + uva_offset_low;
        // The last chunk may be smaller than HAX_CHUNK_SIZE
        uint64_t chunk_size = uva_offset_high > block->size ?
                            block->size % HAX_CHUNK_SIZE :
                            HAX_CHUNK_SIZE;
        hax_chunk *chunk;
        int ret;

        hax_assert(block->chunks[chunk_index] == NULL);
        ret = chunk_alloc(chunk_base_uva, chunk_size, &chunk);
        if (ret) {
            int was_clear;

            // No need to test the bit here (which should be set), but there is
            // no such API as hax_clear_bit()
            was_clear = hax_test_and_clear_bit((int) chunk_index,
                                               (uint64_t *) block->chunks_bitmap);
            hax_log(HAX_LOGE, "%s: Failed to allocate chunk: ret=%d, "
                    "index=%llu, base_uva=0x%llx, size=0x%llx, was_clear=%d\n",
                    __func__, ret, chunk_index, chunk_base_uva, chunk_size,
                    was_clear);
            return NULL;
        }
        hax_assert(chunk != NULL);
        hax_assert(block->chunks[chunk_index] == NULL);
        block->chunks[chunk_index] = chunk;
    } else {
        // The bit corresponding to this chunk has been set, possibly by another
        // thread executing this function concurrently with this thread
        // Wait for that thread to finish allocating/pinning the chunk
        int i = 0;

        while (!block->chunks[chunk_index]) {
            if (!hax_test_bit((int) chunk_index,
                              (uint64_t *) block->chunks_bitmap)) {
                // The other thread has reset the bit, indicating the chunk
                // could not be allocated/pinned
                hax_log(HAX_LOGE, "%s: Another thread tried to allocate this "
                        "chunk first, but failed: index=%llu, "
                        "block.size=0x%llx, block.base_uva=0x%llx\n",
                        __func__, chunk_index, block->size, block->base_uva);
                return NULL;
            }
            if (!(++i % 100000)) {  // 10^5
                hax_log(HAX_LOGI, "%s: In iteration %d of while loop\n",
                        __func__, i);
                if (i == 1000000000) {  // 10^9 (< INT_MAX)
                    hax_log(HAX_LOGE, "%s: Breaking out of infinite loop: "
                            "index=%llu, block.size=0x%llx, "
                            "block.base_uva=0x%llx\n", __func__, chunk_index,
                            block->size, block->base_uva);
                    return NULL;
                }
            }
        }
    }
done:
    return block->chunks[chunk_index];
}

void ramblock_ref(hax_ramblock *block)
{
    if (block == NULL) {
        hax_log(HAX_LOGE, "%s: Invalid RAM block\n", __func__);
        return;
    }

    ++block->ref_count;
    hax_log(HAX_LOGD, "%s: block (%p): base_uva = 0x%llx, size = 0x%llx, "
            "ref_count = %d\n", __func__, block, block->base_uva, block->size,
            block->ref_count);
}

void ramblock_deref(hax_ramblock *block)
{
    if (block == NULL) {
        hax_log(HAX_LOGE, "%s: Invalid RAM block\n", __func__);
        return;
    }
    if (block->ref_count <= 0) {
        hax_log(HAX_LOGE, "%s: Unreferenced block (ref_count = %d)\n",
                __func__, block->ref_count);
        return;
    }
    if (--block->ref_count == 0) {
        hax_log(HAX_LOGD, "%s: Reset RAM block (%p): base_uva = 0x%llx, "
                "size = 0x%llx, ref_count = %d\n", __func__, block,
                block->base_uva, block->size, block->ref_count);
        if (block->is_standalone) {
            // A stand-alone mapping is created along with a "disposable" RAM
            // block, which must be destroyed when the mapping is unmapped, so
            // its HVA range can be reused by other stand-alone mappings.
            ramblock_remove(block);
        } else {
            ramblock_free_chunks(block, false);
        }
        return;
    }

    hax_log(HAX_LOGD, "%s: block (%p): base_uva = 0x%llx, size = 0x%llx, "
            "ref_count = %d\n", __func__, block, block->base_uva, block->size,
            block->ref_count);
}
