;
; Copyright (c) 2011 Intel Corporation
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
;   1. Redistributions of source code must retain the above copyright notice,
;      this list of conditions and the following disclaimer.
;
;   2. Redistributions in binary form must reproduce the above copyright
;      notice, this list of conditions and the following disclaimer in the
;      documentation and/or other materials provided with the distribution.
;
;   3. Neither the name of the copyright holder nor the names of its
;      contributors may be used to endorse or promote products derived from
;      this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
; ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
; LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
; CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
; SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
; INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
; CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
; ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
; POSSIBILITY OF SUCH DAMAGE.
;

option casemap:none

VCPU_STATE STRUCT
    _rax    QWORD   ?
    _rcx    QWORD   ?
    _rdx    QWORD   ?
    _rbx    QWORD   ?
    _rsp    QWORD   ?
    _rbp    QWORD   ?
    _rsi    QWORD   ?
    _rdi    QWORD   ?
    _r8     QWORD   ?
    _r9     QWORD   ?
    _r10    QWORD   ?
    _r11    QWORD   ?
    _r12    QWORD   ?
    _r13    QWORD   ?
    _r14    QWORD   ?
    _r15    QWORD   ?
VCPU_STATE ENDS

.data
;

.code

asm_disable_irq PROC public
    cli
    ret
asm_disable_irq ENDP

asm_enable_irq PROC public
    sti
    ret
asm_enable_irq ENDP

get_cr0 PROC public
    xor rax, rax
    mov rax, cr0
    ret
get_cr0 ENDP

get_cr2 PROC public
    xor rax, rax
    mov rax, cr2
    ret
get_cr2 ENDP

get_cr3 PROC public
    xor rax, rax
    mov rax, cr3
    ret
get_cr3 ENDP

get_cr4 PROC public
    xor rax, rax
    mov rax, cr4
    ret
get_cr4 ENDP

get_dr0 PROC public
    xor rax, rax
    mov rax, dr0
    ret
get_dr0 ENDP

get_dr1 PROC public
    xor rax, rax
    mov rax, dr1
    ret
get_dr1 ENDP

get_dr2 PROC public
    xor rax, rax
    mov rax, dr2
    ret
get_dr2 ENDP

get_dr3 PROC public
    xor rax, rax
    mov rax, dr3
    ret
get_dr3 ENDP

get_dr6 PROC public
    xor rax, rax
    mov rax, dr6
    ret
get_dr6 ENDP

get_dr7 PROC public
    xor rax, rax
    mov rax, dr7
    ret
get_dr7 ENDP

set_cr0 PROC public
    mov cr0, rcx
    ret
set_cr0 ENDP

set_cr2 PROC public
    mov cr2, rcx
    ret
set_cr2 ENDP

set_cr3 PROC public
    mov cr3, rcx
    ret
set_cr3 ENDP

set_cr4 PROC public
    mov cr4, rcx
    ret
set_cr4 ENDP

set_dr0 PROC public
    mov dr0, rcx
    ret
set_dr0 ENDP

set_dr1 PROC public
    mov dr1, rcx
    ret
set_dr1 ENDP

set_dr2 PROC public
    mov dr2, rcx
    ret
set_dr2 ENDP

set_dr3 PROC public
    mov dr3, rcx
    ret
set_dr3 ENDP

set_dr6 PROC public
    mov dr6, rcx
    ret
set_dr6 ENDP

set_dr7 PROC public
    mov dr7, rcx
    ret
set_dr7 ENDP

get_kernel_cs PROC public
    xor rax, rax
    mov ax, cs
    ret
get_kernel_cs ENDP

get_kernel_ds PROC public
    xor rax, rax
    mov ax, ds
    ret
get_kernel_ds ENDP

get_kernel_es PROC public
    xor rax, rax
    mov ax, es
    ret
get_kernel_es ENDP

get_kernel_ss PROC public
    xor rax, rax
    mov ax, ss
    ret
get_kernel_ss ENDP

get_kernel_gs PROC public
    xor rax, rax
    mov ax, gs
    ret
get_kernel_gs ENDP

get_kernel_fs PROC public
    xor rax, rax
    mov ax, fs
    ret
get_kernel_fs ENDP

set_kernel_ds PROC public
    mov ds, cx
    ret
set_kernel_ds ENDP

set_kernel_es PROC public
    mov es, cx
    ret
set_kernel_es ENDP

set_kernel_gs PROC public
    mov gs, cx
    ret
set_kernel_gs ENDP

set_kernel_fs PROC public
    mov fs, cx
    ret
set_kernel_fs ENDP

asm_rdmsr PROC public
    xor rax, rax
    xor rdx, rdx
    rdmsr
    mov cl, 32
    shl rdx, cl
    or rax, rdx
    ret
asm_rdmsr ENDP

asm_wrmsr PROC public
    push rbx
    xor rax, rax
    xor rbx, rbx
    ;mov 1st para to rbx
    mov rbx, rcx
    mov cl, 32
    ;mov 2nd para to rax
    ;got rax
    mov rax, rdx
    shl rax, cl
    shr rax, cl
    ;got rdx
    shr rdx, cl
    mov rcx, rbx
    wrmsr
    pop rbx
    ret
asm_wrmsr ENDP

asm_rdtsc PROC public
    xor rax, rax
    xor rdx, rdx
    xor rcx, rcx
    rdtsc
    mov cl, 32
    shl rdx, cl
    or rax, rdx
    ret
asm_rdtsc ENDP

asm_fxinit PROC public
    finit
    ret
asm_fxinit ENDP

asm_fxsave PROC public
    fxsave [rcx]
    ret
asm_fxsave ENDP

asm_fxrstor PROC public
    fxrstor [rcx]
    ret
asm_fxrstor ENDP

asm_btr PROC public
    lock btr [rcx], rdx
    ret
asm_btr ENDP

asm_bts PROC public
    lock bts [rcx], rdx
    ret
asm_bts ENDP

cpu_has_emt64_support PROC public
    push rbx
    xor rax, rax
    xor rcx, rcx
    xor rdx, rdx
    xor rbx, rbx
    mov rax, 80000001h
    cpuid
    mov eax, edx
    test eax, 20000000h
    pushf
    pop ax
    and eax, 40h
    xor edx, edx
    mov cl, 6h
    shr eax, cl
    xor eax, 1h
    pop rbx
    ret
cpu_has_emt64_support ENDP

cpu_has_vmx_support PROC public
    push rbx
    xor rax, rax
    xor rcx, rcx
    xor rdx, rdx
    xor rbx, rbx
    mov rax, 1h
    cpuid
    mov eax, ecx
    test eax, 20h
    pushf
    pop ax
    and eax, 40h
    xor ecx, ecx
    mov cl, 6h
    shr eax, cl
    xor eax, 1h
    pop rbx
    ret
cpu_has_vmx_support ENDP

cpu_has_nx_support PROC public
    push rbx
    xor rax, rax
    xor rcx, rcx
    xor rdx, rdx
    xor rbx, rbx
    mov rax, 80000001h
    cpuid
    mov eax, edx
    test eax, 100000h
    pushf
    pop ax
    and eax, 40h
    xor edx, edx
    mov cl, 6h
    shr eax, cl
    xor eax, 1h
    pop rbx
    ret
cpu_has_nx_support ENDP

get_kernel_rflags PROC public
    xor rax, rax
    pushf
    pop ax
    ret
get_kernel_rflags ENDP

__nmi PROC public
    int 2h
    ret
__nmi ENDP

__fls PROC public
    xor eax, eax
    bsr eax, ecx
    ret
__fls ENDP

__handle_cpuid PROC public
    push rbx
    xor rax, rax
    xor rbx, rbx
    xor rdx, rdx
    xor r8, r8
    mov r8, rcx
    mov rax, [r8].VCPU_STATE._rax
    mov rcx, [r8].VCPU_STATE._rcx
    cpuid
    mov [r8].VCPU_STATE._rax, rax
    mov [r8].VCPU_STATE._rbx, rbx
    mov [r8].VCPU_STATE._rcx, rcx
    mov [r8].VCPU_STATE._rdx, rdx
    pop rbx
    ret
__handle_cpuid ENDP

end
