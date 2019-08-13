;
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
; Calling convention
;  - eax/rax - dest value
;  - edx/rdx - src1 value
;  - ecx/rcx - src2 value
;
%define reg_dst_08   al
%define reg_dst_16   ax
%define reg_dst_32   eax
%define reg_dst_64   rax

%define reg_src1_08  cl
%define reg_src1_16  cx
%define reg_src1_32  ecx
%define reg_src1_64  rcx

%define reg_src2_08  dl
%define reg_src2_16  dx
%define reg_src2_32  edx
%define reg_src2_64  rdx

%ifidn __BITS__, 32
    %define reg_dst   reg_dst_32
    %define reg_src1  reg_src1_32
    %define reg_src2  reg_src2_32
%elifidn __BITS__, 64
    %define reg_dst   reg_dst_64
    %define reg_src1  reg_src1_64
    %define reg_src2  reg_src2_64
%endif

%macro fop_start 1
    align 16
    global em_%+%1
    em_%+%1:
%endmacro

; Instruction variants
%macro fop 1-*
    align 16
     %{1:-1}
    ret
%endmacro
%macro fop32 1-*
    fop %{1:-1}
%endmacro
%macro fop64 1-*
    %ifidn __BITS__, 64
        fop %{1:-1}
    %endif
%endmacro

; 1-operand instructions
%macro fastop1 1
    fop_start %1
    fop32 %[%1 reg_dst_08]
    fop32 %[%1 reg_dst_16]
    fop32 %[%1 reg_dst_32]
    fop64 %[%1 reg_dst_64]
%endmacro

; 2-operand instructions
%macro fastop2 1
    fop_start %1
    fop32 %[%1 reg_dst_08, reg_src1_08]
    fop32 %[%1 reg_dst_16, reg_src1_16]
    fop32 %[%1 reg_dst_32, reg_src1_32]
    fop64 %[%1 reg_dst_64, reg_src1_64]
%endmacro
%macro fastop2w 1
    fop_start %1
    fop32 %[nop]
    fop32 %[%1 reg_dst_16, reg_src1_16]
    fop32 %[%1 reg_dst_32, reg_src1_32]
    fop64 %[%1 reg_dst_64, reg_src1_64]
%endmacro
%macro fastop2cl 1
    fop_start %1
    fop32 %[%1 reg_dst_08, cl]
    fop32 %[%1 reg_dst_16, cl]
    fop32 %[%1 reg_dst_32, cl]
    fop64 %[%1 reg_dst_64, cl]
%endmacro

; 3-operand instructions
%macro fastop3d 1
    fop_start %1
    fop32 %[nop]
    fop32 %[nop]
    fop32 %[%1 reg_dst_32, reg_src1_32, reg_src2_32]
    fop64 %[%1 reg_dst_64, reg_src1_64, reg_src2_64]
%endmacro
%macro fastop3wcl 1
    fop_start %1
    fop32 %[nop]
    fop32 %[%1 reg_dst_16, reg_src1_16, cl]
    fop32 %[%1 reg_dst_32, reg_src1_32, cl]
    fop64 %[%1 reg_dst_64, reg_src1_64, cl]
%endmacro


section .text

; Instruction handling
fastop1 not
fastop1 neg
fastop1 inc
fastop1 dec

fastop2 add
fastop2 or
fastop2 adc
fastop2 sbb
fastop2 and
fastop2 sub
fastop2 xor
fastop2 test
fastop2 xadd
fastop2 cmp
fastop2w bsf
fastop2w bsr
fastop2w bt
fastop2w bts
fastop2w btr
fastop2w btc
fastop2cl rol
fastop2cl ror
fastop2cl rcl
fastop2cl rcr
fastop2cl shl
fastop2cl shr
fastop2cl sar

fastop3wcl shld
fastop3wcl shrd
fastop3d bextr
fastop3d andn

; Instruction dispatching
global fastop_dispatch
%ifidn __CONV__, x32_cdecl
    ; Arguments:
    ;   handler  - stack0  (accessed directly)
    ;   dst      - stack1  (saved to non-volatile ebx)
    ;   src1     - stack2  (saved to non-volatile esi)
    ;   src2     - stack3  (saved to non-volatile edi)
    ;   flags    - stack4  (saved to non-volatile ebp)
%define stack_arg(index) [esp + 5*04h + 04h + (index*04h)]
fastop_dispatch:
    push ebx
    push esi
    push edi
    push ebp
    pushf
    mov ebx, stack_arg(1)
    mov esi, stack_arg(2)
    mov edi, stack_arg(3)
    mov ebp, stack_arg(4)
    mov reg_dst, [ebx]
    mov reg_src1, [esi]
    mov reg_src2, [edi]
    push dword [ebp]
    popf
    call stack_arg(0)
    pushf
    pop dword [ebp]
    mov [ebx], reg_dst
    popf
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
%undef stack_arg

%elifidn __CONV__, x64_systemv
    ; Arguments:
    ;   handler  - rdi    (accessed directly)
    ;   dst      - rsi    (accessed directly)
    ;   src1     - rdx    (saved to volatile r10)
    ;   src2     - rcx    (saved to volatile r11)
    ;   flags    - r8     (accessed directly)
%define stack_arg(index) [rsp + 1*08h + 08h + (index*08h)]
fastop_dispatch:
    pushf
    mov r10, rdx
    mov r11, rcx
    mov reg_dst, [rsi]
    mov reg_src1, [r10]
    mov reg_src2, [r11]
    push qword [r8]
    popf
    call rdi
    pushf
    pop qword [r8]
    mov [rsi], reg_dst
    popf
    ret
%undef stack_arg

%elifidn __CONV__, x64_microsoft
    ; Arguments:
    ;   handler  - rcx     (saved to volatile r10)
    ;   dst      - rdx     (saved to volatile r11)
    ;   src1     - r8      (accessed directly)
    ;   src2     - r9      (accessed directly)
    ;   flags    - stack4  (saved to non-volatile r12)
%define stack_arg(index) [rsp + 2*08h + 08h + (index*08h)]
fastop_dispatch:
    push r12
    pushf
    mov r10, rcx
    mov r11, rdx
    mov r12, stack_arg(4)
    mov reg_dst, [r11]
    mov reg_src1, [r8]
    mov reg_src2, [r9]
    push qword [r12]
    popf
    call r10
    pushf
    pop qword [r12]
    mov [r11], reg_dst
    popf
    pop r12
    ret
%undef stack_arg

%endif
