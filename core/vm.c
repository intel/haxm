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

#include "include/vm.h"
#include "include/hax_driver.h"
#include "include/vcpu.h"
#include "../include/hax.h"
#include "include/hax_core_interface.h"
#include "include/ept.h"
#include "include/paging.h"
#include "include/config.h"

static uint64_t vm_mid_bits = 0;

#ifdef HAX_ARCH_X86_32
static void gpfn_to_hva_recycle_total(struct vm_t *vm, uint64_t cr3_cur,
                                      int flag);
#endif

static int get_free_vm_mid(void)
{
    int i;

    for (i = 0; i < HAX_MAX_VMS; i++) {
        if (!hax_test_and_set_bit(i, (uint64_t *)&vm_mid_bits))
            return i;
    }

    return -1;
}

static void hax_put_vm_mid(int id)
{
    if (hax_test_and_clear_bit(id, (uint64_t *)&vm_mid_bits))
        hax_log(HAX_LOGW, "Clear a non-set vmid %x\n", id);
}

static int valid_vm_mid(int vm_id)
{
    return (vm_id >= 0) && (vm_id < HAX_MAX_VMS);
}

int hax_vm_set_qemuversion(struct vm_t *vm, struct hax_qemu_version *ver)
{
    if (ver->cur_version >= 0x2) {
        vm->features |= VM_FEATURES_FASTMMIO_BASIC;
        if (ver->cur_version >= 0x4) {
            vm->features |= VM_FEATURES_FASTMMIO_EXTRA;
        }
    }
    return 0;
}

uint64_t vm_get_eptp(struct vm_t *vm)
{
    return vm->ept_tree.eptp.value;
}

/* Ioctl will call this function to create a vm */
struct vm_t * hax_create_vm(int *vm_id)
{
    struct vm_t *hvm;
    int id;
    int ret;

    if ((!hax->vmx_enable_flag) || (!hax->nx_enable_flag)) {
        hax_log(HAX_LOGE, "VT or NX is not enabled, can not setup VM!\n");
        return NULL;
    }

    id = get_free_vm_mid();

    if (!valid_vm_mid(id)) {
        hax_log(HAX_LOGE, "Failed to allocate vm id\n");
        return NULL;
    }
    *vm_id = id;
    hvm = hax_vmalloc(sizeof(struct vm_t), HAX_MEM_NONPAGE);
    if (!hvm) {
        hax_put_vm_mid(id);
        hax_log(HAX_LOGE, "Failed to allocate vm\n");
        return NULL;
    }
    memset(hvm, 0, sizeof(struct vm_t));

    hvm->vm_id = id;
    memset(hvm->vpid_seed, 0, sizeof(hvm->vpid_seed));

#ifdef HAX_ARCH_X86_32
    hvm->hva_list = hax_vmalloc(((HVA_MAP_ARRAY_SIZE / 4096) *
                                sizeof(struct hva_entry)), HAX_MEM_NONPAGE);
    if (!hvm->hva_list)
        goto fail;

    memset((void *)(hvm->hva_list), 0,
           ((HVA_MAP_ARRAY_SIZE / 4096) * sizeof(struct hva_entry)));

    hvm->hva_list_1 = hax_vmalloc(((HVA_MAP_ARRAY_SIZE / 4096) *
                                  sizeof(struct hva_entry)), HAX_MEM_NONPAGE);
    if (!hvm->hva_list_1)
        goto fail0;

    memset((void *)(hvm->hva_list_1), 0,
           ((HVA_MAP_ARRAY_SIZE / 4096) * sizeof(struct hva_entry)));
#endif

    ret = gpa_space_init(&hvm->gpa_space);
    if (ret) {
        hax_log(HAX_LOGE, "%s: gpa_space_init() returned %d\n", __func__, ret);
        goto fail1;
    }
    ret = ept_tree_init(&hvm->ept_tree);
    if (ret) {
        hax_log(HAX_LOGE, "%s: ept_tree_init() returned %d\n", __func__, ret);
        goto fail1;
    }

    hvm->gpa_space_listener.mapping_added = NULL;
    hvm->gpa_space_listener.mapping_removed = ept_handle_mapping_removed;
    hvm->gpa_space_listener.mapping_changed = ept_handle_mapping_changed;
    hvm->gpa_space_listener.opaque = (void *)&hvm->ept_tree;
    gpa_space_add_listener(&hvm->gpa_space, &hvm->gpa_space_listener);

    hax_log(HAX_LOGI, "%s: Invoking INVEPT for VM %d\n", __func__, hvm->vm_id);
    invept(hvm, EPT_INVEPT_SINGLE_CONTEXT);

    hvm->vm_lock = hax_mutex_alloc_init();
    if (!hvm->vm_lock)
        goto fail1;
    hax_init_list_head(&hvm->vcpu_list);
    if (hax_vm_create_host(hvm, id) < 0)
        goto fail2;

    /* Publish the VM */
    hax_mutex_lock(hax->hax_lock);
    hax_list_add(&hvm->hvm_list, &hax->hax_vmlist);
    hvm->ref_count = 1;
    hax_mutex_unlock(hax->hax_lock);
    return hvm;
fail2:
    hax_mutex_free(hvm->vm_lock);
fail1:
#ifdef HAX_ARCH_X86_32
    hax_vfree(hvm->hva_list_1,
              ((HVA_MAP_ARRAY_SIZE / 4096) * sizeof(struct hva_entry)));

fail0:
    hax_vfree(hvm->hva_list,
              ((HVA_MAP_ARRAY_SIZE / 4096) * sizeof(struct hva_entry)));
fail:
#endif
    hax_vfree(hvm, sizeof(struct vm_t));
    hax_put_vm_mid(id);
    return NULL;
}

static void hax_vm_free_p2m_map(struct vm_t *vm)
{
    int i;
    for (i = 0; i < MAX_GMEM_G; i++) {
        if (!vm->p2m_map[i])
            continue;
        hax_vfree(vm->p2m_map[i], GPFN_MAP_ARRAY_SIZE);
        vm->p2m_map[i] = NULL;
    }
}

/*
 * We don't need corresponding vm_core_close because once closed, the VM will be
 * destroyed.
 */
int hax_vm_core_open(struct vm_t *vm)
{
    if (!vm)
        return -ENODEV;

    if (hax_test_and_set_bit(VM_STATE_FLAGS_OPENED, &(vm->flags)))
        return -EBUSY;

    return 0;
}

int hax_teardown_vm(struct vm_t *vm)
{
    if (!hax_list_empty(&(vm->vcpu_list))) {
        hax_log(HAX_LOGI, "Try to teardown non-empty vm\n");
        return -1;
    }
#ifdef HAX_ARCH_X86_32
    if (vm->hva_list) {
        gpfn_to_hva_recycle_total(vm, 0, true);
        hax_vfree(vm->hva_list,
                  ((HVA_MAP_ARRAY_SIZE / 4096) * sizeof(struct hva_entry)));
        hax_vfree(vm->hva_list_1,
                  ((HVA_MAP_ARRAY_SIZE / 4096) * sizeof(struct hva_entry)));
    }
#endif

    hax_vm_free_p2m_map(vm);
    hax_mutex_free(vm->vm_lock);
    hax_put_vm_mid(vm->vm_id);

    gpa_space_remove_listener(&vm->gpa_space, &vm->gpa_space_listener);
    ept_tree_free(&vm->ept_tree);
    gpa_space_free(&vm->gpa_space);

    hax_vfree(vm, sizeof(struct vm_t));
    hax_log(HAX_LOGE, "...........hax_teardown_vm\n");
    return 0;
}

struct vcpu_t * hax_get_vcpu(int vm_id, int vcpu_id, int refer)
{
    struct vm_t *vm = NULL;
    struct vcpu_t *vcpu = NULL;
    hax_list_head *list;
    int found = 0;

    vm = hax_get_vm(vm_id, 1);
    if (!vm)
        return NULL;

    hax_mutex_lock(vm->vm_lock);
    hax_list_for_each(list, (hax_list_head *)(&vm->vcpu_list)) {
        vcpu = hax_list_entry(vcpu_list, struct vcpu_t, list);
        if (vcpu->vcpu_id == vcpu_id) {
            found = 1;

            if (refer) {
                signed int count;

                count = hax_atomic_add(&vcpu->ref_count, 1);
                // Destroy on way already, we need to return NULL now.
                if (count <= 0) {
                    hax_atomic_dec(&vcpu->ref_count);
                    vcpu = NULL;
                }
            }
            break;
        }
    }
    hax_mutex_unlock(vm->vm_lock);
    if (!found)
        vcpu = NULL;
    hax_put_vm(vm);
    return vcpu;
}

struct vm_t * hax_get_vm(int vm_id, int ref)
{
    struct vm_t *vm = NULL;
    hax_list_head *list;
    int found = 0;

    hax_mutex_lock(hax->hax_lock);
    hax_list_for_each(list, (hax_list_head *)(&hax->hax_vmlist)) {
        vm = hax_list_entry(hvm_list, struct vm_t, list);
        if (vm->vm_id == vm_id) {
            found = 1;
            if (ref) {
                signed int count;
                count = hax_atomic_add(&vm->ref_count, 1);
                // If ref count is 0, that means the vm is on way to destroy
                if (count <= 0) {
                    hax_atomic_dec(&vm->ref_count);
                    vm = NULL;
                }
            }
            break;
        }
    }
    hax_mutex_unlock(hax->hax_lock);

    if (!found)
        return NULL;
    return vm;
}

int hax_put_vm(struct vm_t *vm)
{
    int count;

    count = hax_atomic_dec(&vm->ref_count);

    if (count == 1) {
        hax_mutex_lock(hax->hax_lock);
        hax_list_del(&vm->hvm_list);
        hax_mutex_unlock(hax->hax_lock);

        hax_vm_destroy_host(vm, vm->vm_host);
        hax_teardown_vm(vm);
    }
    return count--;
}

void * get_vm_host(struct vm_t *vm)
{
    return vm ? vm->vm_host : NULL;
}

int set_vm_host(struct vm_t *vm, void *vm_host)
{
    if (!vm || ((vm->vm_host) && (vm->vm_host != vm_host)))
        return -1;
    vm->vm_host = vm_host;
    return 0;
}

static struct hax_p2m_entry * hax_get_p2m_entry(struct vm_t *vm, uint64_t gpfn)
{
    uint32_t which_g = gpfn_to_g(gpfn);
    uint32_t index = gpfn_in_g(gpfn);
    struct hax_p2m_entry *p2m_base;

    if (!vm->p2m_map[which_g])
        return NULL;
    p2m_base = vm->p2m_map[which_g];
    return &p2m_base[index];
}

uint64_t hax_gpfn_to_hpa(struct vm_t *vm, uint64_t gpfn)
{
    uint64_t pfn;

    pfn = gpa_space_get_pfn(&vm->gpa_space, gpfn, NULL);
    if (INVALID_PFN == pfn) {
        return 0;
    }
    return pfn << PG_ORDER_4K;
}

#ifdef HAX_ARCH_X86_32
static void gpfn_to_hva_recycle_total(struct vm_t *vm, uint64_t cr3_cur, int flag)
{
    int i = 0;
    int top = 0;
    struct hax_p2m_entry *entry;

    if (!vm->hva_list || !vm->hva_list_1)
        return;

    top = (HOST_VIRTUAL_ADDR_LIMIT - HOST_VIRTUAL_ADDR_RECYCLE) / 4096;
    for (i = 0; i < top; i++) {
        if (!vm->hva_list[i].level && vm->hva_list[i].hva) {
            entry = hax_get_p2m_entry(vm, vm->hva_list[i].gpfn);
            hax_vunmap((void *)(vm->hva_list[i].hva), 4096);

            if (entry) {
                entry->hva = 0;
            }
            vm->hva_list[i].gpfn = 0;
            vm->hva_list[i].hva = 0;
            vm->hva_list[i].gcr3 = 0;
            vm->hva_list[i].is_kern = 0;
            vm->hva_list[i].level = 0;
            vm->hva_limit -= 4096;
        }
    }

    for (i = 0; i < top; i++) {
        if (vm->hva_list[i].hva) {
            entry = hax_get_p2m_entry(vm, vm->hva_list[i].gpfn);
            hax_vunmap((void *)(vm->hva_list[i].hva), 4096);

            if (entry) {
                entry->hva = 0;
            }
            vm->hva_list[i].gpfn = 0;
            vm->hva_list[i].hva = 0;
            vm->hva_list[i].gcr3 = 0;
            vm->hva_list[i].is_kern = 0;
            vm->hva_list[i].level = 0;
            vm->hva_limit -= 4096;
        }
    }

    top = HOST_VIRTUAL_ADDR_RECYCLE / 4096;
    for (i = 0; i < top; i++) {
        if (!vm->hva_list_1[i].level && vm->hva_list_1[i].hva) {
            entry = hax_get_p2m_entry(vm, vm->hva_list_1[i].gpfn);
            hax_vunmap((void *)(vm->hva_list_1[i].hva), 4096);

            if (entry) {
                entry->hva = 0;
            }
            vm->hva_list_1[i].gpfn = 0;
            vm->hva_list_1[i].hva = 0;
            vm->hva_list_1[i].gcr3 = 0;
            vm->hva_list_1[i].is_kern = 0;
            vm->hva_list_1[i].level = 0;
            vm->hva_limit -= 4096;
        }
    }

    for (i = 0; i < top; i++) {
        if (vm->hva_list_1[i].hva) {
            entry = hax_get_p2m_entry(vm, vm->hva_list_1[i].gpfn);
            hax_vunmap((void *)(vm->hva_list_1[i].hva), 4096);

            if (entry) {
                entry->hva = 0;
            }
            vm->hva_list_1[i].gpfn = 0;
            vm->hva_list_1[i].hva = 0;
            vm->hva_list_1[i].gcr3 = 0;
            vm->hva_list_1[i].is_kern = 0;
            vm->hva_list_1[i].level = 0;
            vm->hva_limit -= 4096;
        }
    }
}

static int gpfn_to_hva_recycle(struct vm_t *vm, uint64_t cr3_cur, int flag)
{
    int i = 0, count = 0;
    int top = 0;
    struct hax_p2m_entry *entry;

    if (!vm->hva_list)
        return 0;

    top = (HOST_VIRTUAL_ADDR_LIMIT - HOST_VIRTUAL_ADDR_RECYCLE) / 4096;
    for (i = 0; i < top; i++) {
        if (flag || ((vm->hva_list[i].gcr3 != cr3_cur) &&
                !vm->hva_list[i].is_kern)) {
            entry = hax_get_p2m_entry(vm, vm->hva_list[i].gpfn);
            hax_vunmap((void *)(vm->hva_list[i].hva), 4096);

            if (entry) {
                entry->hva = 0;
            }
            vm->hva_list[i].gpfn = 0;
            vm->hva_list[i].hva = 0;
            vm->hva_list[i].gcr3 = 0;
            vm->hva_list[i].is_kern = 0;
            vm->hva_list[i].level = 0;
            vm->hva_limit -= 4096;
            count++;
        }
    }
    vm->hva_index = 0;
    return count;
}
#endif
