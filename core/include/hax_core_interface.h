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

#ifndef HAX_CORE_HAX_CORE_INTERFACE_H_
#define HAX_CORE_HAX_CORE_INTERFACE_H_

#include "../../include/hax.h"
struct vcpu_t;
struct vm_t;

#ifdef __cplusplus
extern "C" {
#endif

int vcpu_set_msr(struct vcpu_t *vcpu, uint64_t index, uint64_t value);
int vcpu_get_msr(struct vcpu_t *vcpu, uint64_t index, uint64_t *value);
int vcpu_put_fpu(struct vcpu_t *vcpu, struct fx_layout *fl);
int vcpu_get_fpu(struct vcpu_t *vcpu, struct fx_layout *fl);
int vcpu_set_regs(struct vcpu_t *vcpu, struct vcpu_state_t *vs);
int vcpu_get_regs(struct vcpu_t *vcpu, struct vcpu_state_t *vs);
int vcpu_set_cpuid(struct vcpu_t *vcpu, hax_cpuid *cpuid_info);
void vcpu_debug(struct vcpu_t *vcpu, struct hax_debug_t *debug);

void * get_vcpu_host(struct vcpu_t *vcpu);
int set_vcpu_host(struct vcpu_t *vcpu, void *vcpu_host);

struct hax_tunnel * get_vcpu_tunnel(struct vcpu_t *vcpu);
int hax_vcpu_destroy_hax_tunnel(struct vcpu_t *cv);
int hax_vcpu_setup_hax_tunnel(struct vcpu_t *cv, struct hax_tunnel_info *info);
int hax_vm_set_ram(struct vm_t *vm, struct hax_set_ram_info *info);
int hax_vm_set_ram2(struct vm_t *vm, struct hax_set_ram_info2 *info);
int hax_vm_protect_ram(struct vm_t *vm, struct hax_protect_ram_info *info);
int hax_vm_free_all_ram(struct vm_t *vm);
int hax_vm_add_ramblock(struct vm_t *vm, uint64_t start_uva, uint64_t size);

void * get_vm_host(struct vm_t *vm);
int set_vm_host(struct vm_t *vm, void *vm_host);

/* Called when module loading/unloading */
int hax_module_init(void);
int hax_module_exit(void);

struct vcpu_t * vcpu_create(struct vm_t *vm, void *vm_host, int vcpu_id);
int hax_vcpu_core_open(struct vcpu_t *vcpu);
int vcpu_teardown(struct vcpu_t *vcpu);
int vcpu_execute(struct vcpu_t *vcpu);
int vcpu_interrupt(struct vcpu_t *vcpu, uint8_t vector);

/*
 * Find a vcpu with corresponding id, |refer| decides whether a reference count
 * is added
 */
struct vcpu_t * hax_get_vcpu(int vm_id, int vcpu_id, int refer);
/* Corresponding to hax_get_vcpu with refer == 1 */
int hax_put_vcpu(struct vcpu_t *vcpu);

int hax_get_capability(void *buf, int bufLeng, int *outLength);
int hax_set_memlimit(void *buf, int bufLength, int *outLength);
struct vm_t * hax_get_vm(int vm_id, int refer);
int hax_vm_core_open(struct vm_t *vm);
/* Corresponding hax_get_vm with refer == 1 */
int hax_put_vm(struct vm_t *vm);
int hax_vm_set_qemuversion(struct vm_t *vm, struct hax_qemu_version *ver);

struct vm_t * hax_create_vm(int *vm_id);
int hax_teardown_vm(struct vm_t *vm);
int vcpu_event_pending(struct vcpu_t *vcpu);

#ifdef __cplusplus
}
#endif

#endif  // HAX_CORE_HAX_CORE_INTERFACE_H_
