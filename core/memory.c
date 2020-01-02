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
#include "include/vcpu.h"
#include "include/vm.h"
#include "include/hax_driver.h"
#include "include/ept.h"
#include "include/paging.h"

static int handle_alloc_ram(struct vm_t *vm, uint64_t start_uva, uint64_t size)
{
    int ret;
    hax_ramblock *block;

    if (!start_uva) {
        hax_log(HAX_LOGE, "%s: start_uva == 0\n", __func__);
        return -EINVAL;
    }
    if (!size) {
        hax_log(HAX_LOGE, "%s: size == 0\n", __func__);
        return -EINVAL;
    }

    hax_assert(vm != NULL);
    ret = ramblock_add(&vm->gpa_space.ramblock_list, start_uva, size, NULL,
                       &block);
    if (ret) {
        hax_log(HAX_LOGE, "%s: ramblock_add() failed: ret=%d, start_uva=0x%llx,"
                " size=0x%llx\n", __func__, ret, start_uva, size);
        return ret;
    }
    return 0;
}

int hax_vm_add_ramblock(struct vm_t *vm, uint64_t start_uva, uint64_t size)
{
    return handle_alloc_ram(vm, start_uva, size);
}

int hax_vm_free_all_ram(struct vm_t *vm)
{
    int i;
    uint64_t tsize = 0;
    struct hax_vcpu_mem *mem;
    uint64_t spare = 0;

    mem = vm->ram_entry;
    for (i = 0; i < vm->ram_entry_num; i++) {
        tsize += mem->size;
        hax_clear_vcpumem(mem);
        mem++;
    }
    if (vm->ram_entry) {
        hax_vfree(vm->ram_entry,
                  sizeof(struct hax_vcpu_mem) * vm->ram_entry_num);
        vm->ram_entry_num = 0;
        vm->ram_entry = NULL;
    }

    hax_mutex_lock(hax->hax_lock);
    if (hax->mem_limit) {
        if (hax_test_bit(VM_STATE_FLAGS_MEM_ALLOC, &vm->flags)) {
            spare = VM_SPARE_RAMSIZE - vm->spare_ramsize;
        }
        hax->mem_quota += tsize - spare;
    }
    hax_mutex_unlock(hax->hax_lock);
    return 0;
}

static int handle_set_ram(struct vm_t *vm, uint64_t start_gpa, uint64_t size,
                          uint64_t start_uva, uint32_t flags)
{
    bool unmap = flags & HAX_MEMSLOT_INVALID;
    hax_gpa_space *gpa_space;
    uint64_t start_gfn, npages;
    int ret;
    hax_ept_tree *ept_tree;

    // HAX_RAM_INFO_INVALID indicates that guest physical address range
    // [start_gpa, start_gpa + size) should be unmapped
    if (unmap && (flags != HAX_MEMSLOT_INVALID || start_uva)) {
        hax_log(HAX_LOGE, "%s: Invalid start_uva=0x%llx or flags=0x%x for "
                "unmapping\n", __func__, start_uva, flags);
        return -EINVAL;
    }
    if (!unmap && !start_uva) {
        hax_log(HAX_LOGE, "%s: Cannot map to an invalid UVA\n", __func__);
        return -EINVAL;
    }
    if (!size) {
        hax_log(HAX_LOGE, "%s: size == 0\n", __func__);
        return -EINVAL;
    }

    hax_assert(vm != NULL);
    gpa_space = &vm->gpa_space;
    start_gfn = start_gpa >> PG_ORDER_4K;
    npages = size >> PG_ORDER_4K;

    ret = gpa_space_adjust_prot_bitmap(gpa_space, start_gfn + npages);
    if (ret) {
        hax_log(HAX_LOGE, "%s: Failed to resize prot bitmap: ret=%d, start_gfn="
                "0x%llx, npages=0x%llx, start_uva=0x%llx, flags=0x%x\n",
                __func__, ret, start_gfn, npages, start_uva, flags);
        return ret;
    }

    ret = memslot_set_mapping(gpa_space, start_gfn, npages, start_uva, flags);
    if (ret) {
        hax_log(HAX_LOGE, "%s: memslot_set_mapping() failed: ret=%d, start_gfn="
                "0x%llx, npages=0x%llx, start_uva=0x%llx, flags=0x%x\n",
                __func__, ret, start_gfn, npages, start_uva, flags);
        return ret;
    }
    memslot_dump_list(gpa_space);

    ept_tree = &vm->ept_tree;
    if (!hax_test_and_clear_bit(0, (uint64_t *)&ept_tree->invept_pending)) {
        // INVEPT pending flag was set
        hax_log(HAX_LOGI, "%s: Invoking INVEPT for VM #%d\n",
                __func__, vm->vm_id);
        invept(vm, EPT_INVEPT_SINGLE_CONTEXT);
    }
    return 0;
}

int hax_vm_set_ram(struct vm_t *vm, struct hax_set_ram_info *info)
{
    return handle_set_ram(vm, info->pa_start, info->size, info->va,
                          info->flags);
}

int hax_vm_set_ram2(struct vm_t *vm, struct hax_set_ram_info2 *info)
{
    return handle_set_ram(vm, info->pa_start, info->size, info->va,
                          info->flags);
}

int hax_vm_protect_ram(struct vm_t *vm, struct hax_protect_ram_info *info)
{
    return gpa_space_protect_range(&vm->gpa_space, info->pa_start, info->size,
                                   info->flags);
}

int hax_vcpu_setup_hax_tunnel(struct vcpu_t *cv, struct hax_tunnel_info *info)
{
    int ret = -ENOMEM;

    if (!cv || !info)
        return -EINVAL;

    // The tunnel and iobuf are always set together.
    if (cv->tunnel && cv->iobuf_vcpumem) {
        hax_log(HAX_LOGI, "setup hax tunnel request for already setup one\n");
        info->size = HAX_PAGE_SIZE;
        info->va = cv->tunnel_vcpumem->uva;
        info->io_va = cv->iobuf_vcpumem->uva;
        return 0;
    }

    cv->tunnel_vcpumem = hax_vmalloc(sizeof(struct hax_vcpu_mem), 0);
    if (!cv->tunnel_vcpumem)
        goto error;

    cv->iobuf_vcpumem = hax_vmalloc(sizeof(struct hax_vcpu_mem), 0);
    if (!cv->iobuf_vcpumem)
        goto error;

    ret = hax_setup_vcpumem(cv->tunnel_vcpumem, 0, HAX_PAGE_SIZE, 0);
    if (ret < 0)
        goto error;

    ret = hax_setup_vcpumem(cv->iobuf_vcpumem, 0, IOS_MAX_BUFFER, 0);
    if (ret < 0)
        goto error;

    info->va = cv->tunnel_vcpumem->uva;
    info->io_va = cv->iobuf_vcpumem->uva;
    info->size = HAX_PAGE_SIZE;
    set_vcpu_tunnel(cv, (struct hax_tunnel *)cv->tunnel_vcpumem->kva,
                    (uint8_t *)cv->iobuf_vcpumem->kva);
    return 0;
error:
    if (cv->tunnel_vcpumem) {
        if (cv->tunnel_vcpumem->uva) {
            hax_clear_vcpumem(cv->tunnel_vcpumem);
        }
        hax_vfree(cv->tunnel_vcpumem, sizeof(struct hax_vcpu_mem));
        cv->tunnel_vcpumem = NULL;
    }
    if (cv->iobuf_vcpumem) {
        if (cv->iobuf_vcpumem->uva) {
            hax_clear_vcpumem(cv->iobuf_vcpumem);
        }
        hax_vfree(cv->iobuf_vcpumem, sizeof(struct hax_vcpu_mem));
        cv->iobuf_vcpumem = NULL;
    }
    return ret;
}

int hax_vcpu_destroy_hax_tunnel(struct vcpu_t *cv)
{
    if (!cv)
        return -EINVAL;
    if (!cv->tunnel_vcpumem && !cv->iobuf_vcpumem)
        return 0;
    set_vcpu_tunnel(cv, NULL, NULL);

    if (cv->tunnel_vcpumem) {
        hax_assert(cv->tunnel_vcpumem->uva);
        hax_clear_vcpumem(cv->tunnel_vcpumem);
        hax_vfree(cv->tunnel_vcpumem, sizeof(struct hax_vcpu_mem));
        cv->tunnel_vcpumem = NULL;
    }
    if (cv->iobuf_vcpumem) {
        hax_assert(cv->iobuf_vcpumem->uva);
        hax_clear_vcpumem(cv->iobuf_vcpumem);
        hax_vfree(cv->iobuf_vcpumem, sizeof(struct hax_vcpu_mem));
        cv->iobuf_vcpumem = NULL;
    }
    return 0;
}
