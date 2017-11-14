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

#ifndef HAX_DARWIN_COM_INTEL_HAX_COMPONENT_H_
#define HAX_DARWIN_COM_INTEL_HAX_COMPONENT_H_

#include "com_intel_hax.h"

struct hax_vm_mac {
    /* The hax core's vm id */
    int vm_id;
    /* the minor id of corresponding devfs device */
    void *pnode;
    /* the uid of the process that creates the VM */
    uid_t owner;
    gid_t gowner;
    int ram_entry_num;
    struct hax_vcpu_mem *ram_entry;
    struct vm_t *cvm;
};

struct hax_vcpu_mac {
    int vcpu_id;
    int vm_id;
    void *pnode;
    /* pointer to the common vcpu structure */
    struct vcpu_t *cvcpu;
    /* the uid of the process that creates the VM */
    uid_t owner;
    gid_t gowner;
};

__private_extern__
struct hax_vcpu_mac * hax_vcpu_create_mac(struct vcpu_t *cvcpu, void *vm_host,
                                          int vm_id, int vcpu_id);

__private_extern__
void hax_vcpu_destroy_mac(struct hax_vcpu_mac *vcpu);

__private_extern__
void hax_vm_destroy_mac(struct hax_vm_mac *vm);

__private_extern__
struct hax_vm_mac * hax_vm_create_mac(struct vm_t *vm, int vm_id);

__private_extern__
int hax_vcpu_destroy(struct vcpu_t *cvcpu, int dest_vm);

static struct vcpu_t *mvcpu2cvcpu(struct hax_vcpu_mac *vcpu) {
    if (!vcpu)
        return NULL;
    return vcpu->cvcpu;
}

#endif  // HAX_DARWIN_COM_INTEL_HAX_COMPONENT_H_
