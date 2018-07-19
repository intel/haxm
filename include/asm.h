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

union cpuid_args_t;
struct system_desc_t;

mword ASMCALL get_cr0(void);
mword ASMCALL get_cr2(void);
uint64 ASMCALL get_cr3(void);
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

uint16 ASMCALL get_kernel_cs(void);
uint16 ASMCALL get_kernel_ds(void);
uint16 ASMCALL get_kernel_es(void);
uint16 ASMCALL get_kernel_ss(void);
uint16 ASMCALL get_kernel_gs(void);
uint16 ASMCALL get_kernel_fs(void);

void ASMCALL set_kernel_ds(uint16 val);
void ASMCALL set_kernel_es(uint16 val);
void ASMCALL set_kernel_gs(uint16 val);
void ASMCALL set_kernel_fs(uint16 val);

void ASMCALL asm_btr(uint8 *addr, uint bit);
void ASMCALL asm_bts(uint8 *addr, uint bit);
void ASMCALL asm_fxinit(void);
void ASMCALL asm_fxsave(mword *addr);
void ASMCALL asm_fxrstor(mword *addr);
void ASMCALL asm_cpuid(union cpuid_args_t *state);

void ASMCALL __nmi(void);
uint32 ASMCALL __fls(uint32 bit32);

uint64 ia32_rdmsr(uint32 reg);
void ia32_wrmsr(uint32 reg, uint64 val);

uint64 rdtsc(void);

void fxinit(void);
void fxsave(mword *addr);
void fxrstor(mword *addr);

void btr(uint8 *addr, uint bit);
void bts(uint8 *addr, uint bit);

uint64 ASMCALL get_kernel_rflags(void);
uint16 ASMCALL get_kernel_tr_selector(void);

void ASMCALL set_kernel_gdt(struct system_desc_t *sys_desc);
void ASMCALL set_kernel_idt(struct system_desc_t *sys_desc);
void ASMCALL set_kernel_ldt(uint16 sel);
void ASMCALL get_kernel_gdt(struct system_desc_t *sys_desc);
void ASMCALL get_kernel_idt(struct system_desc_t *sys_desc);
uint16 ASMCALL get_kernel_ldt(void);

#endif  // HAX_ASM_H_
