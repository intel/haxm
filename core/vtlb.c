/*
 * Copyright (c) 2009 Intel Corporation
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
#include "include/ia32_defs.h"
#include "include/paging.h"
#include "include/vcpu.h"
#include "include/vtlb.h"
#include "include/ept.h"
#include "include/intr.h"
#include "include/page_walker.h"

/*
 * Design rule: Only support pure 32-bit guest with 2-level page table.
 *              Host uses 3-level PAE paging for the virtual TLB.
 *
 * Design idea: Host always keeps topmost two levels' mapping in memory, and
 * this mapping services for all translations in the guest. Only change the PTE
 * level to emulate guest's page tables.
 *
 * Key APIs:
 * 1. handle_vtlb: Used to handle guest's page faults and refill proper mappings
 *    in vTLB to meet guest's translation requirement.
 * 2. vcpu_invalidate_tlb: Invalidate all the vTLB entries, maybe called in
 *    mov CR3 or page mode switch case.
 * 3. vcpu_invalidate_tlb_addr: Invalidate the special virtual address for the
 *    current page table. Maybe called as emulating invlpg instruction.
 * 4. vcpu_translate: Translate a virtual address to guest a physical address.
 * 5. vcpu_vtlb_alloc: In vcpu initialization stage, allocate a vTLB for each
 *    vCPU.
 * 6. vcpu_vtlb_free: Reverse operation of vcpu_vtlb_alloc at vcpu destroy
 *    stage.
 * 7. vcpu_read_guest_virtual:
 * 8. vcpu_write_guest_virtual:
 */

#define NR_PDE_PAGES      4
#define NR_PDE_PAGE_ORDER 2  // 2 ^ NR_PDE_PAGE_ORDER = NR_PDE_PAGES

#define NR_MMU_PAGES      256

static struct hax_page * mmu_zalloc_one_page(hax_mmu_t *mmu, bool igo);
static void mmu_recycle_vtlb_pages(hax_mmu_t *mmu);
static void vtlb_free_all_entries(hax_mmu_t *mmu);
static pagemode_t vcpu_get_pagemode(struct vcpu_t *vcpu);
static pte64_t * vtlb_get_pde(hax_mmu_t *mmu, hax_vaddr_t va, bool is_shadow);
static uint32_t vcpu_mmu_walk(struct vcpu_t *vcpu, hax_vaddr_t va, uint32_t access,
                              hax_paddr_t *pa, uint *order, uint64_t *flags,
                              bool update, bool prefetch);

static void vtlb_update_pde(pte64_t *pde, pte64_t *shadow_pde,
                            struct hax_page *page)
{
    pte64_set_entry(pde, 1, hax_page_pa(page), true, true, true);
    shadow_pde->raw = (mword)hax_page_va(page);
}

// Insert a vTLB entry to the system for the guest
static uint32_t vtlb_insert_entry(struct vcpu_t *vcpu, hax_mmu_t *mmu,
                                vtlb_t *tlb)
{
    pte64_t *pte_base, *pde, *shadow_pde, *pte;
    uint idx, is_user, is_write, is_exec, is_global, is_pwt, is_pcd, is_pat;
    uint base_idx, i;
    struct hax_page *page;
    uint64_t flags = 0;

    is_global = !!(tlb->flags & PTE32_G_BIT_MASK);
    hax_assert(mmu->host_mode == PM_PAE && tlb->order == 12);
retry:
    pde = vtlb_get_pde(mmu, tlb->va, 0);
    shadow_pde = vtlb_get_pde(mmu, tlb->va, 1);
    if (!pte64_is_present(pde)) {
        page = mmu_zalloc_one_page(mmu, is_global && igo_addr(tlb->va));
        if (!page) {
            mmu_recycle_vtlb_pages(mmu);
            goto retry;
        }
        vtlb_update_pde(pde, shadow_pde, page);
        pte64_set_accessed(pde, 1);
    }

    // Grab the PTE entry
    pte_base = (void *)(mword)shadow_pde->raw;
    idx = pte64_get_idx(0, tlb->va);
    pte = &pte_base[idx];

    is_user = !!(tlb->flags & PTE32_USER_BIT_MASK);
    is_write = !!((tlb->access & TF_WRITE) ||
                  ((tlb->flags & PTE32_D_BIT_MASK) &&
                  (tlb->flags & PTE32_W_BIT_MASK)));
    is_exec = !!(tlb->flags & ((uint64_t)1 << 63));
    is_pwt = !!(tlb->flags & PTE32_PWT_BIT_MASK);
    is_pcd = !!(tlb->flags & PTE32_PCD_BIT_MASK);
    is_pat = !!(tlb->flags & PTE32_PAT_BIT_MASK);

    // Set the pte entry accordingly.
    pte64_set_entry(pte, 0, tlb->ha, is_user, is_write, is_exec);
    pte64_set_ad(pte, 0, 1);
    pte64_set_caching(pte, is_pat, is_pcd, is_pwt);
    pte64_set_global(pte, 0, is_global);

    pte->x2 = 0x0;
    base_idx = idx - idx % 16;

    for (i = 0; i < 16; i++) {
        if (!vcpu->prefetch[i].flag)
            continue;
        pte = &pte_base[base_idx + i];
        if (pte64_is_present(pte) && (pte->x2 == 0x0))
            continue;
        pte->raw = 0;

        flags = vcpu->prefetch[i].flags;
        is_user = !!(flags & PTE32_USER_BIT_MASK);
        is_write = !!((flags & PTE32_D_BIT_MASK) && (flags & PTE32_W_BIT_MASK));
        is_exec = !!(flags & ((uint64_t)1 << 63));
        is_pwt = !!(flags & PTE32_PWT_BIT_MASK);
        is_pcd = !!(flags & PTE32_PCD_BIT_MASK);
        is_pat = !!(flags & PTE32_PAT_BIT_MASK);
        is_global = !!(flags & PTE32_G_BIT_MASK);

        // Set the pte entry accordingly.
        pte64_set_entry(pte, 0, vcpu->prefetch[i].ha, is_user, is_write,
                        is_exec);
        pte64_set_ad(pte, 0, 1);
        pte64_set_caching(pte, is_pat, is_pcd, is_pwt);
        pte64_set_global(pte, 0, is_global);
        pte->x2 = 0x1;
    }

    if(!is_global && igo_addr(tlb->va)) {
        mmu->igo = false;
    }

    return 0;
}

static uint mmu_alloc_vtlb_pages(hax_mmu_t *mmu)
{
    int i;
    struct hax_page *page, *n;

    for (i = 0; i < NR_MMU_PAGES; i++) {
        page = hax_alloc_page(0, 1);
        if (!page)
            goto alloc_fail;
        hax_list_add(&page->list, &mmu->free_page_list);
    }
    return 1;

alloc_fail:
    hax_list_entry_for_each_safe(page, n, &mmu->free_page_list, struct hax_page,
                                 list) {
        hax_list_del(&page->list);
        hax_free_page(page);
    }
    return 0;
}

static void mmu_free_vtlb_pages(hax_mmu_t *mmu)
{
    struct hax_page *page, *n;

    hax_list_entry_for_each_safe(page, n, &mmu->free_page_list, struct hax_page,
                                 list) {
        hax_list_del(&page->list);
        hax_free_page(page);
    }
    hax_list_entry_for_each_safe(page, n, &mmu->used_page_list, struct hax_page,
                                 list) {
        hax_list_del(&page->list);
        hax_free_page(page);
    }
    hax_list_entry_for_each_safe(page, n, &mmu->igo_page_list, struct hax_page,
                                 list) {
        hax_list_del(&page->list);
        hax_free_page(page);
    }
}

static struct hax_page * mmu_zalloc_one_page(hax_mmu_t *mmu, bool igo)
{
    struct hax_page *page;
    void *page_va;

    if (!hax_list_empty(&mmu->free_page_list)) {
        page = hax_list_entry(list, struct hax_page, mmu->free_page_list.next);
        hax_list_del(&page->list);
        if (igo) {
            hax_list_add(&page->list, &mmu->igo_page_list);
        } else {
            hax_list_add(&page->list, &mmu->used_page_list);
        }
        page_va = hax_page_va(page);
        hax_assert(page_va);
        memset(page_va, 0, PAGE_SIZE_4K);
        return page;
    }
    return NULL;
}

// Recycle all vTLB pages from used_list to free_list.
static void mmu_recycle_vtlb_pages(hax_mmu_t *mmu)
{
    vtlb_free_all_entries(mmu);
    hax_list_join(&mmu->used_page_list, &mmu->free_page_list);
    hax_init_list_head(&mmu->used_page_list);
    if (!mmu->igo) {
        hax_list_join(&mmu->igo_page_list, &mmu->free_page_list);
        hax_init_list_head(&mmu->igo_page_list);
    }
    mmu->igo = true;
    mmu->clean = true;
}

uint vcpu_vtlb_alloc(struct vcpu_t *vcpu)
{
    struct hax_page *page;
    uint i;
    pte64_t *pdpte;
    unsigned char *pde_va, *addr;
    hax_mmu_t *mmu;

    hax_assert(!vcpu->mmu);

    mmu = hax_vmalloc(sizeof(hax_mmu_t), 0);

    if (!mmu) {
        hax_log(HAX_LOGE, "No memory to create mmu for vcpu:%d\n",
                vcpu->vcpu_id);
        return 0;
    }
    memset(mmu, 0, sizeof(hax_mmu_t));
    vcpu->mmu = mmu;
    mmu->mmu_mode = MMU_MODE_INVALID;

    // Must ensure the first page should be lower than 4G
    page = hax_alloc_page(HAX_MEM_LOW_4G, 1);
    if (!page) {
        hax_log(HAX_LOGD, "No enough memory for creating vTLB root page!\n");
        goto alloc_fail0;
    }
    mmu->hpd_page = page;

    // Only support 32-bit guests
    mmu->pde_page = hax_alloc_pages(NR_PDE_PAGE_ORDER, 0, 1);
    if (!mmu->pde_page)
        goto alloc_fail1;

    mmu->pde_shadow_page = hax_alloc_pages(NR_PDE_PAGE_ORDER, 0, 1);
    if (!mmu->pde_shadow_page)
        goto alloc_fail2;

    pde_va = hax_page_va(mmu->pde_page);
    memset(pde_va, 0, NR_PDE_PAGES * PAGE_SIZE_4K);

    addr = hax_page_va(page);
    memset(addr, 0, PAGE_SIZE_4K);
    // Get the first PDPTE entry
    pdpte = (pte64_t *)addr;

    for (i = 0; i < 4; i++) {
        pte64_set_entry(pdpte + i, 2, hax_pa(pde_va + i * PAGE_SIZE_4K), 0, 0,
                        0);
    }

    hax_init_list_head(&mmu->free_page_list);
    hax_init_list_head(&mmu->used_page_list);
    hax_init_list_head(&mmu->igo_page_list);
    if (!mmu_alloc_vtlb_pages(mmu))
        goto alloc_fail3;

    mmu->host_mode = PM_INVALID;
    mmu->clean = true;
    mmu->igo = true;
    return 1;

alloc_fail3:
    hax_free_pages(mmu->pde_shadow_page);
    mmu->pde_shadow_page = 0;
alloc_fail2:
    hax_free_pages(mmu->pde_page);
    mmu->pde_page = 0;
alloc_fail1:
    hax_free_pages(mmu->hpd_page);
    mmu->hpd_page = 0;
alloc_fail0:
    hax_vfree(vcpu->mmu, sizeof(hax_mmu_t));
    vcpu->mmu = 0;
    return 0;
}

void vcpu_vtlb_free(struct vcpu_t *vcpu)
{
    hax_mmu_t *mmu = vcpu->mmu;
    mmu_free_vtlb_pages(mmu);
    if (mmu->pde_page) {
        hax_free_page(mmu->pde_page);
        mmu->pde_page = 0;
    }
    if (mmu->pde_shadow_page) {
        hax_free_page(mmu->pde_shadow_page);
        mmu->pde_shadow_page = 0;
    }
    if (mmu->hpd_page) {
        hax_free_page(mmu->hpd_page);
        mmu->hpd_page = 0;
    }
    hax_vfree(mmu, sizeof(hax_mmu_t));
    vcpu->mmu = 0;
}

/*
 * If is_shadow = 1, must ensure the non-shadow pde is present before calling
 * here.
 */
static pte64_t * vtlb_get_pde(hax_mmu_t *mmu, hax_vaddr_t va, bool is_shadow)
{
    pte64_t *pde;
    void *pde_va;
    uint idx = (va >> 21) & 0x1ff;
    uint32_t which_g = va >> 30;
    struct hax_page *pde_page = is_shadow ? mmu->pde_shadow_page
                                          : mmu->pde_page;

    pde_va = (unsigned char *)hax_page_va(pde_page) + which_g * PAGE_SIZE_4K;

    hax_assert(mmu->guest_mode < PM_PAE);
    pde = (pte64_t *)pde_va + idx;
    return pde;
}

static void vtlb_invalidate_pte(pte64_t *shadow_pde, hax_vaddr_t va)
{
    pte64_t *pte;
    void *pte_base;
    uint idx;

    pte_base = (void *)(mword)shadow_pde->raw;
    if (!pte_base)
        return;
    idx = pte64_get_idx(0, va);
    pte = (pte64_t *)pte_base + idx;
    pte64_clear_entry(pte);
}

void vtlb_invalidate_addr(hax_mmu_t *mmu, hax_vaddr_t va)
{
    pte64_t *pde;

    if (mmu->clean && !igo_addr(va))
        return;

    hax_assert(mmu->host_mode == PM_PAE);

    hax_log(HAX_LOGD, "Flush address 0x%llx\n", va);

    pde = vtlb_get_pde(mmu, va, 0);

    if (!pte64_is_present(pde))
        return;
    pde = vtlb_get_pde(mmu, va, 1);
    vtlb_invalidate_pte(pde, va);
}

/*
 * Doesn't need to free shadow pde here, because its entry's validity depends on
 * corresponding pde entry is present.
 */
static void vtlb_free_all_entries(hax_mmu_t *mmu)
{
    int nr_page = mmu->igo ? NR_PDE_PAGES - 1 : NR_PDE_PAGES;
    void *pde_va = hax_page_va(mmu->pde_page);
    memset(pde_va, 0, nr_page * PAGE_SIZE_4K);
}

void vtlb_invalidate(hax_mmu_t *mmu)
{
    if (mmu->clean)
        return;

    hax_assert(mmu->host_mode == PM_PAE);
    hax_log(HAX_LOGD, "Flush whole vTLB\n");
    mmu_recycle_vtlb_pages(mmu);

    mmu->clean = 1;
}

static uint vtlb_handle_page_fault(struct vcpu_t *vcpu, pagemode_t guest_mode,
                                   hax_paddr_t pdir, hax_vaddr_t va, uint32_t access)
{
    uint r;
    hax_paddr_t gpa;
    vtlb_t tlb;
    uint need_invalidation = 0;
    hax_mmu_t *mmu = vcpu->mmu;

    hax_log(HAX_LOGD, "vTLB::handle_pagefault %08llx, %08llx %x [Mode %u]\n",
            pdir, va, access, guest_mode);

    hax_assert(guest_mode != PM_INVALID);
    if (guest_mode != mmu->guest_mode) {
        pagemode_t new_host_mode = PM_INVALID;
        switch (guest_mode) {
            case PM_FLAT:
            case PM_2LVL: {
                new_host_mode = PM_PAE;
                break;
            }
            case PM_PAE:
            case PM_PML4:
            default: {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "Invalid guest page table mode %d\n",
                        mmu->guest_mode);
            }
        }

        if (new_host_mode != mmu->host_mode) {
            vtlb_invalidate(mmu);
        } else {
            need_invalidation = 1;
        }

        mmu->guest_mode = guest_mode;
        mmu->host_mode = new_host_mode;
        mmu->pdir = pdir;
        hax_log(HAX_LOGD, "New vTLB mode %u, pdir %08llx\n", guest_mode, pdir);
    }

    if (need_invalidation ||
        (pdir != mmu->pdir && mmu->guest_mode != PM_FLAT)) {
        if (!mmu->clean) {
            vtlb_invalidate(mmu);
        }
        mmu->pdir = pdir;
    }

    // Check for a mapping in the guest page tables.
    // If there isn't one, return the error code.
    switch (mmu->guest_mode) {
        case PM_FLAT: {
            r = 0;
            gpa = va;
            tlb.guest_order = PG_ORDER_4K;
            tlb.flags = (0ULL ^ EXECUTION_DISABLE_MASK) | PTE32_G_BIT_MASK |
                    PTE32_D_BIT_MASK | PTE32_USER_BIT_MASK | PTE32_W_BIT_MASK;
            break;
        }
        case PM_2LVL: {
            r = vcpu_mmu_walk(vcpu, va, access, &gpa, &tlb.guest_order,
                              &tlb.flags, true, /*true*/false);
            break;
        }
        default: {
            hax_log(HAX_LOGE, "Invalid guest's paging mode %d\n",
                    mmu->guest_mode);
            return TF_FAILED;
        }
    }

    if (r != TF_OK) {
        if (!(r & TF_GP2HP)) {
            vtlb_invalidate_addr(mmu, va);
        }
        return r;
    }

    tlb.order = tlb.guest_order = PG_ORDER_4K;
    hax_assert(tlb.order == PG_ORDER_4K);

    tlb.ha = hax_gpfn_to_hpa(vcpu->vm, gpa >> 12);
    if (!tlb.ha)
        return TF_FAILED | TF_GP2HP;

    tlb.va = va;
    tlb.access = access;

    /*
     * Only PAE paging is used to emulate pure 32-bit 2-level paging.
     * Now insert the entry in the vtlb for the translation.
     */
    hax_assert(mmu->host_mode == PM_PAE);
    vtlb_insert_entry(vcpu, mmu, &tlb);
    mmu->clean = 0;

    return r;
}

uint64_t vtlb_get_cr3(struct vcpu_t *vcpu)
{
    uint64_t cr3;

    hax_mmu_t *mmu = vcpu->mmu;

    cr3 = hax_page_pfn(mmu->hpd_page) << 12;

    hax_log(HAX_LOGD, "vTLB: guest mode %u, host mode %d, GUEST_CR3: %08llx\n",
            mmu->guest_mode, mmu->host_mode, cr3);

    return cr3;
}

/*
 * Page table walker.
 * @param vcpu      Current vcpu point
 * @param va        Guest virtual address
 * @param access    Access descriptor (read/write, user/supervisor)
 * @param pa        Guest physical address
 * @param size      Size of physical page
 * @param update    Update access and dirty bits of guest structures
 * @returns 0 if translation is successful, otherwise 0x80000000 OR'ed with
 * the page fault error code.
 */
static uint32_t vcpu_mmu_walk(struct vcpu_t *vcpu, hax_vaddr_t va, uint32_t access,
                              hax_paddr_t *pa, uint *order, uint64_t *flags,
                              bool update, bool prefetch)
{
    uint lvl, idx;
    void *pte_va;
    hax_kmap_user pte_kmap;
    bool writable;
    pte32_t *pte, old_pte;
    hax_paddr_t gpt_base;
    bool pat;
    uint64_t rights, requested_rights;

    access = access & (TF_WRITE | TF_USER | TF_EXEC);
    requested_rights = (access & (TF_WRITE | TF_USER)) |
                       (access & TF_EXEC ? EXECUTION_DISABLE_MASK : 0);
    // Seems the following one is wrong?
    // hax_assert((mmu->guest_mode) == PM_2LVL);

retry:
    rights = TF_WRITE | TF_USER;
    gpt_base = vcpu->state->_cr3 & pte32_get_cr3_mask();

    // Page table walker.
    for (lvl = PM_2LVL; lvl--; ) {
        // Fetch the page table entry.
        idx = pte32_get_idx(lvl, va);
        pte_va = gpa_space_map_page(&vcpu->vm->gpa_space,
                                    gpt_base >> PG_ORDER_4K, &pte_kmap,
                                    &writable);

        if (!pte_va)
            return TF_FAILED;

        hax_assert(!(update && !writable));

        pte = (pte32_t *)pte_va + idx;
        old_pte = *pte;

        // Check access
        if (!pte32_is_present(&old_pte)) {
            gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);

            return TF_FAILED | access;
        }

        if (pte32_check_rsvd(&old_pte, lvl)) {
            gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);

            return TF_FAILED | TF_PROTECT | TF_RSVD | access;
        }

        // Always allow execution for pure 32-bit guest!
        rights &= old_pte.raw;
        rights ^= EXECUTION_DISABLE_MASK;

        if (!pte32_is_leaf(&old_pte, lvl)) {
            // Not leaf; update accessed bit and go to the next level.
            // Note: Accessed bit is set even though the access may not
            // complete. This matches Atom behavior.
            if (update && !pte32_is_accessed(&old_pte)) {
                if (!pte32_atomic_set_accessed(pte, &old_pte)) {
                    hax_log(HAX_LOGD,
                            "translate walk: atomic PTE update failed\n");
                    gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);

                    goto retry;
                }
            }
            gpt_base = pte32_get_address(&old_pte, lvl, 0);
            gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);
        } else {
            // Permission violations must be checked only after present bit is
            // checked at every level.
            // Allow supervisor mode writes to read-only pages unless WP=1.
            if (!(access & TF_USER) && !(vcpu->state->_cr0 & CR0_WP)) {
                rights &= ~(uint64_t)TF_USER;
                rights |= TF_WRITE;
            }

            if ((rights & requested_rights) != requested_rights) {
                gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);

                return TF_FAILED | TF_PROTECT | access;
            }

            // Update accessed/dirty bits.
            if (update && (!pte32_is_accessed(&old_pte) ||
                ((access & TF_WRITE) && !pte32_is_dirty(&old_pte)))) {
                if (!pte32_atomic_set_ad(pte, lvl, access & TF_WRITE,
                    &old_pte)) {
                    hax_log(HAX_LOGD,
                            "translate walk: atomic PTE update failed\n");
                    gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);
                    goto retry;
                }
            }

            *pa = pte32_get_address(&old_pte, lvl, va);

            if (pte32_is_4M_page(&old_pte, lvl)) {
                *order = PG_ORDER_4M;
            } else {
                *order = PG_ORDER_4K;
            }
            pat = pte32_get_pat(&old_pte);
            // G, D, PCD, PWT
            *flags = rights | pat << 7 | (pte32_get_val(&old_pte) & 0x158);
            if (prefetch && hax_gpfn_to_hpa(vcpu->vm, *pa >> 12)) {
                uint base_idx = 0;
                pte32_t pre_pte;
                uint i;
                //hax_log(HAX_LOGE, "guest: va %lx\n", va);

                base_idx = idx - idx % 16;
                for (i = 0; i < 16; i++) {
                    vcpu->prefetch[i].flag = 0;
                    if (idx == base_idx + i)
                        continue;

                    pte = (pte32_t *)pte_va + (base_idx + i);
                    pre_pte = *pte;
                    if (!pte32_is_present(&pre_pte))
                        continue;

                    if (pte32_check_rsvd(&pre_pte, lvl))
                        continue;

                    if (!pte32_is_accessed(&pre_pte) ||
                        !pte32_is_dirty(&pre_pte))
                        continue;

                    vcpu->prefetch[i].ha = hax_gpfn_to_hpa(vcpu->vm,
                                                           pre_pte.raw >> 12);
                    if (!vcpu->prefetch[i].ha)
                        continue;

                    rights = 0;
                    vcpu->prefetch[i].order = PG_ORDER_4K;
                    pat = pte32_get_pat(&pre_pte);
                    vcpu->prefetch[i].flags = rights | pat << 7 |
                            (pte32_get_val(&pre_pte) & 0xf7f);

                    vcpu->prefetch[i].flag = 1;
                }
            }

            gpa_space_unmap_page(&vcpu->vm->gpa_space, &pte_kmap);

            return TF_OK;
        }
    }
    return TF_OK;
}

bool handle_vtlb(struct vcpu_t *vcpu)
{
    uint32_t access = vmx(vcpu, exit_exception_error_code);
    pagemode_t mode = vcpu_get_pagemode(vcpu);
    hax_paddr_t pdir = vcpu->state->_cr3 & (mode == PM_PAE ? ~0x1fULL : ~0xfffULL);
    hax_vaddr_t cr2 = vmx(vcpu, exit_qualification).address;

    uint32_t ret = vtlb_handle_page_fault(vcpu, mode, pdir, cr2, access);

    hax_log(HAX_LOGD, "handle vtlb fault @%llx\n", cr2);
    if (ret == 0) {
        vcpu->vmcs_pending_guest_cr3 = 1;
        return 1;
    }

    if (ret & TF_GP2HP) {
        hax_log(HAX_LOGD, "G2H translation failed (%08llx, %x)\n", cr2, access);
        return 0;
    }

    // Otherwise, inject PF into guest
    access = ret & (vcpu->state->_efer & IA32_EFER_XD ? 0x1f : 0x0f);
    vcpu->state->_cr2 = cr2;
    hax_inject_page_fault(vcpu, access);
    hax_log(HAX_LOGD, "Page fault (%08llx, %x)\n", cr2, access);

    return 1;
}

// TODO: Move these functions to another source file (e.g. mmio.c), since they
// are not specific to vTLB mode
static inline void * mmio_map_guest_virtual_page_fast(struct vcpu_t *vcpu,
                                                      uint64_t gva, int len)
{
    if (!vcpu->mmio_fetch.kva) {
        return NULL;
    }
    if ((gva >> PG_ORDER_4K) != (vcpu->mmio_fetch.last_gva >> PG_ORDER_4K) ||
        vcpu->state->_cr3 != vcpu->mmio_fetch.last_guest_cr3) {
        // Invalidate the cache
        vcpu->mmio_fetch.kva = NULL;
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &vcpu->mmio_fetch.kmap);
        if (vcpu->mmio_fetch.hit_count < 2) {
            hax_log(HAX_LOGD, "%s: Cache miss: cached_gva=0x%llx, "
                    "cached_cr3=0x%llx, gva=0x%llx, cr3=0x%llx, hits=0x%d, "
                    "vcpu_id=0x%u\n", __func__, vcpu->mmio_fetch.last_gva,
                    vcpu->mmio_fetch.last_guest_cr3, gva, vcpu->state->_cr3,
                    vcpu->mmio_fetch.hit_count, vcpu->vcpu_id);
        }
        return NULL;
    }
    // Here we assume the GVA of the MMIO instruction maps to the same guest
    // page frame that contains the previous MMIO instruction, as long as guest
    // CR3 has not changed.
    // TODO: Is it possible for a guest to modify its page tables without
    // replacing the root table (CR3) between two consecutive MMIO accesses?
    vcpu->mmio_fetch.hit_count++;
    // Skip GVA=>GPA=>KVA conversion, and just use the cached KVA
    // TODO: We do not walk the guest page tables in this case, which saves
    // time, but also means the accessed/dirty bits of the relevant guest page
    // table entries are not updated. This should be okay, since the same MMIO
    // instruction was just fetched by hardware (before this EPT violation),
    // which presumably has taken care of this matter.
    return vcpu->mmio_fetch.kva;
}

static void * mmio_map_guest_virtual_page_slow(struct vcpu_t *vcpu, uint64_t gva,
                                               hax_kmap_user *kmap)
{
    uint64_t gva_aligned = gva & pgmask(PG_ORDER_4K);
    uint64_t gpa;
    uint ret;
    void *kva;

    ret = vcpu_translate(vcpu, gva_aligned, 0, &gpa, NULL, true);
    if (ret) {
        hax_log(HAX_LOGE, "%s: vcpu_translate() returned 0x%x: vcpu_id=%u,"
                " gva=0x%llx\n", __func__, ret, vcpu->vcpu_id, gva);
        // TODO: Inject a guest page fault?
        return NULL;
    }
    hax_log(HAX_LOGD, "%s: gva=0x%llx => gpa=0x%llx, vcpu_id=0x%u\n", __func__,
            gva_aligned, gpa, vcpu->vcpu_id);

    kva = gpa_space_map_page(&vcpu->vm->gpa_space, gpa >> PG_ORDER_4K, kmap,
                             NULL);
    if (!kva) {
        hax_log(HAX_LOGE, "%s: gpa_space_map_page() failed: vcpu_id=%u, "
                "gva=0x%llx, gpa=0x%llx\n", __func__, vcpu->vcpu_id, gva, gpa);
        return NULL;
    }
    return kva;
}

int mmio_fetch_instruction(struct vcpu_t *vcpu, uint64_t gva, uint8_t *buf, int len)
{
    uint64_t end_gva;
    uint8_t *src_buf;
    uint offset;

    hax_assert(vcpu != NULL);
    hax_assert(buf != NULL);
    // A valid IA instruction is never longer than 15 bytes
    hax_assert(len > 0 && len <= 15);
    end_gva = gva + (uint)len - 1;
    if ((gva >> PG_ORDER_4K) != (end_gva >> PG_ORDER_4K)) {
        uint32_t ret;

        hax_log(HAX_LOGI, "%s: GVA range spans two pages: gva=0x%llx, len=%d\n",
                __func__, gva, len);
        ret = vcpu_read_guest_virtual(vcpu, gva, buf, (uint)len, (uint)len, 0);
        if (!ret) {
            hax_log(HAX_LOGE, "%s: vcpu_read_guest_virtual() failed: "
                    "vcpu_id=%u, gva=0x%llx, len=%d\n", __func__,
                    vcpu->vcpu_id, gva, len);
            return -ENOMEM;
        }
        return 0;
    }

    src_buf = mmio_map_guest_virtual_page_fast(vcpu, gva, len);
    if (!src_buf) {
        src_buf = mmio_map_guest_virtual_page_slow(vcpu, gva,
                                                   &vcpu->mmio_fetch.kmap);
        if (!src_buf) {
            return -ENOMEM;
        }
        vcpu->mmio_fetch.last_gva = gva;
        vcpu->mmio_fetch.last_guest_cr3 = vcpu->state->_cr3;
        vcpu->mmio_fetch.hit_count = 0;
        vcpu->mmio_fetch.kva = src_buf;
    }
    offset = (uint)(gva & pgoffs(PG_ORDER_4K));
    memcpy_s(buf, len, src_buf + offset, len);
    return 0;
}

/*
 * Read guest-linear memory.
 * If flag is 0, this read is on behalf of the guest. This function updates the
 * access/dirty bits in the guest page tables and injects a page fault if there
 * is an error. In this case, the return value is true for success, false if a
 * page fault was injected.
 * If flag is 1, this function updates the access/dirty bits in the guest page
 * tables but does not inject a page fault if there is an error. Instead, it
 * returns the number of bytes read.
 * If flag is 2, the memory read is for internal use. It does not update the
 * guest page tables. It returns the number of bytes read.
 */
uint32_t vcpu_read_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr, void *dst,
                                 uint32_t dst_buflen, uint32_t size, uint flag)
{
    // TBD: use guest CPL for access checks
    char *dstp = dst;
    uint32_t offset = 0;
    int len2;

    // Flag == 1 is not currently used, but it could be enabled if useful.
    hax_assert(flag == 0 || flag == 2);

    while (offset < size) {
        hax_paddr_t gpa;
        uint64_t len = size - offset;
        uint r = vcpu_translate(vcpu, addr + offset, 0, &gpa, &len, flag != 2);
        if (r != 0) {
            if (flag != 0)
                return offset;  // Number of bytes successfully read
            if (r & TF_GP2HP) {
                hax_log(HAX_LOGE, "read_guest_virtual(%llx, %x) failed\n",
                        addr, size);
            }
            hax_log(HAX_LOGD, "read_guest_virtual(%llx, %x) injecting #PF\n",
                    addr, size);
            vcpu->state->_cr2 = addr + offset;
            hax_inject_page_fault(vcpu, r & 0x1f);
            return false;
        }
//      if (addr + offset != gpa) {
//          hax_log(HAX_LOGI, "%s: gva=0x%llx, gpa=0x%llx, len=0x%llx\n",
//                  __func__, addr + offset, gpa, len);
//      }

        len2 = gpa_space_read_data(&vcpu->vm->gpa_space, gpa, (int)len,
                                   (uint8_t *)(dstp + offset));
        if (len2 <= 0) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC,
                    "read guest virtual error, gpa:0x%llx, len:0x%llx\n",
                    gpa, len);
            return false;
        } else {
            len = (uint64_t)len2;
        }

        offset += len;
    }

    return flag != 0 ? size : true;
}

/*
 * Write guest-linear memory.
 * If flag is 0, this memory write is on behalf of the guest. This function
 * updates the access/dirty bits in the guest page tables and injects a page
 * fault if there is an error. In this case, the return value is true for
 * success, false if a page fault was injected.
 * If flag is 1, it updates the access/dirty bits in the guest page tables but
 * does not inject a page fault if there is an error. Instead, it returns the
 * number of bytes written.
 * A flag value of 2 is implemented, but not used. It does not update the guest
 * page tables. It returns the number of bytes written.
 */
uint32_t vcpu_write_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr,
                                  uint32_t dst_buflen, const void *src, uint32_t size,
                                  uint flag)
{
    // TODO: use guest CPL for access checks
    const char *srcp = src;
    uint32_t offset = 0;
    int len2;

    hax_assert(flag == 0 || flag == 1);
    hax_assert(dst_buflen >= size);

    while (offset < size) {
        hax_paddr_t gpa;
        uint64_t len = size - offset;
        uint r = vcpu_translate(vcpu, addr + offset, TF_WRITE, &gpa, &len,
                                flag != 2);
        if (r != 0) {
            if (flag != 0)
                return offset;  // Number of bytes successfully written
            if (r & TF_GP2HP) {
                vcpu_set_panic(vcpu);
                hax_log(HAX_LOGPANIC, "write_guest_virtual(%llx, %x) failed\n",
                        addr, size);
            }
            hax_log(HAX_LOGD, "write_guest_virtual(%llx, %x) injecting #PF\n",
                    addr, size);
            vcpu->state->_cr2 = addr + offset;
            hax_inject_page_fault(vcpu, r & 0x1f);
            return false;
        }

        len2 = (uint64_t)gpa_space_write_data(&vcpu->vm->gpa_space, gpa, len,
                                            (uint8_t *)(srcp + offset));
        if (len2 <= 0) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC,
                    "write guest virtual error, gpa:0x%llx, len:0x%llx\n",
                    gpa, len);
            return false;
        } else {
            len = len2;
        }

        offset += len;
    }

    return flag != 0 ? size : true;
}

/*
 * Guest virtual to guest physical address translation.
 * @param va        Guest virtual address
 * @param access    Access descriptor (read/write, user/supervisor)
 * @param pa        Guest physical address
 * @param len       Number of bytes for which translation is valid
 * @param update    Update access and dirty bits of guest structures
 * @returns 0 if translation is successful, 0x80000000 OR'ed with the exception
 * number otherwise.
 */
uint vcpu_translate(struct vcpu_t *vcpu, hax_vaddr_t va, uint access, hax_paddr_t *pa,
                    uint64_t *len, bool update)
{
    pagemode_t mode = vcpu_get_pagemode(vcpu);
    uint order = 0;
    uint r = -1;

    hax_log(HAX_LOGD, "vcpu_translate: %llx (%s,%s) mode %u\n", va,
            access & TF_WRITE ? "W" : "R", access & TF_USER ? "U" : "S", mode);

    switch (mode) {
        case PM_FLAT: {
            // Non-paging mode, no further actions.
            *pa = va;
            r = 0;
            break;
        }
        case PM_2LVL:
        case PM_PAE:
        case PM_PML4: {
            r = pw_perform_page_walk(vcpu, va, access, pa, &order, update,
                                     false);
            break;
        }
        default: {
            // Should never happen
            break;
        }
    }

    if (r == 0) {
        /*
         * Translation is guaranteed valid until the end of 4096 bytes page
         * (the minimum page size) due possible EPT remapping for the bigger
         * translation units
         */
        uint64_t size = (uint64_t)1 << PG_ORDER_4K;
        uint64_t extend = size - (va & (size - 1));

        // Adjust validity of translation if necessary.
        if (len != NULL && (*len == 0 || *len > extend)) {
            *len = extend;
        }
    }
    return r;
}

pagemode_t vcpu_get_pagemode(struct vcpu_t *vcpu)
{
    if (!(vcpu->state->_cr0 & CR0_PG))
        return PM_FLAT;

    if (!(vcpu->state->_cr4 & CR4_PAE))
        return PM_2LVL;

    // Only support pure 32-bit paging. May support PAE paging in future.
    // hax_assert(0);
    if (!(vcpu->state->_efer & IA32_EFER_LMA))
        return PM_PAE;

    return PM_PML4;
}

void vcpu_invalidate_tlb(struct vcpu_t *vcpu, bool global)
{
    if (global) {
        vcpu->mmu->igo = false;
    }
    vtlb_invalidate(vcpu->mmu);
}

void vcpu_invalidate_tlb_addr(struct vcpu_t *vcpu, hax_vaddr_t va)
{
    vtlb_invalidate_addr(vcpu->mmu, va);
}
