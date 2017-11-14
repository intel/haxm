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

#include "..\hax_win.h"

struct qword_val {
    uint32 low;
    uint32 high;
};

extern uint32 ia32_asm_vmread(component_index_t component);
extern void _cdecl asm_enable_irq(void);
extern void _cdecl asm_disable_irq(void);
extern void ia32_asm_vmwrite(component_index_t component, uint32 val);

extern uint64 asm_rdmsr(uint32 reg, struct qword_val *qv);
extern void asm_wrmsr(uint32 reg, struct qword_val *qv);

extern uint64 asm_rdtsc(struct qword_val *qv);

extern void asm_fxinit(void);
extern void asm_fxsave(mword *addr);
extern void asm_fxrstor(mword *addr);

extern void asm_btr(uint8 *addr, uint bit);
extern void asm_bts(uint8 *addr, uint bit);

extern void asm_vmptrst(paddr_t *addr);

paddr_t __vmptrst(void)
{
    paddr_t address = 0;
    asm_vmptrst(&address);
    return address;
}

uint64 ia32_rdmsr(uint32 reg)
{
    struct qword_val val = {0};
    asm_rdmsr(reg, &val);

    return ((uint64)(val.low) | (uint64)(val.high) << 32);
}

void ia32_wrmsr(uint32 reg, uint64 val)
{
    struct qword_val tmp = {0};
    uint64 old = val;

    tmp.high = (uint32)(val >> 32);
    tmp.low = (uint32)val;

    asm_wrmsr(reg, &tmp);
}

uint64 rdtsc(void)
{
    struct qword_val val = {0};
    asm_rdtsc(&val);
    return ((uint64)(val.low) | (uint64)(val.high) << 32);
}

void fxinit(void)
{
    asm_fxinit();
}

void fxsave(mword *addr)
{
    asm_fxsave(addr);
}

void fxrstor(mword *addr)
{
    asm_fxrstor(addr);
}

void btr(uint8 *addr, uint bit)
{
    // asm_btr() may not be able to handle bit offsets greater than 0xff. For
    // absolute safety, ensure that the bit offset is less than 8.
    uint8 *base = addr + bit / 8;
    uint offset = bit % 8;
    asm_btr(base, offset);
}

void bts(uint8 *addr, uint bit)
{
    uint8 *base = addr + bit / 8;
    uint offset = bit % 8;
    asm_bts(base, offset);
}

uint32 ia32_vmread(component_index_t component)
{
    return (ia32_asm_vmread(component));
}

void ia32_vmwrite(component_index_t component, uint32 val)
{
    ia32_asm_vmwrite(component, val);
}

void _vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                  component_index_t component,
                  mword source_val)
{
    ia32_vmwrite(component, source_val);
}

void _vmx_vmwrite_64(struct vcpu_t *vcpu, const char *name,
                     component_index_t component,
                     uint64 source_val)
{
    ia32_vmwrite(component, (uint32)source_val);
    ia32_vmwrite(component + 1, (uint32)(source_val >> 32));
}

void _vmx_vmwrite_natural(struct vcpu_t *vcpu, const char *name,
                          component_index_t component,
                          uint64 source_val)
{
    ia32_vmwrite(component, (uint32)source_val);
}

uint64 vmx_vmread(struct vcpu_t *vcpu, component_index_t component)
{
    uint64 val = 0;

    val = ia32_vmread(component);
    return val;
}

uint64 vmx_vmread_natural(struct vcpu_t *vcpu, component_index_t component)
{
    uint64 val = 0;

    val = ia32_vmread(component);
    return val;
}

uint64 vmx_vmread_64(struct vcpu_t *vcpu, component_index_t component)
{
    uint64 val = 0;

    val = ia32_vmread(component);
    val |= ((uint64)(ia32_vmread(component + 1)) << 32);
    return val;
}

void hax_enable_irq(void)
{
    asm_enable_irq();
}

void hax_disable_irq(void)
{
    asm_disable_irq();
}
