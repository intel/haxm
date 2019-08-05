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
#include "../include/hax_host_mem.h"
#include "include/paging.h"

static hax_epte INVALID_EPTE = {
    .pfn = INVALID_PFN
    // Other fields are initialized to 0
};

static inline uint get_pml4_index(uint64_t gfn)
{
    return (uint) (gfn >> (HAX_EPT_TABLE_SHIFT * 3));
}

static inline uint get_pdpt_gross_index(uint64_t gfn)
{
    return (uint) (gfn >> (HAX_EPT_TABLE_SHIFT * 2));
}

static inline uint get_pdpt_index(uint64_t gfn)
{
    return (uint) ((gfn >> (HAX_EPT_TABLE_SHIFT * 2)) &
                   (HAX_EPT_TABLE_SIZE - 1));
}

static inline uint get_pd_gross_index(uint64_t gfn)
{
    return (uint) (gfn >> HAX_EPT_TABLE_SHIFT);
}

static inline uint get_pd_index(uint64_t gfn)
{
    return (uint) ((gfn >> HAX_EPT_TABLE_SHIFT) & (HAX_EPT_TABLE_SIZE - 1));
}

static inline uint get_pt_index(uint64_t gfn)
{
    return (uint) (gfn & (HAX_EPT_TABLE_SIZE - 1));
}

// Allocates a |hax_ept_page| for the given |hax_ept_tree|. Returns the
// allocated |hax_ept_page|, whose underlying host page frame is filled with
// zeroes, or NULL on error.
static hax_ept_page * ept_tree_alloc_page(hax_ept_tree *tree)
{
    hax_ept_page *page;
    int ret;

    page = (hax_ept_page *) hax_vmalloc(sizeof(*page), 0);
    if (!page) {
        return NULL;
    }
    ret = hax_alloc_page_frame(HAX_PAGE_ALLOC_ZEROED, &page->memdesc);
    if (ret) {
        hax_log(HAX_LOGE, "%s: hax_alloc_page_frame() returned %d\n",
                __func__, ret);
        hax_vfree(page, sizeof(*page));
        return NULL;
    }
    hax_assert(tree != NULL);
    ept_tree_lock(tree);
    hax_list_add(&page->entry, &tree->page_list);
    ept_tree_unlock(tree);
    return page;
}

// Returns a buffer containing cached information about the |hax_ept_page|
// specified by the given EPT level (PML4, PDPT, PD or PT) and the given GFN.
// The returned buffer can be used to fill the cache if it is not yet available.
// Returns NULL if the |hax_ept_page| in question is not a frequently-used page.
static inline hax_ept_page_kmap * ept_tree_get_freq_page(hax_ept_tree *tree,
                                                         uint64_t gfn, int level)
{
    // Only HAX_EPT_FREQ_PAGE_COUNT EPT pages are considered frequently-used,
    // whose KVA mappings are cached in tree->freq_pages[]. They are:
    // a) The EPT PML4 table, covering the entire GPA space. Cached in
    //    freq_pages[0].
    // b) The first EPT PDPT table, pointed to by entry 0 of a), covering the
    //    first 512GB of the GPA space. Cached in freq_pages[1].
    // c) The first n EPT PD tables (n = HAX_EPT_FREQ_PAGE_COUNT - 2), pointed
    //    to by entries 0..(n - 1) of b), covering the first nGB of the GPA
    //    space. Cached in freq_pages[2..(n + 1)].
    hax_ept_page_kmap *freq_page = NULL;

    hax_assert(tree != NULL);
    switch (level) {
        case HAX_EPT_LEVEL_PML4: {
            freq_page = &tree->freq_pages[0];
            break;
        }
        case HAX_EPT_LEVEL_PDPT: {
            // Extract bits 63..39 of the GPA (== gfn << 12)
            uint pml4_index = get_pml4_index(gfn);
            if (pml4_index == 0) {
                freq_page = &tree->freq_pages[1];
            }
            break;
        }
        case HAX_EPT_LEVEL_PD: {
            // Extract bits 63..30 of the GPA (== gfn << 12)
            uint pml4_pdpt_index = get_pdpt_gross_index(gfn);
            if (pml4_pdpt_index < HAX_EPT_FREQ_PAGE_COUNT - 2) {
                freq_page = &tree->freq_pages[2 + pml4_pdpt_index];
            }
            break;
        }
        default: {
            break;
        }
    }
    return freq_page;
}

int ept_tree_init(hax_ept_tree *tree)
{
    hax_ept_page *root_page;
    hax_ept_page_kmap *root_page_kmap;
    void *kva;
    uint64_t pfn;

    if (!tree) {
        hax_log(HAX_LOGE, "%s: tree == NULL\n", __func__);
        return -EINVAL;
    }

    hax_init_list_head(&tree->page_list);
    memset(tree->freq_pages, 0, sizeof(tree->freq_pages));
    tree->invept_pending = false;

    tree->lock = hax_spinlock_alloc_init();
    if (!tree->lock) {
        hax_log(HAX_LOGE, "%s: Failed to allocate EPT tree lock\n", __func__);
        return -ENOMEM;
    }

    root_page = ept_tree_alloc_page(tree);
    if (!root_page) {
        hax_log(HAX_LOGE, "%s: Failed to allocate EPT root page\n", __func__);
        hax_spinlock_free(tree->lock);
        return -ENOMEM;
    }
    kva = hax_get_kva_phys(&root_page->memdesc);
    hax_assert(kva != NULL);
    pfn = hax_get_pfn_phys(&root_page->memdesc);
    hax_assert(pfn != INVALID_PFN);
    root_page_kmap = ept_tree_get_freq_page(tree, 0, HAX_EPT_LEVEL_PML4);
    hax_assert(root_page_kmap != NULL);
    root_page_kmap->page = root_page;
    root_page_kmap->kva = kva;

    tree->eptp.value = 0;
    tree->eptp.ept_mt = HAX_EPT_MEMTYPE_WB;
    tree->eptp.max_level = HAX_EPT_LEVEL_MAX;
    tree->eptp.pfn = pfn;
    hax_log(HAX_LOGI, "%s: eptp=0x%llx\n", __func__, tree->eptp.value);
    return 0;
}

static void ept_page_free(hax_ept_page *page)
{
    int ret;

    if (!page) {
        hax_log(HAX_LOGW, "%s: page == NULL\n", __func__);
        return;
    }

    ret = hax_free_page_frame(&page->memdesc);
    if (ret) {
        hax_log(HAX_LOGW, "%s: hax_free_page_frame() returned %d\n",
                __func__, ret);
        // Still need to free the hax_ept_page object
    }
    hax_vfree(page, sizeof(*page));
}

int ept_tree_free(hax_ept_tree *tree)
{
    hax_ept_page *page, *tmp;
    int i = 0;

    if (!tree) {
        hax_log(HAX_LOGE, "%s: tree == NULL\n", __func__);
        return -EINVAL;
    }

    hax_list_entry_for_each_safe(page, tmp, &tree->page_list, hax_ept_page,
                                 entry) {
        hax_list_del(&page->entry);
        ept_page_free(page);
        i++;
    }
    hax_log(HAX_LOGI, "%s: Total %d EPT page(s) freed\n", __func__, i);

    hax_spinlock_free(tree->lock);
    return 0;
}

void ept_tree_lock(hax_ept_tree *tree)
{
    hax_spin_lock(tree->lock);
}

void ept_tree_unlock(hax_ept_tree *tree)
{
    hax_spin_unlock(tree->lock);
}

// Returns a pointer (KVA) to the root page (PML4 table) of the given
// |hax_ept_tree|.
static inline hax_epte * ept_tree_get_root_table(hax_ept_tree *tree)
{
    hax_ept_page_kmap *root_page_kmap;

    root_page_kmap = ept_tree_get_freq_page(tree, 0, HAX_EPT_LEVEL_PML4);
    hax_assert(root_page_kmap != NULL);
    return (hax_epte *) root_page_kmap->kva;
}

// Given a GFN and a pointer (KVA) to an EPT page table at a non-leaf level
// (PML4, PDPT or PD) that covers the GFN, returns a pointer (KVA) to the next-
// level page table that covers the GFN. This function can be used to walk a
// |hax_ept_tree| from root to leaf.
// |tree|: The |hax_ept_tree| to walk.
// |gfn|: The GFN from which to obtain EPT page table indices.
// |current_level|: The EPT level to which |current_table| belongs. Must be a
//                  non-leaf level (PML4, PDPT or PD).
// |current_table|: The KVA of the current EPT page table. Must not be NULL.
// |kmap|: A buffer to store a host-specific KVA mapping descriptor, which may
//         be created if the next-level EPT page table is not a frequently-used
//         page. The caller must call hax_unmap_page_frame() to destroy the KVA
//         mapping when it is done with the returned pointer.
// |create|: If true and the next-level EPT page table does not yet exist,
//           creates it and updates the corresponding |hax_epte| in
//           |current_table|.
// |visit_current_epte|: An optional callback to be invoked on the |hax_epte|
//                       that belongs to |current_table| and covers |gfn|. May
//                       be NULL.
// |opaque|: An arbitrary pointer passed as-is to |visit_current_epte|.
static hax_epte * ept_tree_get_next_table(hax_ept_tree *tree, uint64_t gfn,
                                          int current_level,
                                          hax_epte *current_table,
                                          hax_kmap_phys *kmap, bool create,
                                          epte_visitor visit_current_epte,
                                          void *opaque)
{
    int next_level = current_level - 1;
    hax_ept_page_kmap *freq_page;
    uint index;
    hax_epte *epte;
    hax_epte *next_table = NULL;

    hax_assert(tree != NULL);
    hax_assert(next_level >= HAX_EPT_LEVEL_PT && next_level <= HAX_EPT_LEVEL_PDPT);
    index = (uint) ((gfn >> (HAX_EPT_TABLE_SHIFT * current_level)) &
                    (HAX_EPT_TABLE_SIZE - 1));
    hax_assert(current_table != NULL);
    epte = &current_table[index];
    if (visit_current_epte) {
        visit_current_epte(tree, gfn, current_level, epte, opaque);
    }
    if (epte->perm == HAX_EPT_PERM_NONE && !create) {
        return NULL;
    }

    freq_page = ept_tree_get_freq_page(tree, gfn, next_level);

    if (hax_cmpxchg64(0, INVALID_EPTE.value, &epte->value)) {
        // epte->value was 0, implying epte->perm == HAX_EPT_PERM_NONE, which
        // means the EPT entry pointing to the next-level page table is not
        // present, i.e. the next-level table does not exist
        hax_ept_page *page;
        uint64_t pfn;
        hax_epte temp_epte = { 0 };
        void *kva;

        page = ept_tree_alloc_page(tree);
        if (!page) {
            epte->value = 0;
            hax_log(HAX_LOGE, "%s: Failed to create EPT page table: gfn=0x%llx,"
                    " next_level=%d\n", __func__, gfn, next_level);
            return NULL;
        }
        pfn = hax_get_pfn_phys(&page->memdesc);
        hax_assert(pfn != INVALID_PFN);

        temp_epte.perm = HAX_EPT_PERM_RWX;
        // This is a non-leaf |hax_epte|, so ept_mt and ignore_pat_mt are
        // reserved (see IA SDM Vol. 3C 28.2.2 Figure 28-1)
        temp_epte.pfn = pfn;

        kva = hax_get_kva_phys(&page->memdesc);
        hax_assert(kva != NULL);
        if (freq_page) {
            // The next-level EPT table is frequently used, so initialize its
            // KVA mapping cache
            freq_page->page = page;
            freq_page->kva = kva;
        }

        // Create this non-leaf EPT entry
        epte->value = temp_epte.value;

        next_table = (hax_epte *) kva;
        hax_log(HAX_LOGD, "%s: Created EPT page table: gfn=0x%llx, "
                "next_level=%d, pfn=0x%llx, kva=%p, freq_page_index=%ld\n",
                __func__, gfn, next_level, pfn, kva,
                freq_page ? freq_page - tree->freq_pages : -1);
    } else {  // !hax_cmpxchg64(0, INVALID_EPTE.value, &epte->value)
        // epte->value != 0, which could mean epte->perm != HAX_EPT_PERM_NONE,
        // i.e. the EPT entry pointing to the next-level EPT page table is
        // present. But there is another case: *epte == INVALID_EPTE, which
        // means the next-level page table is being created by another thread
        void *kva;
        int i = 0;

        while (epte->value == INVALID_EPTE.value) {
            // Eventually the other thread will set epte->pfn to either a valid
            // PFN or 0
            if (!(++i % 10000)) {  // 10^4
                hax_log(HAX_LOGI, "%s: In iteration %d of while loop\n",
                        __func__, i);
                if (i == 100000000) {  // 10^8 (< INT_MAX)
                    hax_log(HAX_LOGE, "%s: Breaking out of infinite loop: "
                            "gfn=0x%llx, next_level=%d\n", __func__, gfn,
                            next_level);
                    return NULL;
                }
            }
        }
        if (!epte->value) {
            // The other thread has cleared epte->value, indicating it could not
            // create the next-level page table
            hax_log(HAX_LOGE, "%s: Another thread tried to create the same EPT "
                    "page table first, but failed: gfn=0x%llx, next_level=%d\n",
                    __func__, gfn, next_level);
            return NULL;
        }

        if (freq_page) {
            // The next-level EPT table is frequently used, so its KVA mapping
            // must have been cached
            kva = freq_page->kva;
            hax_assert(kva != NULL);
        } else {
            // The next-level EPT table is not frequently used, which means a
            // temporary KVA mapping needs to be created
            hax_assert(epte->pfn != INVALID_PFN);
            hax_assert(kmap != NULL);
            kva = hax_map_page_frame(epte->pfn, kmap);
            if (!kva) {
                hax_log(HAX_LOGE, "%s: Failed to map pfn=0x%llx into "
                        "KVA space\n", __func__, epte->pfn);
            }
        }
        next_table = (hax_epte *) kva;
    }
    return next_table;
}

static inline void kmap_swap(hax_kmap_phys *kmap1, hax_kmap_phys *kmap2)
{
    hax_kmap_phys tmp;

    hax_assert(kmap1 != NULL && kmap2 != NULL);
    tmp = *kmap1;
    *kmap1 = *kmap2;
    *kmap2 = tmp;
}

int ept_tree_create_entry(hax_ept_tree *tree, uint64_t gfn, hax_epte value)
{
    hax_epte *table;
    int level;
    hax_kmap_phys kmap = { 0 }, prev_kmap = { 0 };
    int ret;
    uint pt_index;
    hax_epte *pte;

    if (!tree) {
        hax_log(HAX_LOGE, "%s: tree == NULL\n", __func__);
        return -EINVAL;
    }
    if (value.perm == HAX_EPT_PERM_NONE) {
        hax_log(HAX_LOGE, "%s: value.perm == 0\n", __func__);
        return -EINVAL;
    }

    table = ept_tree_get_root_table(tree);
    hax_assert(table != NULL);
    for (level = HAX_EPT_LEVEL_PML4; level >= HAX_EPT_LEVEL_PD; level--) {
        table = ept_tree_get_next_table(tree, gfn, level, table, &kmap, true,
                                        NULL, NULL);
        // The previous table is no longer used, so destroy its KVA mapping
        // Note that hax_unmap_page_frame() does not fail when the KVA mapping
        // descriptor is filled with zeroes
        ret = hax_unmap_page_frame(&prev_kmap);
        hax_assert(ret == 0);
        // prev_kmap is now filled with zeroes
        if (!table) {
            hax_log(HAX_LOGE, "%s: Failed to grab the next-level EPT page "
                    "table: gfn=0x%llx, level=%d\n", __func__, gfn, level);
            return -ENOMEM;
        }
        // Swap prev_kmap with kmap
        kmap_swap(&prev_kmap, &kmap);
        // kmap is now filled with zeroes
    }
    // Now level == HAX_EPT_LEVEL_PT, and table points to an EPT leaf page (PT)
    pt_index = get_pt_index(gfn);
    hax_assert(table != NULL);
    pte = &table[pt_index];
    if (!hax_cmpxchg64(0, value.value, &pte->value)) {
        // pte->value != 0, implying pte->perm != HAX_EPT_PERM_NONE
        if (pte->value != value.value) {
            hax_log(HAX_LOGE, "%s: A different PTE corresponding to gfn=0x%llx"
                    " already exists: old_value=0x%llx, new_value=0x%llx\n",
                    __func__, gfn, pte->value, value.value);
            hax_unmap_page_frame(&kmap);
            return -EEXIST;
        } else {
            hax_log(HAX_LOGI, "%s: Another thread has already created the"
                    " same PTE: gfn=0x%llx, value=0x%llx\n",
                    __func__, gfn, value.value);
        }
    }

    ret = hax_unmap_page_frame(&prev_kmap);
    hax_assert(ret == 0);
    return 0;
}

int ept_tree_create_entries(hax_ept_tree *tree, uint64_t start_gfn, uint64_t npages,
                            hax_chunk *chunk, uint64_t offset_within_chunk,
                            uint8_t flags)
{
    bool is_rom = flags & HAX_MEMSLOT_READONLY;
    hax_epte new_pte = { 0 };
    uint64_t gfn, end_gfn;
    hax_epte *pml4, *pdpt, *pd, *pt;
    hax_kmap_phys pdpt_kmap = { 0 }, pd_kmap = { 0 }, pt_kmap = { 0 };
    int ret;
    uint index, start_index, end_index;
    uint64_t offset = offset_within_chunk;
    int created_count = 0;

    hax_assert(tree != NULL);
    hax_assert(npages != 0);
    hax_assert(chunk != NULL);
    hax_assert(offset_within_chunk + (npages << PG_ORDER_4K) <= chunk->size);

    new_pte.perm = is_rom ? HAX_EPT_PERM_RX : HAX_EPT_PERM_RWX;
    // According to IA SDM Vol. 3A 11.3.2, WB offers the best performance and
    // should be used in most cases, whereas UC is mostly for MMIO and WC for
    // frame buffers
    new_pte.ept_mt = HAX_EPT_MEMTYPE_WB;
    // TODO: Should ignore_pat_mt be set?

    gfn = start_gfn;
    end_gfn = start_gfn + npages - 1;
    pml4 = ept_tree_get_root_table(tree);
    hax_assert(pml4 != NULL);
next_pdpt:
    pdpt = ept_tree_get_next_table(tree, gfn, HAX_EPT_LEVEL_PML4, pml4,
                                   &pdpt_kmap, true, NULL, NULL);
    if (!pdpt) {
        hax_log(HAX_LOGE, "%s: Failed to grab the EPT PDPT for %s gfn=0x%llx\n",
                __func__, is_rom ? "ROM" : "RAM", gfn);
        ret = -ENOMEM;
        goto out;
    }
next_pd:
    pd = ept_tree_get_next_table(tree, gfn, HAX_EPT_LEVEL_PDPT, pdpt, &pd_kmap,
                                 true, NULL, NULL);
    if (!pd) {
        hax_log(HAX_LOGE, "%s: Failed to grab the EPT PD for %s gfn=0x%llx\n",
                __func__, is_rom ? "ROM" : "RAM", gfn);
        ret = -ENOMEM;
        goto out_pdpt;
    }
next_pt:
    pt = ept_tree_get_next_table(tree, gfn, HAX_EPT_LEVEL_PD, pd, &pt_kmap,
                                 true, NULL, NULL);
    if (!pt) {
        hax_log(HAX_LOGE, "%s: Failed to grab the EPT PT for %s gfn=0x%llx\n",
                __func__, is_rom ? "ROM" : "RAM", gfn);
        ret = -ENOMEM;
        goto out_pd;
    }

    // Suppose that there was a macro
    //  make_gfn(pml4_index, pdpt_index, pd_index, pt_index)
    // and that gfn == make_gfn(w, x, y, z), where each of w, x, y, z is between
    // 0 and 511 (i.e. HAX_EPT_TABLE_SIZE - 1). Now we have obtained the PT that
    // covers GFNs make_gfn(w, x, y, 0) .. make_gfn(w, x, y, 511).
    start_index = get_pt_index(gfn);
    // There are two cases here:
    //  i) end_gfn == make_gfn(w, x, y, z'), where z <= z' <= 511. Obviously we
    //     just need to create PTEs pt[z] .. pt[z'].
    // ii) end_gfn == make_gfn(w', x', y', z'), where
    //      make_gfn(w', x', y', 0) > make_gfn(w, x, y, 0)
    //     which implies end_gfn > make_gfn(w, x, y, 511). This means we need to
    //     first create PTEs pt[z] .. pt[511], and then grab the next PT by
    //     incrementing y.
    end_index = get_pd_gross_index(end_gfn) > get_pd_gross_index(gfn) ?
                HAX_EPT_TABLE_SIZE - 1 : get_pt_index(end_gfn);
    for (index = start_index; index <= end_index; index++) {
        hax_epte *pte = &pt[index];

        new_pte.pfn = hax_get_pfn_user(&chunk->memdesc, offset);
        hax_assert(new_pte.pfn != INVALID_PFN);
        if (!hax_cmpxchg64(0, new_pte.value, &pte->value)) {
            // pte->value != 0, implying pte->perm != HAX_EPT_PERM_NONE
            if (pte->value != new_pte.value) {
                hax_log(HAX_LOGE, "%s: A different PTE corresponding to %s "
                        "gfn=0x%llx already exists: old_value=0x%llx, "
                        "new_value=0x%llx\n", __func__, is_rom ? "ROM" : "RAM",
                        gfn, pte->value, new_pte.value);
                ret = -EEXIST;
                goto out_pt;
            } else {
                hax_log(HAX_LOGD, "%s: Another thread has already created the "
                        "same PTE: gfn=0x%llx, value=0x%llx, is_rom=%s\n",
                        __func__, gfn, new_pte.value,
                        is_rom ? "true" : "false");
            }
        } else {
            // pte->value was 0, but has been set to new_pte.value
            created_count++;
        }
        gfn++;
        offset += PAGE_SIZE_4K;
    }
    if (gfn <= end_gfn) {
        // We are in case ii) described above, i.e. we just created a PTE for
        // gfn - 1 == make_gfn(w, x, y, 511), and need to grab the next PT.
        // Now gfn must be equal to one of the following:
        // a) make_gfn(w, x, y + 1, 0), if y < 511;
        // b) make_gfn(w, x + 1, 0, 0), if y == 511 and x < 511;
        // c) make_gfn(w + 1, 0, 0, 0), if x == y == 511 and w < 511;
        // d) make_gfn(512, 0, 0, 0) (invalid), if w == x == y == 511. This
        //    cannot possibly happen, because end_gfn must be valid.
        hax_assert(!get_pt_index(gfn));
        hax_unmap_page_frame(&pt_kmap);
        if (!get_pd_index(gfn)) {
            hax_unmap_page_frame(&pd_kmap);
            if (!get_pdpt_index(gfn)) {
                // This is case c) above
                hax_unmap_page_frame(&pdpt_kmap);
                goto next_pdpt;
            } else {  // get_pdpt_index(gfn) != 0
                // This is case b) above
                goto next_pd;
            }
        } else {  // get_pd_index(gfn) != 0
            // This is case a) above
            goto next_pt;
        }
    }
    // Now gfn > end_gfn, i.e. we are done
    ret = created_count;

out_pt:
    hax_unmap_page_frame(&pt_kmap);
out_pd:
    hax_unmap_page_frame(&pd_kmap);
out_pdpt:
    hax_unmap_page_frame(&pdpt_kmap);
out:
    return ret;
}

void get_pte(hax_ept_tree *tree, uint64_t gfn, int level, hax_epte *epte,
             void *opaque)
{
    hax_epte *pte;

    if (level > HAX_EPT_LEVEL_PT) {
        return;
    }

    // level == HAX_EPT_LEVEL_PT
    hax_assert(epte != NULL);
    hax_assert(opaque != NULL);
    pte = (hax_epte *) opaque;
    *pte = *epte;
}

hax_epte ept_tree_get_entry(hax_ept_tree *tree, uint64_t gfn)
{
    hax_epte pte = { 0 };

    ept_tree_walk(tree, gfn, get_pte, &pte);
    return pte;
}

void ept_tree_walk(hax_ept_tree *tree, uint64_t gfn, epte_visitor visit_epte,
                   void *opaque)
{
    hax_epte *table;
    int level;
    hax_kmap_phys kmap = { 0 }, prev_kmap = { 0 };
    int ret;
    uint pt_index;
    hax_epte *pte;

    if (!tree) {
        hax_log(HAX_LOGE, "%s: tree == NULL\n", __func__);
        return;
    }
    if (!visit_epte) {
        hax_log(HAX_LOGW, "%s: visit_epte == NULL\n", __func__);
        return;
    }

    table = ept_tree_get_root_table(tree);
    hax_assert(table != NULL);
    for (level = HAX_EPT_LEVEL_PML4; level >= HAX_EPT_LEVEL_PD; level--) {
        table = ept_tree_get_next_table(tree, gfn, level, table, &kmap, false,
                                        visit_epte, opaque);
        ret = hax_unmap_page_frame(&prev_kmap);
        hax_assert(ret == 0);
        if (!table) {
            // An intermediate EPT page table is missing, which means the EPT
            // leaf entry to be invalidated is not present
            return;
        }
        kmap_swap(&prev_kmap, &kmap);
    }
    pt_index = get_pt_index(gfn);
    hax_assert(table != NULL);
    pte = &table[pt_index];
    visit_epte(tree, gfn, HAX_EPT_LEVEL_PT, pte, opaque);

    ret = hax_unmap_page_frame(&prev_kmap);
    hax_assert(ret == 0);
}

void invalidate_pte(hax_ept_tree *tree, uint64_t gfn, int level, hax_epte *epte,
                    void *opaque)
{
    hax_epte *pte;
    bool *modified;

    if (level > HAX_EPT_LEVEL_PT) {
        return;
    }

    // level == HAX_EPT_LEVEL_PT
    hax_assert(tree != NULL);
    hax_assert(epte != NULL);
    hax_assert(opaque != NULL);
    pte = epte;
    modified = (bool *) opaque;
    if (pte->perm == HAX_EPT_PERM_NONE) {
        *modified = false;
        return;
    }

    hax_log(HAX_LOGI, "%s: Invalidating PTE: gfn=0x%llx, value=0x%llx\n",
            __func__, gfn, pte->value);
    ept_tree_lock(tree);
    pte->value = 0;  // implies pte->perm == HAX_EPT_PERM_NONE
    ept_tree_unlock(tree);
    *modified = true;
}

// Returns 1 if the EPT leaf entry to be invalidated was present, or 0 if it is
// not present.
static int ept_tree_invalidate_entry(hax_ept_tree *tree, uint64_t gfn)
{
    bool modified = false;

    ept_tree_walk(tree, gfn, invalidate_pte, &modified);
    return modified ? 1 : 0;
}

int ept_tree_invalidate_entries(hax_ept_tree *tree, uint64_t start_gfn,
                                uint64_t npages)
{
    uint64_t end_gfn = start_gfn + npages, gfn;
    int modified_count = 0;

    if (!tree) {
        hax_log(HAX_LOGE, "%s: tree == NULL\n", __func__);
        return -EINVAL;
    }

    // TODO: Implement a faster algorithm
    for (gfn = start_gfn; gfn < end_gfn; gfn++) {
        int ret = ept_tree_invalidate_entry(tree, gfn);
        hax_assert(ret == 0 || ret == 1);
        modified_count += ret;
    }
    if (modified_count) {
        if (hax_test_and_set_bit(0, (uint64_t *) &tree->invept_pending)) {
            hax_log(HAX_LOGW, "%s: INVEPT pending flag is already set\n",
                    __func__);
        }
    }
    return modified_count;
}
