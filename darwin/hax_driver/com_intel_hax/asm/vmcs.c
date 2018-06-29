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

#include "../../../../core/include/vmx.h"
#include "../../../../core/include/types.h"
#include "../../../../core/include/vcpu.h"

/* Don't call these two functions as NOT INLINE manner */
#ifdef __i386__
static inline void switch_to_64bit_mode(void)
{
    __asm__ __volatile__ (
        ".code32        \n\t"
        ".byte 0xea     \n\t"
        ".long 1f       \n\t"
        ".word %P0      \n\t"
        ".code64        \n\t"
        "1:"
        :
        : "i" (HAX_KERNEL64_CS)
    );
}

static inline void switch_to_compat_mode(void)
{
    __asm__ __volatile__ (
        ".code64        \n\t"
        "ljmp *(%%rip)  \n\t"
        ".long 2f       \n\t"
        ".word %P0      \n\t"
        ".code32        \n\t"
        "2:"
        :
        : "i" (HAX_KERNEL32_CS)
    );
}
#endif

static uint64 _get_cr3(void)
{
    uint64 val = 0;
    asm volatile (
        "mov %%cr3, %0"
        : "=r" (val)
    );
    return val;
}

uint64 get_cr3(void)
{
#ifdef __x86_64__
    return _get_cr3();
#else
    uint64 val = 0;
    uint64 up = 0;
    uint32 low = 0;

    if (is_compatible()) {
        __asm__ __volatile__ (
            ".code32            \n\t"
            ".byte 0xea         \n\t"
            ".long 1f           \n\t"
            ".word %P2          \n\t"
            ".code64            \n\t"
            "1:                 \n\t"
            "mov %%cr3, %%rax   \n\t"
            "mov %%rax, %%rdx   \n\t"
            "shr $32, %%rdx     \n\t"
            "mov %%edx, %0      \n\t"
            "mov %%eax, %1      \n\t"
            "ljmp *(%%rip)      \n\t"
            "2:                 \n\t"
            ".long 3f           \n\t"
            ".word %P3          \n\t"
            ".code32            \n\t"
            "3:"
            : "=m" (up),
              "=m" (low)
            : "i" (HAX_KERNEL64_CS),
              "i" (HAX_KERNEL32_CS)
            : "memory",
              "cc",
              "rdx"
        );
        val = (uint64)up << 32 | low;
        return val;
    } else
        return _get_cr3();
#endif
}

#ifdef __x86_64__
static inline vmx_error_t vmx_vmxon_64(paddr_t addr)
{
    vmx_error_t eflags = 0;
    asm volatile (
        "vmxon %1       \n\t"
        "pushf          \n\t"
        "pop %0"
        : "=r" (eflags)
        : "m" (addr)
        : "memory"
    );

    return eflags & VMX_FAIL_MASK;
}
#else
static inline vmx_error_t vmx_vmxon_32(paddr_t addr)
{
    vmx_error_t eflags = 0;

    asm volatile (
        "vmxon %4       \n\t"
        "cmovcl %2, %0  \n\t"
        "cmovzl %3, %0"
        : "=&r" (eflags)
        : "0" (VMX_SUCCEED),
          "r" (VMX_FAIL_INVALID),
          "r" (VMX_FAIL_VALID),
          "m" (addr)
        : "memory",
          "cc"
    );
    return eflags;
}
#endif

vmx_error_t __vmxon(uint64 addr)
{
#ifdef __x86_64__
    return (vmx_vmxon_64(addr));
#else
    if (is_compatible()) {
        vmx_error_t result = 0;
        switch_to_64bit_mode();
        result = vmx_vmxon_32(addr);
        switch_to_compat_mode();
        return result;
    } else {
        return (vmx_vmxon_32(addr));
    }
#endif
}

#ifdef __x86_64__
static inline vmx_error_t vmx_vmxoff_64(void)
{
    vmx_error_t eflags = 0;
    asm volatile (
        "vmxoff         \n\t"
        "pushf          \n\t"
        "pop %0"
        : "=r" (eflags)
        :
        : "memory",
          "cc"
    );

    return eflags & VMX_FAIL_MASK;
}
#else
static inline vmx_error_t vmx_vmxoff_32(void)
{
    vmx_error_t eflags = 0;

    asm volatile (
        "vmxoff         \n\t"
        "cmovcl %2, %0  \n\t"
        "cmovzl %3, %0"
        : "=&r" (eflags)
        : "0" (VMX_SUCCEED),
          "r" (VMX_FAIL_INVALID),
          "r" (VMX_FAIL_VALID)
        : "memory",
          "cc"
    );
    return eflags;
}
#endif

vmx_error_t __vmxoff(void)
{
#ifdef __x86_64__
    return (vmx_vmxoff_64());
#else
    if (is_compatible()) {
        vmx_error_t result = 0;
        switch_to_64bit_mode();
        result = vmx_vmxoff_32();
        switch_to_compat_mode();
        return result;
    } else {
        return (vmx_vmxoff_32());
    }
#endif
}

#ifdef __x86_64__
static inline vmx_error_t vmx_vmclear_64(paddr_t address)
{
    vmx_error_t eflags = 0;
    asm volatile (
        "vmclear %1     \n\t"
        "pushf          \n\t"
        "pop %0"
        : "=r" (eflags)
        : "m" (address)
        : "memory"
    );
    return eflags & VMX_FAIL_MASK;
}
#else
static inline vmx_error_t vmx_vmclear_32(paddr_t addr)
{
    vmx_error_t eflags = 0;

    asm volatile (
        "vmclear %4     \n\t"
        "cmovcl %2, %0  \n\t"
        "cmovzl %3, %0"
        : "=&r" (eflags)
        : "0" (VMX_SUCCEED),
          "r" (VMX_FAIL_INVALID),
          "r" (VMX_FAIL_VALID),
          "m" (addr)
        : "memory",
          "cc"
    );
    return eflags;
}
#endif

vmx_error_t __vmclear(uint64 addr)
{
#ifdef __x86_64__
    return (vmx_vmclear_64(addr));
#else
    if (is_compatible()) {
        vmx_error_t result = 0;
        /* Don't put anything between these lines! */
        switch_to_64bit_mode();
        result = vmx_vmclear_32(addr);
        switch_to_compat_mode();
        return result;
    } else {
        return (vmx_vmclear_32(addr));
    }
#endif
}

#ifdef __x86_64__
static inline vmx_error_t vmx_vmptrld_64(paddr_t addr)
{
    vmx_error_t eflags = 0;
    asm volatile (
        "vmptrld %1     \n\t"
        "pushf          \n\t"
        "pop %0"
        : "=r" (eflags)
        : "m" (addr)
        : "memory"
    );
    return eflags & VMX_FAIL_MASK;
}
#else
static inline vmx_error_t vmx_vmptrld_32(paddr_t addr)
{
    vmx_error_t eflags = 0;

    asm volatile (
        "vmptrld %4     \n\t"
        "cmovcl %2, %0  \n\t"
        "cmovzl %3, %0"
        : "=&r" (eflags)
        : "0" (VMX_SUCCEED),
          "r" (VMX_FAIL_INVALID),
          "r" (VMX_FAIL_VALID),
          "m" (addr)
        : "memory",
          "cc"
    );
    return eflags;
}
#endif

vmx_error_t __vmptrld(paddr_t addr)
{
#ifdef __x86_64__
    return (vmx_vmptrld_64(addr));
#else
    if (is_compatible()) {
        vmx_error_t result = 0;
        /* Don't put anything between these lines! */
        switch_to_64bit_mode();
        result = vmx_vmptrld_32(addr);
        switch_to_compat_mode();
        return result;
    } else {
        return (vmx_vmptrld_32(addr));
    }
#endif
}

#ifdef __x86_64__
static inline paddr_t vmx_vmptrst_64(void)
{
    paddr_t address;

    asm volatile (
        "vmptrst %0"
        : "=m" (address)
        :
        : "memory"
    );
    return address;
}
#else
static inline paddr_t vmx_vmptrst_32(void)
{
    paddr_t address;

    asm volatile (
        "vmptrst %0"
        : "=m" (address)
        :
        : "memory",
          "cc"
    );
    return address;
}
#endif

paddr_t __vmptrst(void)
{
#ifdef __x86_64__
    return (vmx_vmptrst_64());
#else
    if (is_compatible()) {
        /* Don't put anything between these lines! */
        paddr_t address;
        switch_to_64bit_mode();
        address = vmx_vmptrst_32();
        switch_to_compat_mode();
        return address;
    } else {
        return (vmx_vmptrst_32());
    }
#endif
}

static inline uint64 ___vmx_vmread(component_index_t component)
{
    mword result;
    asm volatile (
        "vmread %1, %0"
        : "=rm" (result)
        : "r" ((mword)(component))
    );
    return result;
}

#ifdef __i386__
static inline uint64 ___vmx_vmread_64_compatible(component_index_t component)
{
    uint64 result = 0;

    __asm__ __volatile__ (
        ".code32            \n\t"
        ".byte 0xea         \n\t"
        ".long 1f           \n\t"
        ".word %P1          \n\t"
        ".code64            \n\t"
        "1:                 \n\t"
        "pushq %%rbx        \n\t"
        "movq $0, %%rbx     \n\t"
        "mov %3, %%ebx      \n\t"
        "vmread %%rbx, %0   \n\t"
        "popq %%rbx         \n\t"
        "ljmp *(%%rip)      \n\t"
        "2:                 \n\t"
        ".long 3f           \n\t"
        ".word %P2          \n\t"
        ".code32            \n\t"
        "3:"
        : "=m" (result)
        : "i" (HAX_KERNEL64_CS),
          "i" (HAX_KERNEL32_CS),
          "m" ((mword)(component))
        : "memory",
          "cc",
          "rbx"
    );
    return result;
}
#endif

uint64 vmx_vmread(struct vcpu_t *vcpu, component_index_t component)
{
    uint64 val = 0;

#ifdef __x86_64__
    val = ___vmx_vmread(component);
#else
    if (is_compatible()) {
        uint64 value = ___vmx_vmread_64_compatible(component);
        val = value & 0xffffffffULL;
    } else {
        val = ___vmx_vmread(component);
    }
#endif
    return val;
}

uint64 vmx_vmread_natural(struct vcpu_t *vcpu, component_index_t component)
{
    uint64 val;
#ifdef __x86_64__
    val = ___vmx_vmread(component);
#else
    if (is_compatible()) {
        val = ___vmx_vmread_64_compatible(component);
    } else {
        val = ___vmx_vmread(component);
    }
#endif
    return val;
}

uint64 vmx_vmread_64(struct vcpu_t *vcpu, component_index_t component)
{
    uint64 val;
#ifdef __x86_64__
    val = ___vmx_vmread(component);
#else
    if (is_compatible()) {
        val = ___vmx_vmread_64_compatible(component);
    } else {
        val = ___vmx_vmread(component + 1);
        val <<= 32;
        val |= ___vmx_vmread(component);
    }
#endif
    return val;
}

static inline void ___vmx_vmwrite(const char *name, component_index_t component,
                                  mword val)
{
    asm volatile (
        "vmwrite %0, %1"
        :
        : "rm" (val),
          "r" ((mword)(component))
    );
}

#ifdef __i386__
static inline uint64 ___vmx_vmwrite_64_compatible(
        const char *name, component_index_t component, uint64 val)
{
    uint64 eflags = 0;

    __asm__ __volatile__ (
        ".code32            \n\t"
        ".byte 0xea         \n\t"
        ".long 1f           \n\t"
        ".word %P1          \n\t"
        ".code64            \n\t"
        "1:                 \n\t"
        "pushq %%rbx        \n\t"
        "xorq %%rbx, %%rbx  \n\t"
        "mov %4, %%ebx      \n\t"
        "vmwrite %3, %%rbx  \n\t"
        "pushf              \n\t"
        "pop %0             \n\t"
        "popq %%rbx         \n\t"
        "ljmp *(%%rip)      \n\t"
        "2:                 \n\t"
        ".long 3f           \n\t"
        ".word %P2          \n\t"
        ".code32            \n\t"
        "3:"
        : "=m" (eflags)
        : "i" (HAX_KERNEL64_CS),
          "i" (HAX_KERNEL32_CS),
          "m" (val),
          "m" ((mword)(component))
        : "memory",
          "cc",
          "rbx"
    );

    return (eflags & VMX_FAIL_MASK);
}
#endif

void _vmx_vmwrite_natural(struct vcpu_t *vcpu, const char *name,
                          component_index_t component, uint64 source_val)
{
#ifdef __x86_64__
    ___vmx_vmwrite(name, component, source_val);
#endif

#ifdef __i386__
    if (is_compatible()) {
        uint64 result = 0;
        result =  ___vmx_vmwrite_64_compatible(name, component, source_val);
        if (result) {
            printf("vmwrite_natural: com %x, val %llx, result %llx\n",
                   component, source_val, result);
            panic();
        }
    } else {
        ___vmx_vmwrite(name, component, source_val);
    }
#endif
}

void _vmx_vmwrite_64(struct vcpu_t *vcpu, const char *name,
                     component_index_t component, uint64 source_val)
{
#ifdef __x86_64__
    ___vmx_vmwrite(name, component, source_val);
#endif

#ifdef __i386__
    if (is_compatible()) {
        uint64 result = 0;
        result = ___vmx_vmwrite_64_compatible(name, component, source_val);
        if (result) {
            printf("vmwrite_64: com %x, val %llx, result %llx\n", component,
                   source_val, result);
            panic();
        }
    } else {
        ___vmx_vmwrite(name, component, source_val);
        ___vmx_vmwrite(name, component + 1, source_val >> 32);
    }
#endif
}

void _vmx_vmwrite(struct vcpu_t *vcpu, const char *name,
                  component_index_t component, mword source_val)
{
#ifdef __x86_64__
    ___vmx_vmwrite(name, component, source_val);
#else
    if (is_compatible()) {
        uint64 result = 0, val = 0;
        val |= source_val;
        result = ___vmx_vmwrite_64_compatible(name, component, val);
        if (result) {
            printf("vmwrite com %x, val %lx\n", component, source_val);
            panic();
        }
    } else {
        ___vmx_vmwrite(name, component, source_val);
    }
#endif
}

static inline void vmcall(void)
{
    asm volatile (
        "vmcall"
        :
        :
        : "memory"
    );
}

void __vmcall(void)
{
#ifdef __x86_64__
    vmcall();
#else
    if (is_compatible()) {
        /* Don't put anything between these lines! */
        switch_to_64bit_mode();
        vmcall();
        switch_to_compat_mode();
    } else {
        return vmcall();
    }
#endif
}

static inline vmx_error_t ___invept(uint type, struct invept_desc *desc)
{
    vmx_error_t eflags = 0;
#if 1
    // Hard-code the instruction because INVEPT is not recognized by Xcode.
    // 0x08 is the ModR/M byte, specifying *CX as the register operand and *AX
    // as the memory operand (see IASDM Vol. 2A 2.1.5, Table 2-2)
#define IA32_INVEPT_OPCODE ".byte 0x66, 0x0f, 0x38, 0x80, 0x08"
    asm volatile (
        IA32_INVEPT_OPCODE "\n\t"
        "pushf              \n\t"
        "pop %0"
        : "=d" (eflags)
        : "c" (type),
          "a" (desc)
        : "memory"
    );
#else
    asm volatile (
        "invept %1, %2      \n\t"
        "pushf              \n\t"
        "pop %0"
        : "=r" (eflags)
        : "r" (type),
          "m" (desc)
        : "memory"
    );
#endif
    return eflags & VMX_FAIL_MASK;
}

#ifdef __i386__  // Obsolete, because 32-bit Mac is no longer supported
static inline void ___invept_compatible(uint type, struct invept_desc *desc)
{
#if 1
    __asm__ __volatile__ (
        ".code32                            \n\t"
        ".byte 0xea                         \n\t"
        ".long 1f                           \n\t"
        ".word %P0                          \n\t"
        ".code64                            \n\t"
        "1:                                 \n\t"
        "mov %2, %%rcx                      \n\t"
        "mov %3, %%rax                      \n\t"
        ".byte 0x66, 0x0f, 0x38, 0x80, 0x08 \n\t"
        "ljmp *(%%rip)                      \n\t"
        "2:                                 \n\t"
        ".long 3f                           \n\t"
        ".word %P1                          \n\t"
        ".code32                            \n\t"
        "3:"
        :
        : "i" (HAX_KERNEL64_CS),
          "i" (HAX_KERNEL32_CS),
          "m" (type),
          "m" (desc)
    );
#else
    __asm__ __volatile__ (
        ".code32                            \n\t"
        ".byte 0xea                         \n\t"
        ".long 1f                           \n\t"
        ".word %P0                          \n\t"
        ".code64                            \n\t"
        "1:                                 \n\t"
        "mov %P2, %%rax                     \n\t"
        "invept %P2, %P3                    \n\t"
        "ljmp *(%%rip)                      \n\t"
        "2:                                 \n\t"
        ".long 3f                           \n\t"
        ".word %P1                          \n\t"
        ".code32                            \n\t"
        "3:"
        :
        : "i" (HAX_KERNEL64_CS),
          "i" (HAX_KERNEL32_CS),
          "m" (type),
          "m" (desc)
        : "memory"
    );
#endif
}
#endif

vmx_error_t __invept(uint type, struct invept_desc *desc)
{
#ifdef __x86_64__
    return ___invept(type, desc);
#else  // obsolete, because 32-bit Mac is no longer supported
    if (is_compatible()) {
        ___invept_compatible(type, desc);
        // Just return a fake value (this code path is never taken anyway)
        return (vmx_error_t) -1;
    } else {
        return ___invept(type, desc);
    }
#endif
}

mword get_rip(void)
{
    mword host_rip;
#ifdef __x86_64__
    asm volatile (
        "leaq EXIT_ENTRY(%%rip), %0"
        : "=r" (host_rip)
    );
#else
    asm volatile (
        "movl $EXIT_ENTRY, %0"
        : "=r" (host_rip)
    );
#endif
    return host_rip;
}

uint64 __vmx_run(struct vcpu_state_t *state, uint16 launched)
{
    uint64 rflags = 0;

#ifdef __x86_64__
    asm volatile (
        "pushfq                             \n\t"
        "pushq %%r8                         \n\t"
        "pushq %%r9                         \n\t"
        "pushq %%r10                        \n\t"
        "pushq %%r11                        \n\t"
        "pushq %%r12                        \n\t"
        "pushq %%r13                        \n\t"
        "pushq %%r14                        \n\t"
        "pushq %%r15                        \n\t"
        "pushq %%rcx                        \n\t"
        "pushq %%rdx                        \n\t"
        "pushq %%rsi                        \n\t"
        "pushq %%rdi                        \n\t"
        "pushq %%rbp                        \n\t"
        "pushq %%rax                        \n\t"
        "pushq %%rbx                        \n\t"
        "movq $0x6c14, %%rbx                \n\t"
        "movq %%rsp, %%rax                  \n\t"
        "subq $8, %%rax                     \n\t"
        "vmwrite %%rax, %%rbx               \n\t"
        "popq %%rbx                         \n\t"
        "popq %%rax                         \n\t"
        "pushq %%rax                        \n\t"
        "pushq %%rbx                        \n\t"
        "pushq %3                           \n\t"
        "cmpl $1, %2                        \n\t"
        "movq 0x8(%%rax),  %%rcx            \n\t"
        "movq 0x10(%%rax), %%rdx            \n\t"
        "movq 0x18(%%rax), %%rbx            \n\t"
        "movq 0x28(%%rax), %%rbp            \n\t"
        "movq 0x30(%%rax), %%rsi            \n\t"
        "movq 0x38(%%rax), %%rdi            \n\t"
        "movq 0x40(%%rax), %%r8             \n\t"
        "movq 0x48(%%rax), %%r9             \n\t"
        "movq 0x50(%%rax), %%r10            \n\t"
        "movq 0x58(%%rax), %%r11            \n\t"
        "movq 0x60(%%rax), %%r12            \n\t"
        "movq 0x68(%%rax), %%r13            \n\t"
        "movq 0x70(%%rax), %%r14            \n\t"
        "movq 0x78(%%rax), %%r15            \n\t"
        "movq 0x00(%%rax), %%rax            \n\t"
        "je RESUME                          \n\t"
        "vmlaunch                           \n\t"
        "jmp EXIT_ENTRY_FAIL                \n\t"
        "RESUME:                            \n\t"
        "vmresume                           \n\t"
        "jmp EXIT_ENTRY_FAIL                \n\t"
        "EXIT_ENTRY:                        \n\t"
        "push %%rdi                         \n\t"
        "movq 0x8(%%rsp), %%rdi             \n\t"
        "movq %%rax, 0x00(%%rdi)            \n\t"
        "movq %%rcx, 0x08(%%rdi)            \n\t"
        "movq %%rdx, 0x10(%%rdi)            \n\t"
        "popq %%rcx                         \n\t"
        "movq %%rbx, 0x18(%%rdi)            \n\t"
        "movq %%rbp, 0x28(%%rdi)            \n\t"
        "movq %%rsi, 0x30(%%rdi)            \n\t"
        "movq %%rcx, 0x38(%%rdi)            \n\t"
        "movq %%r8,  0x40(%%rdi)            \n\t"
        "movq %%r9,  0x48(%%rdi)            \n\t"
        "movq %%r10, 0x50(%%rdi)            \n\t"
        "movq %%r11, 0x58(%%rdi)            \n\t"
        "movq %%r12, 0x60(%%rdi)            \n\t"
        "movq %%r13, 0x68(%%rdi)            \n\t"
        "movq %%r14, 0x70(%%rdi)            \n\t"
        "movq %%r15, 0x78(%%rdi)            \n\t"
        "EXIT_ENTRY_FAIL:                   \n\t"
        "popq %%rbx                         \n\t"
        "popq %%rbx                         \n\t"
        "popq %%rax                         \n\t"
        "popq %%rbp                         \n\t"
        "popq %%rdi                         \n\t"
        "popq %%rsi                         \n\t"
        "popq %%rdx                         \n\t"
        "popq %%rcx                         \n\t"
        "popq %%r15                         \n\t"
        "popq %%r14                         \n\t"
        "popq %%r13                         \n\t"
        "popq %%r12                         \n\t"
        "popq %%r11                         \n\t"
        "popq %%r10                         \n\t"
        "popq %%r9                          \n\t"
        "popq %%r8                          \n\t"
        "pushf                              \n\t"
        "pop %0                             \n\t"
        "popfq"
        : "=m" (rflags)
        : "a" (state),
          "b" ((uint32)launched),
          "m" (state)
    );
#else
#define HAX_KERNEL32_CS 0x08
#define HAX_KERNEL64_CS 0x80
    asm volatile (
        ".code32                            \n\t"
        ".byte 0xea                         \n\t"
        ".long 1f                           \n\t"
        ".word 0x80                         \n\t"
        ".code64                            \n\t"
        "1:                                 \n\t"
        "pushfq                             \n\t"
        "pushq %%r8                         \n\t"
        "pushq %%r9                         \n\t"
        "pushq %%r10                        \n\t"
        "pushq %%r11                        \n\t"
        "pushq %%r12                        \n\t"
        "pushq %%r13                        \n\t"
        "pushq %%r14                        \n\t"
        "pushq %%r15                        \n\t"
        "pushq %%rcx                        \n\t"
        "pushq %%rdx                        \n\t"
        "pushq %%rsi                        \n\t"
        "pushq %%rdi                        \n\t"
        "pushq %%rbp                        \n\t"
        "pushq %%rax                        \n\t"
        "pushq %%rbx                        \n\t"
        "movq $0x6c14, %%rbx                \n\t"
        "movq %%rsp, %%rax                  \n\t"
        "subq $8, %%rax                     \n\t"
        "vmwrite %%rax, %%rbx               \n\t"
        "popq %%rbx                         \n\t"
        "popq %%rax                         \n\t"
        "pushq %%rax                        \n\t"
        "pushq %%rbx                        \n\t"
        "xorq %%rbx, %%rbx                  \n\t"
        "movq $0x00000000ffffffff, %%rbx    \n\t"
        "andq %%rbx, %%rax                  \n\t"
        "popq %%rbx                         \n\t"
        "cmpl $1, %2                        \n\t"
        "pushq %%rbx                        \n\t"
        "pushq %3                           \n\t"
        "movq 0x8(%%rax),  %%rcx            \n\t"
        "movq 0x10(%%rax), %%rdx            \n\t"
        "movq 0x18(%%rax), %%rbx            \n\t"
        "movq 0x28(%%rax), %%rbp            \n\t"
        "movq 0x30(%%rax), %%rsi            \n\t"
        "movq 0x38(%%rax), %%rdi            \n\t"
        "movq 0x40(%%rax), %%r8             \n\t"
        "movq 0x48(%%rax), %%r9             \n\t"
        "movq 0x50(%%rax), %%r10            \n\t"
        "movq 0x58(%%rax), %%r11            \n\t"
        "movq 0x60(%%rax), %%r12            \n\t"
        "movq 0x68(%%rax), %%r13            \n\t"
        "movq 0x70(%%rax), %%r14            \n\t"
        "movq 0x78(%%rax), %%r15            \n\t"
        "movq 0x00(%%rax), %%rax            \n\t"
        "je RESUME                          \n\t"
        "vmlaunch                           \n\t"
        "jmp EXIT_ENTRY_FAIL                \n\t"
        "RESUME:                            \n\t"
        "vmresume                           \n\t"
        "jmp EXIT_ENTRY_FAIL                \n\t"
        "EXIT_ENTRY:                        \n\t"
        "push %%rdi                         \n\t"
        "movq $0, %%rdi                     \n\t"
        "movl 0x8(%%rsp), %%edi             \n\t"
        "movq %%rax, 0x00(%%rdi)            \n\t"
        "movq %%rcx, 0x08(%%rdi)            \n\t"
        "movq %%rdx, 0x10(%%rdi)            \n\t"
        "pop %%rcx                          \n\t"
        "movq %%rbx, 0x18(%%rdi)            \n\t"
        "movq %%rbp, 0x28(%%rdi)            \n\t"
        "movq %%rsi, 0x30(%%rdi)            \n\t"
        "movq %%rcx, 0x38(%%rdi)            \n\t"
        "movq %%r8,  0x40(%%rdi)            \n\t"
        "movq %%r9,  0x48(%%rdi)            \n\t"
        "movq %%r10, 0x50(%%rdi)            \n\t"
        "movq %%r11, 0x58(%%rdi)            \n\t"
        "movq %%r12, 0x60(%%rdi)            \n\t"
        "movq %%r13, 0x68(%%rdi)            \n\t"
        "movq %%r14, 0x70(%%rdi)            \n\t"
        "movq %%r15, 0x78(%%rdi)            \n\t"
        "EXIT_ENTRY_FAIL:                   \n\t"
        "popq %%rbx                         \n\t"
        "popq %%rbx                         \n\t"
        "popq %%rax                         \n\t"
        "popq %%rbp                         \n\t"
        "popq %%rdi                         \n\t"
        "popq %%rsi                         \n\t"
        "popq %%rdx                         \n\t"
        "popq %%rcx                         \n\t"
        "popq %%r15                         \n\t"
        "popq %%r14                         \n\t"
        "popq %%r13                         \n\t"
        "popq %%r12                         \n\t"
        "popq %%r11                         \n\t"
        "popq %%r10                         \n\t"
        "popq %%r9                          \n\t"
        "popq %%r8                          \n\t"
        "pushf                              \n\t"
        "pop %0                             \n\t"
        "popfq                              \n\t"
        "ljmp *(%%rip)                      \n\t"
        "2:                                 \n\t"
        ".long 3f                           \n\t"
        ".word 0x08                         \n\t"
        ".code32                            \n\t"
        "3:"
        : "=m" (rflags)
        : "a" (state),
          "b" ((uint32)launched),
          "m" (state));
#endif
    return rflags;
}
