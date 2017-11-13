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

#ifndef HAX_ASM_H_
#define HAX_ASM_H_

#include "hax_types.h"

struct vcpu_t;
struct invept_desc;

mword get_cr0(void);
mword get_cr2(void);
uint64 get_cr3(void);
mword get_cr4(void);
mword get_dr0(void);
mword get_dr1(void);
mword get_dr2(void);
mword get_dr3(void);
mword get_dr6(void);
mword get_dr7(void);

void set_cr0(mword val);
void set_cr2(mword val);
void set_cr3(mword val);
void set_cr4(mword val);
void set_dr0(mword val);
void set_dr1(mword val);
void set_dr2(mword val);
void set_dr3(mword val);
void set_dr6(mword val);
void set_dr7(mword val);

uint16 get_kernel_cs(void);
uint16 get_kernel_ds(void);
uint16 get_kernel_es(void);
uint16 get_kernel_ss(void);
uint16 get_kernel_gs(void);
uint16 get_kernel_fs(void);

void set_kernel_ds(uint16 val);
void set_kernel_es(uint16 val);
void set_kernel_gs(uint16 val);
void set_kernel_fs(uint16 val);

mword get_rip(void);

uint64 ia32_rdmsr(uint32 reg);
void ia32_wrmsr(uint32 reg, uint64 val);

uint64 rdtsc(void);

void fxinit(void);
void fxsave(mword *addr);
void fxrstor(mword *addr);

void btr(uint8 *addr, uint bit);
void bts(uint8 *addr, uint bit);
int cpu_has_emt64_support(void);
int cpu_has_vmx_support(void);
int cpu_has_nx_support(void);

uint64 get_kernel_rflags(void);
void __nmi(void);
uint32 __fls(uint32 bit32);

void load_kernel_ldt(uint16 sel);
uint16 get_kernel_tr_selector(void);
uint16 get_kernel_ldt(void);

void set_kernel_gdt(struct system_desc_t *sys_desc);
void set_kernel_idt(struct system_desc_t *sys_desc);
void get_kernel_gdt(struct system_desc_t *sys_desc);
void get_kernel_idt(struct system_desc_t *sys_desc);
void __handle_cpuid(struct vcpu_state_t *state);

vmx_error_t __vmxon(paddr_t addr);
vmx_error_t __vmxoff(void);

vmx_error_t __vmclear(paddr_t addr);
vmx_error_t __vmptrld(paddr_t addr);
paddr_t __vmptrst(void);

uint64 vmx_vmread(struct vcpu_t *vcpu, component_index_t component);
uint64 vmx_vmread_natural(struct vcpu_t *vcpu, component_index_t component);
uint64 vmx_vmread_64(struct vcpu_t *vcpu, component_index_t component);
void _vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                  component_index_t component, mword source_val);
void _vmx_vmwrite_natural(struct vcpu_t *vcpu, const char *name,
                          component_index_t component, uint64 source_val);
void _vmx_vmwrite_64(struct vcpu_t *vcpu, const char *name,
                     component_index_t component, uint64 source_val);

uint64 __vmx_run(struct vcpu_state_t *state, uint16 launch);

vmx_error_t __invept(uint type, struct invept_desc *desc);

#endif  // HAX_ASM_H_
