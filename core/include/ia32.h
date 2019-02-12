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

#ifndef HAX_CORE_IA32_H_
#define HAX_CORE_IA32_H_

#include "../../include/hax_types.h"

union cpuid_args_t;
struct system_desc_t;

mword ASMCALL get_cr0(void);
mword ASMCALL get_cr2(void);
mword ASMCALL get_cr3(void);
mword ASMCALL get_cr4(void);
mword ASMCALL get_dr0(void);
mword ASMCALL get_dr1(void);
mword ASMCALL get_dr2(void);
mword ASMCALL get_dr3(void);
mword ASMCALL get_dr6(void);
mword ASMCALL get_dr7(void);

void ASMCALL set_cr0(mword val);
void ASMCALL set_cr2(mword val);
void ASMCALL set_cr3(mword val);
void ASMCALL set_cr4(mword val);
void ASMCALL set_dr0(mword val);
void ASMCALL set_dr1(mword val);
void ASMCALL set_dr2(mword val);
void ASMCALL set_dr3(mword val);
void ASMCALL set_dr6(mword val);
void ASMCALL set_dr7(mword val);

uint16_t ASMCALL get_kernel_cs(void);
uint16_t ASMCALL get_kernel_ds(void);
uint16_t ASMCALL get_kernel_es(void);
uint16_t ASMCALL get_kernel_ss(void);
uint16_t ASMCALL get_kernel_gs(void);
uint16_t ASMCALL get_kernel_fs(void);

void ASMCALL set_kernel_ds(uint16_t val);
void ASMCALL set_kernel_es(uint16_t val);
void ASMCALL set_kernel_gs(uint16_t val);
void ASMCALL set_kernel_fs(uint16_t val);

void ASMCALL asm_btr(uint8_t *addr, uint bit);
void ASMCALL asm_bts(uint8_t *addr, uint bit);
void ASMCALL asm_clts(void);
void ASMCALL asm_fxinit(void);
void ASMCALL asm_fxsave(mword *addr);
void ASMCALL asm_fxrstor(mword *addr);
void ASMCALL asm_cpuid(union cpuid_args_t *state);

void ASMCALL __nmi(void);
uint32_t ASMCALL asm_fls(uint32_t bit32);

uint64_t ia32_rdmsr(uint32_t reg);
void ia32_wrmsr(uint32_t reg, uint64_t val);

uint64_t ia32_rdtsc(void);

void hax_clts(void);

void hax_fxinit(void);
void hax_fxsave(mword *addr);
void hax_fxrstor(mword *addr);

void btr(uint8_t *addr, uint bit);
void bts(uint8_t *addr, uint bit);

void ASMCALL asm_enable_irq(void);
void ASMCALL asm_disable_irq(void);

uint64_t ASMCALL get_kernel_rflags(void);
uint16_t ASMCALL get_kernel_tr_selector(void);

void ASMCALL set_kernel_gdt(struct system_desc_t *sys_desc);
void ASMCALL set_kernel_idt(struct system_desc_t *sys_desc);
void ASMCALL set_kernel_ldt(uint16_t sel);
void ASMCALL get_kernel_gdt(struct system_desc_t *sys_desc);
void ASMCALL get_kernel_idt(struct system_desc_t *sys_desc);
uint16_t ASMCALL get_kernel_ldt(void);

#endif  // HAX_CORE_IA32_H_
