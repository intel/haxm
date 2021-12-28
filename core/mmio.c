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

#include "mmio.h"

#include "ia32_defs.h"
#include "intr.h"
#include "page_walker.h"
#include "paging.h"

typedef uint32_t pagemode_t;

static void * mmio_map_guest_virtual_page_fast(struct vcpu_t *vcpu,
                                               uint64_t gva, int len);
static void * mmio_map_guest_virtual_page_slow(struct vcpu_t *vcpu,
                                               uint64_t gva,
                                               hax_kmap_user *kmap);
static pagemode_t vcpu_get_pagemode(struct vcpu_t *vcpu);

int mmio_fetch_instruction(struct vcpu_t *vcpu, uint64_t gva, uint8_t *buf,
                           int len)
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
                    "vcpu_id=%u, gva=0x%llx, len=%d\n", __func__, vcpu->vcpu_id,
                    gva, len);
            return -ENOMEM;
        }

        return 0;
    }

    src_buf = mmio_map_guest_virtual_page_fast(vcpu, gva, len);
    if (!src_buf) {
        src_buf = mmio_map_guest_virtual_page_slow(vcpu, gva,
                                                   &vcpu->mmio_fetch.kmap);
        if (!src_buf)
            return -ENOMEM;

        vcpu->mmio_fetch.last_gva        = gva;
        vcpu->mmio_fetch.last_guest_cr3  = vcpu->state->_cr3;
        vcpu->mmio_fetch.hit_count       = 0;
        vcpu->mmio_fetch.kva             = src_buf;
    }
    offset = (uint)(gva & pgoffs(PG_ORDER_4K));
    memcpy_s(buf, len, src_buf + offset, len);

    return 0;
}

uint vcpu_translate(struct vcpu_t *vcpu, hax_vaddr_t va, uint access,
                    hax_paddr_t *pa, uint64_t *len, bool update)
{
    pagemode_t mode = vcpu_get_pagemode(vcpu);
    uint order = 0;
    uint r = -1;

    hax_log(HAX_LOGD, "%s: vcpu_translate: %llx (%s,%s) mode %u\n", __func__,
            va, access & TF_WRITE ? "W" : "R", access & TF_USER ? "U" : "S",
            mode);

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
        // Translation is guaranteed valid until the end of 4096 bytes page
        // (the minimum page size) due possible EPT remapping for the bigger
        // translation units
        uint64_t size = (uint64_t)1 << PG_ORDER_4K;
        uint64_t extend = size - (va & (size - 1));

        // Adjust validity of translation if necessary.
        if (len != NULL && (*len == 0 || *len > extend)) {
            *len = extend;
        }
    }

    return r;
}

uint32_t vcpu_read_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr,
                                 void *dst, uint32_t dst_buflen, uint32_t size,
                                 uint flag)
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
                hax_log(HAX_LOGE, "%s: read_guest_virtual(%llx, %x) failed\n",
                        __func__, addr, size);
            }
            hax_log(HAX_LOGD, "%s: read_guest_virtual(%llx, %x) injecting #PF"
                    "\n", __func__, addr, size);
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
            hax_log(HAX_LOGPANIC, "%s: read guest virtual error, gpa:0x%llx, "
                    "len:0x%llx\n", __func__, gpa, len);
            return false;
        }

        len = (uint64_t)len2;
        offset += len;
    }

    return flag != 0 ? size : true;
}

uint32_t vcpu_write_guest_virtual(struct vcpu_t *vcpu, hax_vaddr_t addr,
                                  uint32_t dst_buflen, const void *src,
                                  uint32_t size, uint flag)
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
                hax_log(HAX_LOGPANIC, "%s: write_guest_virtual(%llx, %x) failed"
                        "\n", __func__, addr, size);
            }
            hax_log(HAX_LOGD, "%s: write_guest_virtual(%llx, %x) injecting #PF"
                    "\n", __func__, addr, size);
            vcpu->state->_cr2 = addr + offset;
            hax_inject_page_fault(vcpu, r & 0x1f);

            return false;
        }

        len2 = (uint64_t)gpa_space_write_data(&vcpu->vm->gpa_space, gpa, len,
                                              (uint8_t *)(srcp + offset));
        if (len2 <= 0) {
            vcpu_set_panic(vcpu);
            hax_log(HAX_LOGPANIC, "%s: write guest virtual error, gpa:0x%llx, "
                    "len:0x%llx\n", __func__, gpa, len);
            return false;
        }

        len = len2;
        offset += len;
    }

    return flag != 0 ? size : true;
}

static inline void * mmio_map_guest_virtual_page_fast(struct vcpu_t *vcpu,
                                                      uint64_t gva, int len)
{
    if (!vcpu->mmio_fetch.kva)
        return NULL;

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
    // Skip GVA=>GPA=>KVA conversion, and just use the cached KVA.
    // TODO: We do not walk the guest page tables in this case, which saves
    // time, but also means the accessed/dirty bits of the relevant guest page
    // table entries are not updated. This should be okay, since the same MMIO
    // instruction was just fetched by hardware (before this EPT violation),
    // which presumably has taken care of this matter.
    return vcpu->mmio_fetch.kva;
}

static void * mmio_map_guest_virtual_page_slow(struct vcpu_t *vcpu,
                                               uint64_t gva,
                                               hax_kmap_user *kmap)
{
    uint64_t gva_aligned = gva & pgmask(PG_ORDER_4K);
    uint64_t gpa;
    uint ret;
    void *kva;

    ret = vcpu_translate(vcpu, gva_aligned, 0, &gpa, NULL, true);
    if (ret) {
        hax_log(HAX_LOGE, "%s: vcpu_translate() returned 0x%x: vcpu_id=%u, "
                "gva=0x%llx\n", __func__, ret, vcpu->vcpu_id, gva);
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

static pagemode_t vcpu_get_pagemode(struct vcpu_t *vcpu)
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
