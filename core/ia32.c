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

#include "include/ia32.h"

struct qword_val {
    uint32_t low;
    uint32_t high;
};

#ifdef HAX_ARCH_X86_32
extern void ASMCALL asm_rdmsr(uint32_t reg, struct qword_val *qv);
extern void ASMCALL asm_wrmsr(uint32_t reg, struct qword_val *qv);
extern void ASMCALL asm_rdtsc(struct qword_val *qv);
#else  // !HAX_ARCH_X86_32
extern uint64_t ASMCALL asm_rdmsr(uint32_t reg);
extern void ASMCALL asm_wrmsr(uint32_t reg, uint64_t val);
extern uint64_t ASMCALL asm_rdtsc(void);
#endif  // HAX_ARCH_X86_32

uint64_t ia32_rdmsr(uint32_t reg)
{
#ifdef HAX_ARCH_X86_32
    struct qword_val val = { 0 };

    asm_rdmsr(reg, &val);
    return ((uint64_t)(val.low) | (uint64_t)(val.high) << 32);
#else
    return asm_rdmsr(reg);
#endif
}

void ia32_wrmsr(uint32_t reg, uint64_t val)
{
#ifdef HAX_ARCH_X86_32
    struct qword_val tmp = { 0 };

    tmp.high = (uint32_t)(val >> 32);
    tmp.low = (uint32_t)val;
    asm_wrmsr(reg, &tmp);
#else
    asm_wrmsr(reg, val);
#endif
}

uint64_t ia32_rdtsc(void)
{
#ifdef HAX_ARCH_X86_32
    struct qword_val val = { 0 };
    asm_rdtsc(&val);
    return ((uint64_t)(val.low) | (uint64_t)(val.high) << 32);
#else
    return asm_rdtsc();
#endif
}

void hax_clts(void)
{
    asm_clts();
}

void hax_fxinit(void)
{
    asm_fxinit();
}

void hax_fxsave(mword *addr)
{
    asm_fxsave(addr);
}

void hax_fxrstor(mword *addr)
{
    asm_fxrstor(addr);
}

void btr(uint8_t *addr, uint bit)
{
    // asm_btr() may not be able to handle bit offsets greater than 0xff. For
    // absolute safety, ensure that the bit offset is less than 8.
    uint8_t *base = addr + bit / 8;
    uint offset = bit % 8;
    asm_btr(base, offset);
}

void bts(uint8_t *addr, uint bit)
{
    uint8_t *base = addr + bit / 8;
    uint offset = bit % 8;
    asm_bts(base, offset);
}
