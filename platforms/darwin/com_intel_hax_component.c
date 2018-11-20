/*
 * Copyright (c) 2011 Intel Corporation
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

/*
 * Define the data structure and handling method for vm, vcpu
 */

#include "com_intel_hax.h"

struct hax_vcpu_mac * hax_vcpu_create_mac(struct vcpu_t *cvcpu, void *vm_host,
                                          int vm_id, int cid) {
    struct hax_vcpu_mac *vcpu;
    struct hax_vm_mac *vm = vm_host;

    if (!cvcpu || !vm_host) {
        printf("NULL cvcpu or vmhost in mac vcpu\n");
        return NULL;
    }

    vcpu = hax_vmalloc(sizeof(struct hax_vcpu_mac), 0);
    if (!vcpu) {
        printf("Failed to allocation\n");
        return NULL;
    }
    memset(vcpu, 0, sizeof(struct hax_vcpu_mac));

    vcpu->vm_id = vm_id;
    vcpu->vcpu_id = cid;
    vcpu->owner = vm->owner;
    vcpu->gowner = vm->gowner;

    set_vcpu_host(cvcpu, vcpu);
    vcpu->cvcpu = cvcpu;

    return vcpu;
}

void hax_vcpu_destroy_mac(struct hax_vcpu_mac *vcpu)
{
    struct vcpu_t *cv;
    cv = mvcpu2cvcpu(vcpu);
    if (!cv)
        return;
    hax_assert(!vcpu->pnode);

    hax_vcpu_destroy_hax_tunnel(cv);

    set_vcpu_host(cv, NULL);
    vcpu->cvcpu = NULL;

    hax_vfree(vcpu, sizeof(struct hax_vcpu_mac));
}


int hax_vcpu_destroy_host(struct vcpu_t *cvcpu, struct hax_vcpu_mac *vcpu)
{
    hax_vcpu_destroy_ui(vcpu);
    hax_vcpu_destroy_mac(vcpu);

    return 0;
}

int hax_vcpu_create_host(struct vcpu_t *cvcpu, void *vm_host, int vm_id,
                         int vcpu_id)
{
    struct hax_vcpu_mac *vcpu;

    printf("cvcpu %p vmid %x vcpu_id %x\n", cvcpu, vm_id, vcpu_id);
    vcpu = hax_vcpu_create_mac(cvcpu, vm_host, vm_id, vcpu_id);
    if (!vcpu) {
        printf("\nvcpu_mac failed\n");
        return -1;
    }

    if (hax_vcpu_create_ui(vcpu) < 0) {
        hax_vcpu_destroy_mac(vcpu);
        return -1;
    }
    return 0;
}

struct hax_vm_mac * hax_vm_create_mac(struct vm_t *cvm, int vm_id) {
    struct hax_vm_mac *vm;

    if (!cvm)
        return NULL;

    vm = hax_vmalloc(sizeof(struct hax_vm_mac), 0);
    if (!vm)
        return NULL;
    memset(vm, 0, sizeof(struct hax_vm_mac));

    vm->vm_id = vm_id;
    vm->cvm = cvm;
    /* the owner is current thread's effective uid */
    vm->owner = kauth_getuid();
    vm->gowner = kauth_getgid();
    set_vm_host(cvm, vm);
    return vm;
}

void hax_vm_destroy_mac(struct hax_vm_mac *vm)
{
    struct vm_t *cvm;

    if (!vm)
        return;
    cvm = vm->cvm;
    set_vm_host(cvm, NULL);
    vm->cvm = NULL;
    hax_vm_free_all_ram(cvm);
    hax_vfree(vm, sizeof(struct hax_vm_mac));
}

/* When comes here, all vcpus should have been destroyed already */
int hax_vm_destroy_host(struct vm_t *cvm, void *host_pointer)
{
    struct hax_vm_mac *vm = (struct hax_vm_mac *)host_pointer;

    hax_vm_destroy_ui(vm);
    hax_vm_destroy_mac(vm);

    return 0;
}

int hax_vm_create_host(struct vm_t *cvm, int vm_id)
{
    struct hax_vm_mac *vm;
    int ret;

    vm = hax_vm_create_mac(cvm, vm_id);
    if (!vm)
        return -1;
    ret = hax_vm_create_ui(vm);
    if (ret < 0)
        hax_vm_destroy_mac(vm);
    return ret;
}

int hax_destroy_host_interface(void)
{
    return com_intel_hax_exit_ui();
}
