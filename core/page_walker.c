/*
 * Copyright (c) 2013 Intel Corporation
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

typedef union PW_PAGE_ENTRY_U {
    union {
        struct {
            uint32_t present          : 1,
                   writable         : 1,
                   user             : 1,
                   pwt              : 1,
                   pcd              : 1,
                   accessed         : 1,
                   dirty            : 1,
                   _page_size       : 1,
                   global           : 1,
                   available        : 3,
                   addr_base        : 20;
        } bits;
        uint32_t val;
    } non_pae_entry;
    union {
        struct {
            uint32_t present          : 1,
                   writable         : 1,
                   user             : 1,
                   pwt              : 1,
                   pcd              : 1,
                   accessed         : 1,
                   dirty            : 1,
                   _page_size       : 1,
                   global           : 1,
                   available        : 3,
                   addr_base_low    : 20;
            uint32_t addr_base_high   : 20,
                   avl_or_res       : 11,
                   exb_or_res       : 1;
        } bits;
        uint64_t val;
    } pae_lme_entry;
} PW_PAGE_ENTRY;

typedef union PW_PFEC_U {
    struct {
        uint32_t present              : 1,
               is_write             : 1,
               is_user              : 1,
               is_reserved          : 1,
               is_fetch             : 1,
               reserved             : 27;
        uint32_t reserved_high;
    } bits;
    uint64_t val;
} PW_PFEC;

#define PW_NUM_OF_TABLE_ENTRIES_IN_PAE_MODE 512
#define PW_NUM_OF_TABLE_ENTRIES_IN_NON_PAE_MODE 1024
#define PW_INVALID_INDEX ((uint32_t)(~(0)));
#define PW_PAE_ENTRY_INCREMENT 8
#define PW_NON_PAE_ENTRY_INCREMENT 4
#define PW_PDPTE_INDEX_MASK_IN_32_BIT_ADDR 0xc0000000
#define PW_PDPTE_INDEX_SHIFT 30
#define PW_PDPTE_INDEX_MASK_IN_64_BIT_ADDR ((uint64_t)0x0000007fc0000000)
#define PW_PML4TE_INDEX_MASK ((uint64_t)0x0000ff8000000000)
#define PW_PML4TE_INDEX_SHIFT 39
#define PW_PDE_INDEX_MASK_IN_PAE_MODE 0x3fe00000
#define PW_PDE_INDEX_SHIFT_IN_PAE_MODE 21
#define PW_PDE_INDEX_MASK_IN_NON_PAE_MODE 0xffc00000
#define PW_PDE_INDEX_SHIFT_IN_NON_PAE_MODE 22
#define PW_PTE_INDEX_MASK_IN_PAE_MODE 0x1ff000
#define PW_PTE_INDEX_MASK_IN_NON_PAE_MODE 0x3ff000
#define PW_PTE_INDEX_SHIFT 12
#define PW_PDPT_ALIGNMENT 32
#define PW_TABLE_SHIFT 12
#define PW_HIGH_ADDRESS_SHIFT 32
#define PW_2M_PAE_PDE_RESERVED_BITS_IN_ENTRY_LOW_MASK ((uint32_t)0x1fe000)
#define PW_4M_NON_PAE_PDE_RESERVED_BITS_IN_ENTRY_LOW_MASK ((uint32_t)0x3fe000)
#define PW_1G_PAE_PDPTE_RESERVED_BITS_IN_ENTRY_LOW_MASK ((uint32_t)0x3fffe000)

uint32_t pw_reserved_bits_high_mask;

static inline uint64_t pw_retrieve_table_from_cr3(uint64_t cr3, bool is_pae,
                                                bool is_lme)
{
    if (!is_pae || is_lme)
        return ALIGN_BACKWARD(cr3, PAGE_SIZE_4K);

    return ALIGN_BACKWARD(cr3, PW_PDPT_ALIGNMENT);
}

static void pw_retrieve_indices(IN uint64_t virtual_address, IN bool is_pae,
                                IN bool is_lme, OUT uint32_t *pml4te_index,
                                OUT uint32_t *pdpte_index, OUT uint32_t *pde_index,
                                OUT uint32_t *pte_index)
{
    uint32_t virtual_address_low_32_bit = (uint32_t)virtual_address;

    if (is_pae) {
        if (is_lme) {
            uint64_t pml4te_index_tmp = (virtual_address & PW_PML4TE_INDEX_MASK)
                                      >> PW_PML4TE_INDEX_SHIFT;
            uint64_t pdpte_index_tmp =
                    (virtual_address & PW_PDPTE_INDEX_MASK_IN_64_BIT_ADDR)
                    >> PW_PDPTE_INDEX_SHIFT;

            *pml4te_index = (uint32_t)pml4te_index_tmp;
            *pdpte_index = (uint32_t)pdpte_index_tmp;
        } else {
            *pml4te_index = PW_INVALID_INDEX;
            *pdpte_index = (virtual_address_low_32_bit &
                            PW_PDPTE_INDEX_MASK_IN_32_BIT_ADDR)
                           >> PW_PDPTE_INDEX_SHIFT;
            hax_assert(*pdpte_index < PW_NUM_OF_PDPT_ENTRIES_IN_32_BIT_MODE);
        }
        *pde_index = (virtual_address_low_32_bit &
                      PW_PDE_INDEX_MASK_IN_PAE_MODE)
                     >> PW_PDE_INDEX_SHIFT_IN_PAE_MODE;
        hax_assert(*pde_index < PW_NUM_OF_TABLE_ENTRIES_IN_PAE_MODE);
        *pte_index = (virtual_address_low_32_bit &
                      PW_PTE_INDEX_MASK_IN_PAE_MODE)
                     >> PW_PTE_INDEX_SHIFT;
        hax_assert(*pte_index < PW_NUM_OF_TABLE_ENTRIES_IN_PAE_MODE);
    } else {
        *pml4te_index = PW_INVALID_INDEX;
        *pdpte_index = PW_INVALID_INDEX;
        *pde_index = (virtual_address_low_32_bit &
                      PW_PDE_INDEX_MASK_IN_NON_PAE_MODE)
                     >> PW_PDE_INDEX_SHIFT_IN_NON_PAE_MODE;
        *pte_index = (virtual_address_low_32_bit &
                      PW_PTE_INDEX_MASK_IN_NON_PAE_MODE)
                     >> PW_PTE_INDEX_SHIFT;
    }
}

static PW_PAGE_ENTRY * pw_retrieve_table_entry(
        struct vcpu_t *vcpu, void *table_hva, uint32_t entry_index, bool is_pae)
{
    uint64_t entry_hva;

    if (is_pae) {
        entry_hva = (uint64_t)table_hva + entry_index * PW_PAE_ENTRY_INCREMENT;
    } else {
        entry_hva = (uint64_t)table_hva
                    + entry_index * PW_NON_PAE_ENTRY_INCREMENT;
    }

    return (PW_PAGE_ENTRY *)entry_hva;
}

static void pw_read_entry_value(
        PW_PAGE_ENTRY *fill_to, PW_PAGE_ENTRY *fill_from, bool is_pae)
{
    if (is_pae) {
        volatile uint64_t *original_value_ptr = (volatile uint64_t *)fill_from;
        uint64_t value1 = *original_value_ptr;
        uint64_t value2 = *original_value_ptr;

        while (value1 != value2) {
            value1 = value2;
            value2 = *original_value_ptr;
        }

        *fill_to = *((PW_PAGE_ENTRY *)(&value1));
    } else {
        fill_to->pae_lme_entry.val = 0; // Clear the whole entry
        fill_to->non_pae_entry.val = fill_from->non_pae_entry.val;
    }
}

static bool pw_is_big_page_pde(PW_PAGE_ENTRY *entry, bool is_lme, bool is_pae,
                               bool is_pse)
{
    // Doesn't matter which type "non_pae" or "pae_lme"
    if (!entry->non_pae_entry.bits._page_size)
        return false;

    // Ignore pse bit in these cases
    if (is_lme || is_pae)
        return true;

    return is_pse;
}

static inline bool pw_is_1gb_page_pdpte(PW_PAGE_ENTRY *entry)
{
    return entry->pae_lme_entry.bits._page_size;
}

static bool pw_are_reserved_bits_in_pml4te_cleared(PW_PAGE_ENTRY *entry,
                                                   bool is_nxe)
{
    if (entry->pae_lme_entry.bits.addr_base_high & pw_reserved_bits_high_mask)
        return false;

    if (!is_nxe && entry->pae_lme_entry.bits.exb_or_res)
        return false;

    return true;
}

static bool pw_are_reserved_bits_in_pdpte_cleared(PW_PAGE_ENTRY *entry,
                                                  bool is_nxe, bool is_lme)
{
    if (entry->pae_lme_entry.bits.addr_base_high & pw_reserved_bits_high_mask)
        return false;

    if (!is_lme) {
        if (entry->pae_lme_entry.bits.avl_or_res ||
            entry->pae_lme_entry.bits.exb_or_res ||
            entry->pae_lme_entry.bits.writable ||
            entry->pae_lme_entry.bits.user)
            return false;
    } else {
        if (!is_nxe && entry->pae_lme_entry.bits.exb_or_res)
            return false;

        if (pw_is_1gb_page_pdpte(entry)) {
            if (entry->pae_lme_entry.val &
                PW_1G_PAE_PDPTE_RESERVED_BITS_IN_ENTRY_LOW_MASK)
                return false;
        }
    }

    return true;
}

static bool pw_are_reserved_bits_in_pde_cleared(
        PW_PAGE_ENTRY *entry, bool is_nxe, bool is_lme, bool is_pae,
        bool is_pse)
{
    if (is_pae) {
        if (entry->pae_lme_entry.bits.addr_base_high &
            pw_reserved_bits_high_mask)
            return false;

        if (!is_nxe && entry->pae_lme_entry.bits.exb_or_res)
            return false;

        if (!is_lme && entry->pae_lme_entry.bits.avl_or_res)
            return false;

        if (pw_is_big_page_pde(entry, is_lme, is_pae, is_pse)) {
            if (entry->pae_lme_entry.val &
                PW_2M_PAE_PDE_RESERVED_BITS_IN_ENTRY_LOW_MASK)
                return false;
        }
    } else if (pw_is_big_page_pde(entry, is_lme, is_pae, is_pse) &&
               entry->non_pae_entry.val &
               PW_4M_NON_PAE_PDE_RESERVED_BITS_IN_ENTRY_LOW_MASK)
        return false;

    return true;
}

static bool pw_are_reserved_bits_in_pte_cleared(
        PW_PAGE_ENTRY *pte, bool is_nxe, bool is_lme, bool is_pae)
{
    if (!is_pae)
        return true;

    if (pte->pae_lme_entry.bits.addr_base_high & pw_reserved_bits_high_mask)
        return false;

    if (!is_lme && pte->pae_lme_entry.bits.avl_or_res)
        return false;

    if (!is_nxe && pte->pae_lme_entry.bits.exb_or_res)
        return false;

    return true;
}

static bool pw_is_write_access_permitted(
        PW_PAGE_ENTRY *pml4te, PW_PAGE_ENTRY *pdpte, PW_PAGE_ENTRY *pde,
        PW_PAGE_ENTRY *pte, bool is_user, bool is_wp, bool is_lme, bool is_pae,
        bool is_pse)
{
    if (!is_user && !is_wp)
        return true;

    if (is_lme) {
        hax_assert(pml4te != NULL);
        hax_assert(pdpte != NULL);
        hax_assert(pml4te->pae_lme_entry.bits.present);
        hax_assert(pdpte->pae_lme_entry.bits.present);
        if (!pml4te->pae_lme_entry.bits.writable ||
            !pdpte->pae_lme_entry.bits.writable)
            return false;
    }

    if (pw_is_1gb_page_pdpte(pdpte))
        return true;

    hax_assert(pde != NULL);
    hax_assert(pde->non_pae_entry.bits.present);
    // Doesn't matter which entry "non_pae" or "pae_lme" is checked
    if (!pde->non_pae_entry.bits.writable)
        return false;

    if (pw_is_big_page_pde(pde, is_lme, is_pae, is_pse))
        return true;

    hax_assert(pte != NULL);
    hax_assert(pte->non_pae_entry.bits.present);

    // Doesn't matter which entry "non_pae" or "pae_lme" is checked
    return pte->non_pae_entry.bits.writable;
}

static bool pw_is_user_access_permitted(
        PW_PAGE_ENTRY *pml4te, PW_PAGE_ENTRY *pdpte, PW_PAGE_ENTRY *pde,
        PW_PAGE_ENTRY *pte, bool is_lme, bool is_pae, bool is_pse)
{
    if (is_lme) {
        hax_assert(pml4te != NULL);
        hax_assert(pdpte != NULL);
        hax_assert(pml4te->pae_lme_entry.bits.present);
        hax_assert(pdpte->pae_lme_entry.bits.present);
        if (!pml4te->pae_lme_entry.bits.user ||
            !pdpte->pae_lme_entry.bits.user)
            return false;
    }

    if (pw_is_1gb_page_pdpte(pdpte))
        return true;

    hax_assert(pde != NULL);
    hax_assert(pde->non_pae_entry.bits.present);
    // Doesn't matter which entry "non_pae" or "pae_lme" is checked
    if (!pde->non_pae_entry.bits.user)
        return false;

    if (pw_is_big_page_pde(pde, is_lme, is_pae, is_pse))
        return true;

    hax_assert(pte != NULL);
    hax_assert(pte->non_pae_entry.bits.present);

    // Doesn't matter which entry "non_pae" or "pae_lme" is checked
    return (pte->non_pae_entry.bits.user);
}

static bool pw_is_fetch_access_permitted(
        PW_PAGE_ENTRY *pml4te, PW_PAGE_ENTRY *pdpte, PW_PAGE_ENTRY *pde,
        PW_PAGE_ENTRY *pte, bool is_lme, bool is_pae, bool is_pse)
{
    if (is_lme) {
        hax_assert(pml4te != NULL);
        hax_assert(pdpte != NULL);
        hax_assert(pml4te->pae_lme_entry.bits.present);
        hax_assert(pdpte->pae_lme_entry.bits.present);

        if (pml4te->pae_lme_entry.bits.exb_or_res ||
            pdpte->pae_lme_entry.bits.exb_or_res)
            return false;
    }

    if (pw_is_1gb_page_pdpte(pdpte))
        return true;

    hax_assert(pde != NULL);
    hax_assert(pde->pae_lme_entry.bits.present);
    if (pde->pae_lme_entry.bits.exb_or_res)
        return false;

    if (pw_is_big_page_pde(pde, is_lme, is_pae, is_pse))
        return true;

    hax_assert(pte != NULL);
    hax_assert(pte->pae_lme_entry.bits.present);

    return !pte->pae_lme_entry.bits.exb_or_res;
}

static uint64_t pw_retrieve_phys_addr(PW_PAGE_ENTRY *entry, bool is_pae)
{
    hax_assert(entry->non_pae_entry.bits.present);
    if (is_pae) {
        uint32_t addr_low = entry->pae_lme_entry.bits.addr_base_low
                          << PW_TABLE_SHIFT;
        uint32_t addr_high = entry->pae_lme_entry.bits.addr_base_high;
        return ((uint64_t)addr_high << PW_HIGH_ADDRESS_SHIFT) | addr_low;
    }

    // Must convert the uint32_t bit-field to uint64_t before the shift. Otherwise,
    // on Mac the shift result will be sign-extended to 64 bits (Clang bug?),
    // yielding invalid GPAs such as 0xffffffff801c8000.
    return (uint64_t)entry->non_pae_entry.bits.addr_base << PW_TABLE_SHIFT;
}

static uint64_t pw_retrieve_big_page_phys_addr(PW_PAGE_ENTRY *entry, bool is_pae,
                                             bool is_1gb)
{
    uint64_t base = pw_retrieve_phys_addr(entry, is_pae);

    // Clean offset bits
    if (is_pae) {
        if (is_1gb)
            return ALIGN_BACKWARD(base, PAGE_SIZE_1G);

        return ALIGN_BACKWARD(base, PAGE_SIZE_2M);
    }

    // Non-PAE mode
    return ALIGN_BACKWARD(base, PAGE_SIZE_4M);
}

static uint32_t pw_get_big_page_offset(uint64_t virtual_address, bool is_pae,
                                     bool is_1gb)
{
    if (is_pae) {
        if (is_1gb) {
            // Take only 30 LSBs
            return (uint32_t)(virtual_address & PAGE_1GB_MASK);
        }
        // Take only 21 LSBs
        return (uint32_t)(virtual_address & PAGE_2MB_MASK);
    }

    // Take 22 LSBs
    return (uint32_t)(virtual_address & PAGE_4MB_MASK);
}

static void pw_update_ad_bits_in_entry(PW_PAGE_ENTRY *native_entry,
                                       PW_PAGE_ENTRY *old_native_value,
                                       PW_PAGE_ENTRY *new_native_value)
{
    hax_assert(native_entry != NULL);
    hax_assert(old_native_value->non_pae_entry.bits.present);
    hax_assert(new_native_value->non_pae_entry.bits.present);

    if (old_native_value->non_pae_entry.val !=
        new_native_value->non_pae_entry.val) {
        hax_cmpxchg64(old_native_value->non_pae_entry.val,
                      new_native_value->non_pae_entry.val,
                      (volatile uint64_t *)native_entry);
        // hw_interlocked_compare_exchange((int32_t volatile *)native_entry,
        //                                 old_native_value->non_pae_entry.val,
        //                                 new_native_value->non_pae_entry.val);
        // The result is not checked. If the cmpxchg has failed, it means that
        // the guest entry was changed, so it is wrong to set status bits on the
        // updated entry.
    }
}

static void pw_update_ad_bits(
        PW_PAGE_ENTRY *guest_space_pml4te, PW_PAGE_ENTRY *pml4te,
        PW_PAGE_ENTRY *guest_space_pdpte, PW_PAGE_ENTRY *pdpte,
        PW_PAGE_ENTRY *guest_space_pde, PW_PAGE_ENTRY *pde,
        PW_PAGE_ENTRY *guest_space_pte, PW_PAGE_ENTRY *pte,
        bool is_write_access, bool is_lme, bool is_pae, bool is_pse)
{
    PW_PAGE_ENTRY pde_before_update;
    PW_PAGE_ENTRY pte_before_update;

    if (is_lme) {
        PW_PAGE_ENTRY pml4te_before_update;
        PW_PAGE_ENTRY pdpte_before_update;

        hax_assert(guest_space_pml4te != NULL);
        hax_assert(pml4te != NULL);
        hax_assert(guest_space_pdpte != NULL);
        hax_assert(pdpte != NULL);

        pml4te_before_update = *pml4te;
        pml4te->pae_lme_entry.bits.accessed = 1;
        pw_update_ad_bits_in_entry(guest_space_pml4te, &pml4te_before_update,
                                   pml4te);

        pdpte_before_update = *pdpte;
        pdpte->pae_lme_entry.bits.accessed = 1;

        if (guest_space_pml4te == guest_space_pdpte) {
            pdpte_before_update.pae_lme_entry.bits.accessed = 1;
        }
        pw_update_ad_bits_in_entry(guest_space_pdpte, &pdpte_before_update,
                                   pdpte);
    }

    if (pw_is_1gb_page_pdpte(pdpte))
        return;

    hax_assert(guest_space_pde != NULL);
    hax_assert(pde != NULL);

    pde_before_update = *pde;
    // Doesn't matter which field "non_pae" or "pae_lme" is used
    pde->non_pae_entry.bits.accessed = 1;
    if ((guest_space_pml4te == guest_space_pde) ||
        (guest_space_pdpte == guest_space_pde)) {
        // Doesn't matter which field "non_pae" or "pae_lme" is used
        pde_before_update.non_pae_entry.bits.accessed = 1;
    }

    if (pw_is_big_page_pde(pde, is_lme, is_pae, is_pse)) {
        if (is_write_access) {
            // Doesn't matter which field "non_pae" or "pae_lme" is used
            pde->non_pae_entry.bits.dirty = 1;
        }
        pw_update_ad_bits_in_entry(guest_space_pde, &pde_before_update, pde);
        return;
    }

    pw_update_ad_bits_in_entry(guest_space_pde, &pde_before_update, pde);

    hax_assert(guest_space_pte != NULL);
    hax_assert(pte != NULL);

    pte_before_update = *pte;
    // Doesn't matter which field "non_pae" or "pae_lme" is used
    pte->non_pae_entry.bits.accessed = 1;

    if ((guest_space_pml4te == guest_space_pte) ||
        (guest_space_pdpte == guest_space_pte) ||
        (guest_space_pde == guest_space_pte)) {
        // Doesn't matter which field "non_pae" or "pae_lme" is used
        pte_before_update.non_pae_entry.bits.accessed = 1;
    }

    if (is_write_access) {
        // Doesn't matter which field "non_pae" or "pae_lme" is used
        pte->non_pae_entry.bits.dirty = 1;
    }

    pw_update_ad_bits_in_entry(guest_space_pte, &pte_before_update, pte);
}

uint32_t pw_perform_page_walk(
        IN struct vcpu_t *vcpu, IN uint64_t virt_addr, IN uint32_t access,
        OUT uint64_t *gpa_out, OUT uint *order, IN bool set_ad_bits,
        IN bool is_fetch)
{
    uint32_t retval = TF_OK;
    uint64_t efer_value = vcpu->state->_efer;
    bool is_nxe = ((efer_value & IA32_EFER_XD) != 0);
    bool is_lme = ((efer_value & IA32_EFER_LME) != 0);
    uint64_t cr0 = vcpu->state->_cr0;
    uint64_t cr3 = vcpu->state->_cr3;
    uint64_t cr4 = vcpu->state->_cr4;
    bool is_wp = ((cr0 & CR0_WP) != 0);
    bool is_pae = ((cr4 & CR4_PAE) != 0);
    bool is_pse = ((cr4 & CR4_PSE) != 0);

    uint64_t gpa = PW_INVALID_GPA;
    uint64_t first_table;

    PW_PAGE_ENTRY *pml4te_ptr, *pdpte_ptr, *pde_ptr, *pte_ptr = NULL;
    PW_PAGE_ENTRY pml4te_val, pdpte_val, pde_val, pte_val;
    void *pml4t_hva, *pdpt_hva, *pd_hva, *pt_hva;
    hax_kmap_user pml4t_kmap, pdpt_kmap, pd_kmap, pt_kmap;
    uint64_t pml4t_gpa, pdpt_gpa, pd_gpa, pt_gpa;
    uint32_t pml4te_index, pdpte_index, pde_index, pte_index;
    bool is_write, is_user;

    pml4te_ptr = pdpte_ptr = pde_ptr = NULL;
    pml4t_hva = pdpt_hva = pd_hva = pt_hva = NULL;

    pml4te_val.pae_lme_entry.val = 0;
    pdpte_val.pae_lme_entry.val = 0;
    pde_val.pae_lme_entry.val = 0;
    pte_val.pae_lme_entry.val = 0;

    is_write = access & TF_WRITE;
    is_user  = access & TF_USER;

    pw_retrieve_indices(virt_addr, is_pae, is_lme, &pml4te_index, &pdpte_index,
                        &pde_index, &pte_index);

    first_table = pw_retrieve_table_from_cr3(cr3, is_pae, is_lme);

    if (is_pae) {
        uint8_t *pdpt_page_hva;

        if (is_lme) {
            pml4t_gpa = first_table;
            pml4t_hva = gpa_space_map_page(&vcpu->vm->gpa_space,
                                           pml4t_gpa >> PG_ORDER_4K,
                                           &pml4t_kmap, NULL);

            if (pml4t_hva == NULL) {
                retval = TF_FAILED;
                goto out;
            }

            pml4te_ptr = pw_retrieve_table_entry(vcpu, pml4t_hva, pml4te_index,
                                                 is_pae);
            pw_read_entry_value(&pml4te_val, pml4te_ptr, is_pae);

            if (!pml4te_val.pae_lme_entry.bits.present) {
                retval = TF_FAILED | access;
                goto out;
            }

            if (!pw_are_reserved_bits_in_pml4te_cleared(&pml4te_val, is_nxe)) {
                retval = TF_FAILED | TF_PROTECT | TF_RSVD | access;
                goto out;
            }

            pdpt_gpa = pw_retrieve_phys_addr(&pml4te_val, is_pae);
        } else {
            pdpt_gpa = first_table;
        }

        pdpt_page_hva = gpa_space_map_page(&vcpu->vm->gpa_space,
                                           pdpt_gpa >> PG_ORDER_4K,
                                           &pdpt_kmap, NULL);

        if (pdpt_page_hva == NULL) {
            retval = TF_FAILED;
            goto out;
        }

        // In PAE paging mode, pdpt_gpa is 32-byte aligned, not 4KB-aligned
        pdpt_hva = pdpt_page_hva + (uint)(pdpt_gpa & (PAGE_SIZE_4K - 1));

        pdpte_ptr = pw_retrieve_table_entry(vcpu, pdpt_hva, pdpte_index,
                                            is_pae);
        pw_read_entry_value(&pdpte_val, pdpte_ptr, is_pae);

        if (!pdpte_val.pae_lme_entry.bits.present) {
            retval = TF_FAILED | access;
            goto out;
        }

        if (!pw_are_reserved_bits_in_pdpte_cleared(&pdpte_val, is_nxe,
                                                   is_lme)) {
            retval = TF_FAILED | TF_PROTECT | TF_RSVD | access;
            goto out;
        }
    }

    // 1GB page size
    if (pw_is_1gb_page_pdpte(&pdpte_val)) {
        uint64_t big_page_addr;
        uint32_t offset_in_big_page;

        *order = PG_ORDER_1G;
        // Retrieve address of the big page in guest space
        big_page_addr = pw_retrieve_big_page_phys_addr(&pdpte_val, is_pae,
                                                       true);
        // Retrieve offset in page
        offset_in_big_page = pw_get_big_page_offset(virt_addr, is_pae, true);
        // Calculate full guest accessed physical address
        gpa = big_page_addr + offset_in_big_page;

        if (is_write && !pw_is_write_access_permitted(
                &pml4te_val, &pdpte_val, NULL, NULL, is_user, is_wp, is_lme,
                is_pae, is_pse)) {
            retval = TF_FAILED | TF_PROTECT | access;
            goto out;
        }

        if (is_user && !pw_is_user_access_permitted(&pml4te_val, &pdpte_val,
                NULL, NULL, is_lme, is_pae, is_pse)) {
            retval = TF_FAILED | TF_PROTECT | access;
            goto out;
        }

        if (is_pae && is_nxe && is_fetch && !pw_is_fetch_access_permitted(
                &pml4te_val, &pdpte_val, NULL, NULL, is_lme, is_pae, is_pse)) {
            retval = TF_FAILED | TF_PROTECT | access;
            goto out;
        }

        if (set_ad_bits) {
            pw_update_ad_bits(pml4te_ptr, &pml4te_val, pdpte_ptr, &pdpte_val,
                              NULL, NULL, NULL, NULL, is_write, is_lme, is_pae,
                              is_pse);
        }

        // Page walk succeeded
        goto out;
    }

    pd_gpa = is_pae ? pw_retrieve_phys_addr(&pdpte_val, is_pae) : first_table;
    pd_hva = gpa_space_map_page(&vcpu->vm->gpa_space, pd_gpa >> PG_ORDER_4K,
                                &pd_kmap, NULL);

    if (pd_hva == NULL) {
        retval = TF_FAILED;
        goto out;
    }

    pde_ptr = pw_retrieve_table_entry(vcpu, pd_hva, pde_index, is_pae);
    pw_read_entry_value(&pde_val, pde_ptr, is_pae);

    // Doesn't matter which entry "non_pae" or "pae" is checked
    if (!pde_val.non_pae_entry.bits.present) {
        retval = TF_FAILED | access;
        goto out;
    }

    if (!pw_are_reserved_bits_in_pde_cleared(&pde_val, is_nxe, is_lme, is_pae,
                                             is_pse)) {
        retval = TF_FAILED | TF_PROTECT | TF_RSVD | access;
        goto out;
    }

    // 2MB, 4MB page size
    if (pw_is_big_page_pde(&pde_val, is_lme, is_pae, is_pse)) {
        uint64_t big_page_addr;
        uint32_t offset_in_big_page = 0;

        *order = is_pae ? PG_ORDER_2M : PG_ORDER_4M;

        // Retrieve address of the big page in guest space
        big_page_addr = pw_retrieve_big_page_phys_addr(&pde_val, is_pae, false);
        // Retrieve offset in page
        offset_in_big_page = pw_get_big_page_offset(virt_addr, is_pae, false);
        // Calculate full guest accessed physical address
        gpa = big_page_addr + offset_in_big_page;

        if (is_write && !pw_is_write_access_permitted(
                &pml4te_val, &pdpte_val, &pde_val, NULL, is_user, is_wp, is_lme,
                is_pae, is_pse)) {
            retval = TF_FAILED | TF_PROTECT | access;
            goto out;
        }

        if (is_user && !pw_is_user_access_permitted(
                &pml4te_val, &pdpte_val, &pde_val, NULL, is_lme, is_pae,
                is_pse)) {
            retval = TF_FAILED | TF_PROTECT | access;
            goto out;
        }

        if (is_pae && is_nxe && is_fetch && !pw_is_fetch_access_permitted(
                &pml4te_val, &pdpte_val, &pde_val, NULL, is_lme, is_pae,
                is_pse)) {
            retval = TF_FAILED | TF_PROTECT | access;
            goto out;
        }

        if (set_ad_bits) {
            pw_update_ad_bits(pml4te_ptr, &pml4te_val, pdpte_ptr, &pdpte_val,
                              pde_ptr, &pde_val, NULL, NULL, is_write, is_lme,
                              is_pae, is_pse);
        }

        // Page walk succeeded
        goto out;
    }

    // 4KB page size
    *order = PG_ORDER_4K;
    pt_gpa = pw_retrieve_phys_addr(&pde_val, is_pae);
    pt_hva = gpa_space_map_page(&vcpu->vm->gpa_space, pt_gpa >> 12, &pt_kmap,
                                NULL);

    if (pt_hva == NULL) {
        retval = TF_FAILED;
        goto out;
    }

    pte_ptr = pw_retrieve_table_entry(vcpu, pt_hva, pte_index, is_pae);
    pw_read_entry_value(&pte_val, pte_ptr, is_pae);

    // Retrieve GPA of guest PT
    gpa = pw_retrieve_phys_addr(&pte_val, is_pae);
    gpa |= (virt_addr & PAGE_4KB_MASK); // Add offset

    // Doesn't matter which field "non_pae" of "pae_lme" is used
    if (!pte_val.non_pae_entry.bits.present) {
        retval = TF_FAILED | access;
        goto out;
    }

    if (!pw_are_reserved_bits_in_pte_cleared(&pte_val, is_nxe, is_lme,
                                             is_pae)) {
        retval = TF_FAILED | TF_PROTECT | TF_RSVD | access;
        goto out;
    }

    if (is_write && !pw_is_write_access_permitted(
            &pml4te_val, &pdpte_val, &pde_val, &pte_val, is_user, is_wp, is_lme,
            is_pae, is_pse)) {
        retval = TF_FAILED | TF_PROTECT | access;
        goto out;
    }

    if (is_user && !pw_is_user_access_permitted(
            &pml4te_val, &pdpte_val, &pde_val, &pte_val, is_lme, is_pae,
            is_pse)) {
        retval = TF_FAILED | TF_PROTECT | access;
        goto out;
    }

    if (is_pae && is_nxe && is_fetch && !pw_is_fetch_access_permitted(
            &pml4te_val, &pdpte_val, &pde_val, &pte_val, is_lme, is_pae,
            is_pse)) {
        retval = TF_FAILED | TF_PROTECT | access;
        goto out;
    }

    if (set_ad_bits) {
        pw_update_ad_bits(pml4te_ptr, &pml4te_val, pdpte_ptr, &pdpte_val,
                          pde_ptr, &pde_val, pte_ptr, &pte_val, is_write,
                          is_lme, is_pae, is_pse);
    }
    // page walk succeeded

out:
    if (pml4t_hva != NULL) {
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &pml4t_kmap);
    }
    if (pdpt_hva != NULL) {
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &pdpt_kmap);
    }
    if (pd_hva != NULL) {
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &pd_kmap);
    }
    if (pt_hva != NULL) {
        gpa_space_unmap_page(&vcpu->vm->gpa_space, &pt_kmap);
    }
    if (gpa_out != NULL) {
        *gpa_out = gpa;
    }

    return retval;
}
