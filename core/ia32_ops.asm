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

%macro function_get_reg 1
    function get_%+%1, 0
    mov reg_ret, %1
    ret
%endmacro
%macro function_set_reg 1
    function set_%+%1, 1
    mov %1, reg_arg1
    ret
%endmacro
%macro function_get_segment 1
    function get_kernel_%+%1, 0
    mov reg_ret_16, %1
    ret
%endmacro
%macro function_set_segment 1
    function set_kernel_%+%1, 1
    mov %1, reg_arg1_16
    ret
%endmacro

section .text

struc qword_struct
    .lo      resd 1
    .hi      resd 1
endstruc

struc cpuid_args
    ._eax    resd 1
    ._ecx    resd 1
    ._edx    resd 1
    ._ebx    resd 1
endstruc

function __nmi, 0
    int 2h
    ret

function asm_fls, 1
    xor reg_ret_32, reg_ret_32
    bsr reg_ret_32, reg_arg1_32
    ret

function asm_cpuid, 1
%ifidn __BITS__, 64
    push rbx
    mov r8, reg_arg1
    mov eax, [r8 + cpuid_args._eax]
    mov ecx, [r8 + cpuid_args._ecx]
    cpuid
    mov [r8 + cpuid_args._eax], eax
    mov [r8 + cpuid_args._ebx], ebx
    mov [r8 + cpuid_args._ecx], ecx
    mov [r8 + cpuid_args._edx], edx
    pop rbx
    ret
%elifidn __BITS__, 32
    push ebx
    push esi
    mov esi, reg_arg1
    mov eax, [esi + cpuid_args._eax]
    mov ecx, [esi + cpuid_args._ecx]
    cpuid
    mov [esi + cpuid_args._eax], eax
    mov [esi + cpuid_args._ebx], ebx
    mov [esi + cpuid_args._ecx], ecx
    mov [esi + cpuid_args._edx], edx
    pop esi
    pop ebx
    ret
%else
    %error "Unimplemented function"
%endif

function asm_btr, 2
    lock btr [reg_arg1], reg_arg2
    ret

function asm_bts, 2
    lock bts [reg_arg1], reg_arg2
    ret

function asm_disable_irq, 0
    cli
    ret

function asm_enable_irq, 0
    sti
    ret

function asm_clts, 0
    clts
    ret

function asm_fxinit, 0
    finit
    ret

function asm_fxrstor, 1
    fxrstor [reg_arg1]
    ret

function asm_fxsave, 1
    fxsave [reg_arg1]
    ret

function asm_rdmsr, 2
%ifidn __BITS__, 64
    mov rcx, reg_arg1
    rdmsr
    shl rdx, 32
    or reg_ret, rdx
    ret
%elifidn __CONV__, x32_cdecl
    push ebx
    mov ebx, reg_arg2
    rdmsr
    mov [ebx + qword_struct.lo], eax
    mov [ebx + qword_struct.hi], edx
    pop ebx
    ret
%else
    %error "Unimplemented function"
%endif

function asm_rdtsc, 1
%ifidn __BITS__, 64
    rdtsc
    shl rdx, 32
    or reg_ret, rdx
    ret
%elifidn __BITS__, 32
    rdtsc
    mov [reg_arg1 + qword_struct.lo], eax
    mov [reg_arg1 + qword_struct.hi], edx
    ret
%else
    %error "Unimplemented function"
%endif

function asm_wrmsr, 2
%ifidn __BITS__, 64
    push rbx
    mov rbx, reg_arg2
    mov rcx, reg_arg1
    mov eax, ebx
    mov rdx, rbx
    shr rdx, 32
    wrmsr
    pop rbx
    ret
%elifidn __CONV__, x32_cdecl
    push edi
    push esi
    mov edi, [reg_arg2 + qword_struct.lo]
    mov esi, [reg_arg2 + qword_struct.hi]
    mov eax, edi
    mov edx, esi
    wrmsr
    pop esi
    pop edi
    ret
%else
    %error "Unimplemented function"
%endif

function get_kernel_tr_selector, 0
    str reg_ret_16
    ret

function get_kernel_ldt, 0
    sldt reg_ret_16
    ret

function get_kernel_gdt, 1
    sgdt [reg_arg1]
    ret

function get_kernel_idt, 1
    sidt [reg_arg1]
    ret

function get_kernel_rflags, 0
    pushfw
    pop reg_ret_16
    ret

function set_kernel_ldt, 1
    lldt reg_arg1_16
    ret

function set_kernel_gdt, 1
    lgdt [reg_arg1]
    ret

function set_kernel_idt, 1
    lidt [reg_arg1]
    ret

function_get_reg cr0
function_get_reg cr2
function_get_reg cr3
function_get_reg cr4
function_get_reg dr0
function_get_reg dr1
function_get_reg dr2
function_get_reg dr3
function_get_reg dr6
function_get_reg dr7

function_set_reg cr0
function_set_reg cr2
function_set_reg cr3
function_set_reg cr4
function_set_reg dr0
function_set_reg dr1
function_set_reg dr2
function_set_reg dr3
function_set_reg dr6
function_set_reg dr7

function_get_segment cs
function_get_segment ds
function_get_segment es
function_get_segment ss
function_get_segment gs
function_get_segment fs

function_set_segment cs
function_set_segment ds
function_set_segment es
function_set_segment ss
function_set_segment gs
function_set_segment fs
