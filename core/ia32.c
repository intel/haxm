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
    uint32 low;
    uint32 high;
};

#ifdef _M_IX86
extern void ASMCALL asm_rdmsr(uint32 reg, struct qword_val *qv);
extern void ASMCALL asm_wrmsr(uint32 reg, struct qword_val *qv);
extern void ASMCALL asm_rdtsc(struct qword_val *qv);
#else  // !_M_IX86
extern uint64 ASMCALL asm_rdmsr(uint32 reg);
extern void ASMCALL asm_wrmsr(uint32 reg, uint64_t val);
extern uint64 ASMCALL asm_rdtsc();
#endif  // _M_IX86

uint64 ia32_rdmsr(uint32 reg)
{
#ifdef _M_IX86
    struct qword_val val = { 0 };

    asm_rdmsr(reg, &val);
    return ((uint64)(val.low) | (uint64)(val.high) << 32);
#else
    return asm_rdmsr(reg);
#endif
}

void ia32_wrmsr(uint32 reg, uint64 val)
{
#ifdef _M_IX86
    struct qword_val tmp = { 0 };

    tmp.high = (uint32)(val >> 32);
    tmp.low = (uint32)val;
    asm_wrmsr(reg, &tmp);
#else
    asm_wrmsr(reg, val);
#endif
}

uint64 rdtsc(void)
{
#ifdef _M_IX86
    struct qword_val val = { 0 };
    asm_rdtsc(&val);
    return ((uint64)(val.low) | (uint64)(val.high) << 32);
#else
    return asm_rdtsc();
#endif
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
