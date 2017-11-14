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

.686p
.mmx
.xmm
.model flat, stdcall
option casemap:none

QWORD_STRUCT STRUCT
    _low    DWORD   ?
    _high   DWORD   ?
QWORD_STRUCT ENDS

VCPU_STATE STRUCT
    _eax    DWORD   ?
    _pad0   DWORD   ?
    _ecx    DWORD   ?
    _pad1   DWORD   ?
    _edx    DWORD   ?
    _pad2   DWORD   ?
    _ebx    DWORD   ?
    _pad3   DWORD   ?
    _esp    DWORD   ?
    _pad4   DWORD   ?
    _ebp    DWORD   ?
    _pad5   DWORD   ?
    _esi    DWORD   ?
    _pad6   DWORD   ?
    _edi    DWORD   ?
    _pad7   DWORD   ?
VCPU_STATE ENDS

.data
;

.code
start:

get_cr0 PROC PUBLIC
    xor eax, eax
    mov eax, cr0
    ret
get_cr0 ENDP

get_cr2 PROC PUBLIC
    xor eax, eax
    mov eax, cr2
    ret
get_cr2 ENDP

get_cr3 PROC PUBLIC
    xor eax, eax
    mov eax, cr3
    ret
get_cr3 ENDP

get_cr4 PROC PUBLIC
    xor eax, eax
    mov eax, cr4
    ret
get_cr4 ENDP

asm_disable_irq PROC C public
    cli
    ret
asm_disable_irq ENDP

asm_enable_irq PROC C public
    sti
    ret
asm_enable_irq ENDP

get_dr0 PROC PUBLIC
    xor eax, eax
    mov eax, dr0
    ret
get_dr0 ENDP

get_dr1 PROC PUBLIC
    xor eax, eax
    mov eax, dr1
    ret
get_dr1 ENDP

get_dr2 PROC PUBLIC
    xor eax, eax
    mov eax, dr2
    ret
get_dr2 ENDP

get_dr3 PROC PUBLIC
    xor eax, eax
    mov eax, dr3
    ret
get_dr3 ENDP

get_dr6 PROC PUBLIC
    xor eax, eax
    mov eax, dr6
    ret
get_dr6 ENDP

get_dr7 PROC PUBLIC
    xor eax, eax
    mov eax, dr7
    ret
get_dr7 ENDP

set_cr0 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov cr0, eax
    ret
set_cr0 ENDP

set_cr2 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov cr2, eax
    ret
set_cr2 ENDP

set_cr3 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov cr3, eax
    ret
set_cr3 ENDP

set_cr4 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov cr4, eax
    ret
set_cr4 ENDP

set_dr0 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov dr0, eax
    ret
set_dr0 ENDP

set_dr1 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov dr1, eax
    ret
set_dr1 ENDP

set_dr2 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov dr2, eax
    ret
set_dr2 ENDP

set_dr3 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov dr3, eax
    ret
set_dr3 ENDP

set_dr6 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov dr6, eax
    ret
set_dr6 ENDP

set_dr7 PROC PUBLIC USES EAX x:dword
    xor eax, eax
    mov eax, x
    mov dr7, eax
    ret
set_dr7 ENDP

get_kernel_cs PROC PUBLIC
    xor eax, eax
    mov ax, cs
    ret
get_kernel_cs ENDP

get_kernel_ds PROC PUBLIC
    xor eax, eax
    mov ax, ds
    ret
get_kernel_ds ENDP

get_kernel_es PROC PUBLIC
    xor eax, eax
    mov ax, es
    ret
get_kernel_es ENDP

get_kernel_ss PROC PUBLIC
    xor eax, eax
    mov ax, ss
    ret
get_kernel_ss ENDP

get_kernel_gs PROC PUBLIC
    xor eax, eax
    mov ax, gs
    ret
get_kernel_gs ENDP

get_kernel_fs PROC PUBLIC
    xor eax, eax
    mov ax, fs
    ret
get_kernel_fs ENDP

set_kernel_ds PROC PUBLIC USES EAX x:word
    xor eax, eax
    mov ax, x
    mov ds, ax
    ret
set_kernel_ds ENDP

set_kernel_es PROC PUBLIC USES EAX x:word
    xor eax, eax
    mov ax, x
    mov es, ax
    ret
set_kernel_es ENDP

set_kernel_gs PROC PUBLIC USES EAX x:word
    xor eax, eax
    mov ax, x
    mov gs, ax
    ret
set_kernel_gs ENDP

set_kernel_fs PROC PUBLIC USES EAX x:word
    xor eax, eax
    mov ax, x
    mov fs, ax
    ret
set_kernel_fs ENDP

asm_rdmsr PROC PUBLIC USES EAX EDX ECX x:dword, y:ptr QWORD_STRUCT
    xor eax, eax
    xor edx, edx
    xor ecx, ecx
    mov ecx, x
    rdmsr
    xor ecx, ecx
    mov ecx, y
    mov [ecx].QWORD_STRUCT._low, eax
    mov [ecx].QWORD_STRUCT._high, edx
    ret
asm_rdmsr ENDP

asm_wrmsr PROC PUBLIC USES EAX EDX ECX x:dword, y:ptr QWORD_STRUCT
    xor eax, eax
    xor edx, edx
    xor ecx, ecx
    mov ecx, y
    mov eax, [ecx].QWORD_STRUCT._low
    mov edx, [ecx].QWORD_STRUCT._high
    xor ecx, ecx
    mov ecx, x
    wrmsr
    ret
asm_wrmsr ENDP

asm_rdtsc PROC PUBLIC USES EAX EDX ECX x:ptr QWORD_STRUCT
    xor eax, eax
    xor edx, edx
    xor ecx, ecx
    mov ecx, x
    rdtsc
    mov [ecx].QWORD_STRUCT._low, eax
    mov [ecx].QWORD_STRUCT._high, edx
    ret
asm_rdtsc ENDP

asm_fxinit PROC PUBLIC
    finit
    ret
asm_fxinit ENDP

asm_fxsave PROC PUBLIC USES EAX x:ptr byte
    xor eax, eax
    mov eax, x
    fxsave byte ptr [eax]
    ret
asm_fxsave ENDP

asm_fxrstor PROC PUBLIC USES EAX x:ptr byte
    xor eax, eax
    mov eax, x
    fxrstor byte ptr [eax]
    ret
asm_fxrstor ENDP

; TODO: Does declaring |x| (bit base address) as "ptr byte" limit the range of
; |y| (bit offset)? For safety, never call this routine with |y| >= 8
asm_btr PROC PUBLIC USES EAX x:ptr byte, y:dword
    xor eax, eax
    mov eax, y
    lock btr x, eax
    ret
asm_btr ENDP

asm_bts PROC PUBLIC USES EAX x:ptr byte, y:dword
    xor eax, eax
    mov eax, y
    lock bts x, eax
    ret
asm_bts ENDP

cpu_has_emt64_support PROC public
    push ebx
    push ecx
    push edx
    xor eax, eax
    xor ecx, ecx
    mov eax, 80000001h
    cpuid
    mov eax, edx
    test eax, 20000000h
    pushf
    pop ax
    and eax, 40h
    xor ecx, ecx
    mov cl, 6h
    shr eax, cl
    xor eax, 1h
    pop edx
    pop ecx
    pop ebx
    ret
cpu_has_emt64_support ENDP

cpu_has_vmx_support PROC PUBLIC
    push ebx
    push ecx
    push edx
    xor eax, eax
    xor ecx, ecx
    mov eax, 1h
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
    pop edx
    pop ecx
    pop ebx
    ret
cpu_has_vmx_support ENDP

cpu_has_nx_support PROC PUBLIC
    push ebx
    push ecx
    push edx
    xor eax, eax
    xor ecx, ecx
    mov eax, 80000001h
    cpuid
    mov eax, edx
    test eax, 100000h
    pushf
    pop ax
    and eax, 40h
    xor ecx, ecx
    mov cl, 6h
    shr eax, cl
    xor eax, 1h
    pop edx
    pop ecx
    pop ebx
    ret
cpu_has_nx_support ENDP

get_kernel_rflags PROC PUBLIC
    xor eax, eax
    pushf
    pop ax
    ret
get_kernel_rflags ENDP

__nmi PROC PUBLIC
    int 2h
    ret
__nmi ENDP

__fls PROC PUBLIC USES EBX x:dword
    xor eax, eax
    xor ebx, ebx
    mov ebx, x
    bsr eax, ebx
    ret
__fls ENDP

__handle_cpuid PROC PUBLIC USES EAX EBX EDX ECX ESI x:ptr VCPU_STATE
    xor eax, eax
    xor ebx, ebx
    xor edx, edx
    xor ecx, ecx
    xor esi, esi
    mov esi, x
    mov eax, [esi].VCPU_STATE._eax
    mov ecx, [esi].VCPU_STATE._ecx
    cpuid
    mov [esi].VCPU_STATE._eax, eax
    mov [esi].VCPU_STATE._ebx, ebx
    mov [esi].VCPU_STATE._ecx, ecx
    mov [esi].VCPU_STATE._edx, edx
    ret
__handle_cpuid ENDP

end
