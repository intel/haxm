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
#include "../../../../core/include/compiler.h"
#include "../../../../core/include/ia32.h"
#include "../../../../include/hax.h"

void set_kernel_gdt(system_desc_t *sys_desc)
{
#ifdef __x86_64__
    asm ("lgdt %0"
         : "=m" (*sys_desc)
    );
#else
    if (is_compatible()) {
        __asm__ __volatile__ (
            ".code32        \n\t"
            ".byte 0xea     \n\t"
            ".long 1f       \n\t"
            ".word %P1      \n\t"
            ".code64        \n\t"
            "1:             \n\t"
            "lgdt %0        \n\t"
            "ljmp *(%%rip)  \n\t"
            "4:             \n\t"
            ".long 5f       \n\t"
            ".word %P2      \n\t"
            ".code32        \n\t"
            "5:"
            : "=m" (*sys_desc)
            : "i" (HAX_KERNEL64_CS),
              "i" (HAX_KERNEL32_CS)
            : "memory",
              "cc"
        );
    } else {
        asm ("lgdt %0"
             : "=m" (*sys_desc)
        );
    }
#endif
}

void set_kernel_idt(system_desc_t *sys_desc)
{
#ifdef __x86_64__
    asm ("lidt %0"
         : "=m" (*sys_desc)
    );
#else
    if (is_compatible()) {
        __asm__ __volatile__ (
            ".code32        \n\t"
            ".byte 0xea     \n\t"
            ".long 1f       \n\t"
            ".word %P1      \n\t"
            ".code64        \n\t"
            "1:             \n\t"
            "lidt %0        \n\t"
            "ljmp *(%%rip)  \n\t"
            "4:             \n\t"
            ".long 5f       \n\t"
            ".word %P2      \n\t"
            ".code32        \n\t"
            "5:"
            : "=m" (*sys_desc)
            : "i" (HAX_KERNEL64_CS),
              "i" (HAX_KERNEL32_CS)
            : "memory",
              "cc"
        );
    } else {
        asm ("lidt %0"
             : "=m" (*sys_desc)
        );
    }
#endif
}

void get_kernel_gdt(system_desc_t *sys_desc)
{
#ifdef __x86_64__
    asm ("sgdt %0"
         : "=m" (*sys_desc)
    );
#else
    if (is_compatible()) {
        __asm__ __volatile__ (
            ".code32        \n\t"
            ".byte 0xea     \n\t"
            ".long 1f       \n\t"
            ".word %P1      \n\t"
            ".code64        \n\t"
            "1:             \n\t"
            "sgdt %0        \n\t"
            "ljmp *(%%rip)  \n\t"
            "4:             \n\t"
            ".long 5f       \n\t"
            ".word %P2      \n\t"
            ".code32        \n\t"
            "5:"
            : "=m" (*sys_desc)
            : "i" (HAX_KERNEL64_CS),
              "i" (HAX_KERNEL32_CS)
            : "memory",
              "cc"
        );
    } else {
        asm ("sgdt %0"
             : "=m" (*sys_desc)
        );
    }
#endif
}

void get_kernel_idt(system_desc_t *sys_desc)
{
#ifdef __x86_64__
    asm ("sidt %0"
         : "=m" (*sys_desc)
    );
#else
    if (is_compatible()) {
        __asm__ __volatile__ (
            ".code32        \n\t"
            ".byte 0xea     \n\t"
            ".long 1f       \n\t"
            ".word %P1      \n\t"
            ".code64        \n\t"
            "1:             \n\t"
            "sidt %0        \n\t"
            "ljmp *(%%rip)  \n\t"
            "4:             \n\t"
            ".long 5f       \n\t"
            ".word %P2      \n\t"
            ".code32        \n\t"
            "5:"
            : "=m" (*sys_desc)
            : "i" (HAX_KERNEL64_CS),
              "i" (HAX_KERNEL32_CS)
            : "memory",
              "cc"
        );
    } else {
        asm ("sidt %0"
             : "=m" (*sys_desc)
        );
    }
#endif
}

void load_kernel_ldt(uint16 sel)
{
#ifdef __x86_64__
    asm ("lldt %0"
         :
         : "m" (sel)
    );
#else
    if (is_compatible()) {
        __asm__ __volatile__ (
            ".code32        \n\t"
            ".byte 0xea     \n\t"
            ".long 1f       \n\t"
            ".word %P0      \n\t"
            ".code64        \n\t"
            "1:             \n\t"
            "lldt %2        \n\t"
            "ljmp *(%%rip)  \n\t"
            "4:             \n\t"
            ".long 5f       \n\t"
            ".word %P1      \n\t"
            ".code32        \n\t"
            "5:"
            :
            : "i" (HAX_KERNEL64_CS),
              "i" (HAX_KERNEL32_CS),
              "m" (sel)
            : "memory",
              "cc"
        );
    } else {
        asm ("lldt %0"
             :
             : "m" (sel)
        );
    }
#endif
}

uint16 get_kernel_tr_selector(void)
{
    uint16 selector, *sel;
    sel = &selector;

    asm ("str %0"
         : "=m" (*sel)
    );
    return selector;
}

uint16 get_kernel_ldt(void)
{
    uint16 selector, *sel;
    sel = &selector;

    asm ("sldt %0"
         : "=m" (*sel)
    );
    return selector;
}
