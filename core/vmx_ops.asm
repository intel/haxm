;
; Copyright (c) 2011 Intel Corporation
; Copyright (c) 2018 Alexandro Sanchez Bach <alexandro@phi.nz>
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
; Detect architecture
;
%ifidn __OUTPUT_FORMAT__, elf32
    %define __BITS__ 32
    %define __CONV__ x32_cdecl
%elifidn __OUTPUT_FORMAT__, win32
    %define __BITS__ 32
    %define __CONV__ x32_cdecl
%elifidn __OUTPUT_FORMAT__, macho32
    %define __BITS__ 32
    %define __CONV__ x32_cdecl
%elifidn __OUTPUT_FORMAT__, elf64
    %define __BITS__ 64
    %define __CONV__ x64_systemv
%elifidn __OUTPUT_FORMAT__, win64
    %define __BITS__ 64
    %define __CONV__ x64_microsoft
%elifidn __OUTPUT_FORMAT__, macho64
    %define __BITS__ 64
    %define __CONV__ x64_systemv
%endif

;
; Describe calling convention
;
%ifidn __CONV__, x32_cdecl
;
; Although cdecl does not place arguments in registers, we simulate fastcall
; by reading the first 2 stack arguments into the ecx/edx respectively.
;
    %define reg_arg1_16  cx
    %define reg_arg1_32  ecx
    %define reg_arg1     reg_arg1_32
    %define reg_arg2_16  dx
    %define reg_arg2_32  edx
    %define reg_arg2     reg_arg2_32
    %define reg_ret_16   ax
    %define reg_ret_32   eax
    %define reg_ret      reg_ret_32
%elifidn __CONV__, x64_systemv
    %define reg_arg1_16  di
    %define reg_arg1_32  edi
    %define reg_arg1_64  rdi
    %define reg_arg1     reg_arg1_64
    %define reg_arg2_16  si
    %define reg_arg2_32  esi
    %define reg_arg2_64  rsi
    %define reg_arg2     reg_arg2_64
    %define reg_ret_16   ax
    %define reg_ret_32   eax
    %define reg_ret_64   rax
    %define reg_ret      reg_ret_64
%elifidn __CONV__, x64_microsoft
    %define reg_arg1_16  cx
    %define reg_arg1_32  ecx
    %define reg_arg1_64  rcx
    %define reg_arg1     reg_arg1_64
    %define reg_arg2_16  dx
    %define reg_arg2_32  edx
    %define reg_arg2_64  rdx
    %define reg_arg2     reg_arg2_64
    %define reg_ret_16   ax
    %define reg_ret_32   eax
    %define reg_ret_64   rax
    %define reg_ret      reg_ret_64
%endif

;
; Helpers
;

; Macro: function
; Declares a function. Arguments:
; - %1  Name of the function
; - %2  Number of arguments
;
%macro function 2
    global %1
    %1:
%ifidn __CONV__, x32_cdecl
    %if %2 >= 3
        %error "Unsupported number of arguments"
    %else
        %if %2 >= 1
            mov reg_arg1, [esp + 0x4]
        %endif
        %if %2 >= 2
            mov reg_arg2, [esp + 0x8]
        %endif
    %endif
%endif
%endmacro

section .text

struc qword_struct
    .lo      resd 1
    .hi      resd 1
endstruc

struc vcpu_state
    ._rax    resq 1
    ._rcx    resq 1
    ._rdx    resq 1
    ._rbx    resq 1
    ._rsp    resq 1
    ._rbp    resq 1
    ._rsi    resq 1
    ._rdi    resq 1
    ._r8     resq 1
    ._r9     resq 1
    ._r10    resq 1
    ._r11    resq 1
    ._r12    resq 1
    ._r13    resq 1
    ._r14    resq 1
    ._r15    resq 1
endstruc

%macro vmx_check 0
    pushfw
    pop ax
    test ax, 0x40
    jnz %%fail_valid
    test ax, 0x01
    jnz %%fail_invalid
    mov reg_ret, 0
    jmp %%continue
%%fail_invalid:
    mov reg_ret, 1
    jmp %%continue
%%fail_valid:
    mov reg_ret, 2
%%continue:
%endmacro

function asm_invept, 2
    invept reg_arg1, [reg_arg2]
    vmx_check
    ret

function asm_vmxon, 1
    vmxon [reg_arg1]
    vmx_check
    ret

function asm_vmxoff, 0
    vmxoff
    vmx_check
    ret

function asm_vmclear, 1
    vmclear [reg_arg1]
    vmx_check
    ret

function asm_vmptrld, 1
    vmptrld [reg_arg1]
    vmx_check
    ret

function asm_vmptrst, 1
    vmptrst [reg_arg1]
    vmx_check
    ret

function asm_vmread, 1
    xor reg_ret, reg_ret
    vmread reg_ret, reg_arg1
    ret

function asm_vmwrite, 2
    vmwrite reg_arg1, reg_arg2
    ret

function asm_vmxrun, 2
%ifidn __BITS__, 64
    pushfq
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push rax
    push rbx
    ; write host rsp
    mov ebx, 6C14h
    mov rax, rsp
    sub rax, 8h
    vmwrite rbx, rax
    pop rbx
    pop rax
    push rax
    push rbx
    ; push the state
    push reg_arg1
    cmp reg_arg2_16, 1h
    mov rax, reg_arg1
    mov rcx, [rax + vcpu_state._rcx]
    mov rdx, [rax + vcpu_state._rdx]
    mov rbx, [rax + vcpu_state._rbx]
    mov rbp, [rax + vcpu_state._rbp]
    mov rsi, [rax + vcpu_state._rsi]
    mov rdi, [rax + vcpu_state._rdi]
    mov r8,  [rax + vcpu_state._r8]
    mov r9,  [rax + vcpu_state._r9]
    mov r10, [rax + vcpu_state._r10]
    mov r11, [rax + vcpu_state._r11]
    mov r12, [rax + vcpu_state._r12]
    mov r13, [rax + vcpu_state._r13]
    mov r14, [rax + vcpu_state._r14]
    mov r15, [rax + vcpu_state._r15]
    mov rax, [rax + vcpu_state._rax]
    je .resume
    vmlaunch
    jmp .exit_entry_fail
.resume:
    vmresume
    jmp .exit_entry_fail
.exit_entry:
    push rdi
    mov rdi, [rsp+8]
    mov [rdi + vcpu_state._rax], rax
    mov [rdi + vcpu_state._rcx], rcx
    mov [rdi + vcpu_state._rdx], rdx
    pop rcx
    mov [rdi + vcpu_state._rbx], rbx
    mov [rdi + vcpu_state._rbp], rbp
    mov [rdi + vcpu_state._rsi], rsi
    mov [rdi + vcpu_state._rdi], rcx
    mov [rdi + vcpu_state._r8], r8
    mov [rdi + vcpu_state._r9], r9
    mov [rdi + vcpu_state._r10], r10
    mov [rdi + vcpu_state._r11], r11
    mov [rdi + vcpu_state._r12], r12
    mov [rdi + vcpu_state._r13], r13
    mov [rdi + vcpu_state._r14], r14
    mov [rdi + vcpu_state._r15], r15
.exit_entry_fail:
    ; pop the state
    pop rbx
    pop rbx
    pop rax
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    vmx_check
    popfq
    ret
%elifidn __BITS__, 32
    pushfd
    push ecx
    push edx
    push esi
    push edi
    push ebp
    push eax
    push ebx
    ; write host rsp
    mov ebx, 6C14h
    mov eax, esp
    sub eax, 4h
    vmwrite ebx, eax
    pop ebx
    pop eax
    push eax
    push ebx
    ; push the state
    mov eax, reg_arg1
    push eax
    cmp reg_arg2_16, 1h
    mov ecx, [eax + vcpu_state._rcx]
    mov edx, [eax + vcpu_state._rdx]
    mov ebx, [eax + vcpu_state._rbx]
    mov ebp, [eax + vcpu_state._rbp]
    mov esi, [eax + vcpu_state._rsi]
    mov edi, [eax + vcpu_state._rdi]
    mov eax, [eax + vcpu_state._rax]
    je .resume
    vmlaunch
    jmp .exit_entry_fail
.resume:
    vmresume
    jmp .exit_entry_fail
.exit_entry:
    push edi
    mov edi, [esp+4]
    mov [edi + vcpu_state._rax], eax
    mov [edi + vcpu_state._rcx], ecx
    mov [edi + vcpu_state._rdx], edx
    pop ecx
    mov [edi + vcpu_state._rbx], ebx
    mov [edi + vcpu_state._rbp], ebp
    mov [edi + vcpu_state._rsi], esi
    mov [edi + vcpu_state._rdi], ecx
.exit_entry_fail:
    ; pop the state
    pop eax
    pop ebx
    pop eax
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    vmx_check
    popfd
    ret
%else
    %error "Unimplemented function"
%endif

function vmx_get_rip, 0
%ifidn __BITS__, 64
    ; Notes:
    ; 1. For some reason, on macOS (macho64), MOV will cause the wrong address
    ;    (VM exit host RIP) to be loaded into reg_ret.
    ; 2. macho64 requires the REL specifier to be used in conjunction with LEA
    ;    to derive the host RIP using RIP-relative addressing.
    lea reg_ret, [rel asm_vmxrun.exit_entry]
%elifidn __BITS__, 32
    mov reg_ret, asm_vmxrun.exit_entry
%else
    %error "Unimplemented function"
%endif
    ret
