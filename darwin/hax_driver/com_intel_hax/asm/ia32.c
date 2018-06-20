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

#include "../../../../core/include/types.h"
#include "../../../../core/include/segments.h"
#include "../../../../core/include/ia32.h"
#include "../../../../core/include/vcpu.h"
#include "../../../../core/include/cpuid.h"
#include "../../../../include/hax.h"

mword get_cr0(void)
{
    mword val;
    asm volatile (
        "mov %%cr0, %0"
        : "=r" (val)
    );
    return val;
}

mword get_cr2(void)
{
    mword val;
    asm volatile (
        "mov %%cr2, %0"
        : "=r" (val)
    );
    return val;
}

mword get_cr4(void)
{
    mword val;
    asm volatile (
        "mov %%cr4, %0"
        : "=r" (val)
    );
    return val;
}

mword get_dr0(void)
{
    mword val;
    asm volatile (
        "mov %%dr0, %0"
        : "=r" (val)
    );
    return val;
}

mword get_dr1(void)
{
    mword val;
    asm volatile (
        "mov %%dr1, %0"
        : "=r" (val)
    );
    return val;
}

mword get_dr2(void)
{
    mword val;
    asm volatile (
        "mov %%dr2, %0"
        : "=r" (val)
    );
    return val;
}

mword get_dr3(void)
{
    mword val;
    asm volatile (
        "mov %%dr3, %0"
        : "=r" (val)
    );
    return val;
}

mword get_dr6(void)
{
    mword val;
    asm volatile (
        "mov %%dr6, %0"
        : "=r" (val)
    );
    return val;
}

mword get_dr7(void)
{
    mword val;
    asm volatile (
        "mov %%dr7, %0"
        : "=r" (val)
    );
    return val;
}

void set_cr0(mword val)
{
    asm volatile (
        "mov %0, %%cr0"
        :
        : "r" (val)
    );
}

void set_cr2(mword val)
{
    asm volatile (
        "mov %0, %%cr2"
        :
        : "r" (val)
    );
}

void set_cr3(mword val)
{
    asm volatile (
        "mov %0, %%cr3"
        :
        : "r" (val)
    );
}

void set_cr4(mword val)
{
    asm volatile (
        "mov %0, %%cr4"
        :
        : "r" (val)
    );
}

void set_dr0(mword val)
{
    asm volatile (
        "mov %0, %%dr0"
        :
        : "r" (val)
    );
}

void set_dr1(mword val)
{
    asm volatile (
        "mov %0, %%dr1"
        :
        : "r" (val)
    );
}

void set_dr2(mword val)
{
    asm volatile (
        "mov %0, %%dr2"
        :
        : "r" (val)
    );
}

void set_dr3(mword val)
{
    asm volatile (
        "mov %0, %%dr3"
        :
        : "r" (val)
    );
}

void set_dr6(mword val)
{
    asm volatile (
        "mov %0, %%dr6"
        :
        : "r" (val)
    );
}

void set_dr7(mword val)
{
    asm volatile (
        "mov %0, %%dr7"
        :
        : "r" (val)
    );
}

uint16 get_kernel_cs(void)
{
    mword cs;
    asm volatile (
        "mov %%cs, %0"
        : "=r" (cs)
    );
    return cs;
}

uint16 get_kernel_ds(void)
{
    mword ds;
    asm volatile (
        "mov %%ds, %0"
        : "=r" (ds)
    );
    return ds;
}

uint16 get_kernel_es(void)
{
    mword es;
    asm volatile (
        "mov %%es, %0"
        : "=r" (es)
    );
    return es;
}

uint16 get_kernel_ss(void)
{
    mword ss;
    asm volatile (
        "mov %%ss, %0"
        : "=r" (ss)
    );
    return ss;
}

uint16 get_kernel_gs(void)
{
    mword gs;
    asm volatile (
        "mov %%gs, %0"
        : "=r" (gs)
    );
    return gs;
}

void set_kernel_gs(uint16 gs)
{
    asm volatile (
        "mov %0, %%gs"
        :
        : "r" (gs)
    );
}

void set_kernel_ds(uint16 ds)
{
    asm volatile (
        "mov %0, %%ds"
        :
        : "r" (ds)
    );
}

void set_kernel_es(uint16 es)
{
    asm volatile (
        "mov %0, %%es"
        :
        : "r" (es)
    );
}

void set_kernel_fs(uint16 fs)
{
    asm volatile (
        "mov %0, %%fs"
        :
        : "r" (fs)
    );
}

uint16 get_kernel_fs(void)
{
    mword fs;
    asm volatile (
        "mov %%fs, %0"
        : "=r" (fs)
    );
    return fs;
}

void ia32_wrmsr(uint32 reg, uint64 val)
{
    asm volatile (
        "wrmsr"
        :
        : "c" (reg),
          "d" ((uint32)(val >> 32)),
          "a" ((uint32)val)
    );
}

uint64 ia32_rdmsr(uint32 reg)
{
    uint32 a, d;
    asm volatile (
        "rdmsr"
        : "=a" (a),
          "=d" (d)
        : "c" (reg)
    );
    return ((uint64)d << 32) | (uint64)a;
}

uint64 rdtsc(void)
{
    mword a, d;
    asm volatile (
        "rdtsc"
        : "=a" (a),
          "=d" (d)
    );
    return ((uint64)d << 32) | (uint64)a;
}

void fxsave(unsigned long *addr)
{
    asm volatile (
        "fxsave %0"
        :
        : "m" (*addr)
    );
}

void fxrstor(unsigned long *addr)
{
    asm volatile (
        "fxrstor %0"
        :
        : "m" (*addr)
    );
}

void btr(uint8 *addr, uint bit)
{
    // bitrl may be able to handle large bit offsets. Nevertheless, use a small
    // offset (i.e. less than 8) as the Windows wrappers do, just to be on the
    // safe side.
    uint8 *base = addr + bit / 8;
    uint offset = bit % 8;

    // C.f. the first code sample in section 6.45.2.3 (Output Operands) of
    // https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
    asm volatile (
        "lock; btrl %1, %0"
        : "+m" (*base)
        : "Ir" (offset)
        : "cc"
    );
}

void bts(uint8 *addr, uint bit)
{
    uint8 *base = addr + bit / 8;
    uint offset = bit % 8;

    asm volatile (
        "lock; btsl %1, %0"
        : "+m" (*base)
        : "Ir" (offset)
        : "cc"
    );
}

void __handle_cpuid(union cpuid_args_t *args)
{
    uint32 a = args->eax, c = args->ecx;

    asm ("cpuid"
         : "=a" (args->eax),
           "=c" (args->ecx),
           "=b" (args->ebx),
           "=d" (args->edx)
         : "0" (a),
           "1" (c)
    );
}

uint64 get_kernel_rflags(void)
{
    mword flags;
#ifdef __x86_64__
    asm volatile (
        "pushfq             \n\t"
        "popq %0            \n\t"
        : "=r" (flags)
    );
#else
    asm volatile (
        "pushfd             \n\t"
        "pop %0             \n\t"
        : "=r" (flags)
    );
#endif
    return flags;
}

void __nmi(void)
{
    asm ("int $2");
}

uint32 __fls(uint32 bit32)
{
    asm ("bsr %1, %0"
         : "=r" (bit32)
         : "rm" (bit32)
    );
    return bit32;
}
