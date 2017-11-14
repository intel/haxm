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

SYSDESC_T STRUCT
    __limit WORD    ?
    __base  DWORD   ?
SYSDESC_T ENDS

.data
;

.code
start:
load_kernel_ldt PROC public USES EAX x:word
    xor eax, eax
    mov ax, x
    lldt ax
    ret
load_kernel_ldt ENDP

get_kernel_tr_selector PROC public
    xor eax, eax
    str ax
    ret
get_kernel_tr_selector ENDP

get_kernel_ldt PROC public
    xor eax, eax
    sldt ax
    ret
get_kernel_ldt ENDP

get_kernel_gdt PROC public USES ECX x:dword
    xor ecx, ecx
    mov ecx, x
    sgdt [ecx]
    ret
get_kernel_gdt ENDP

get_kernel_idt PROC public USES ECX x:dword
    xor ecx, ecx
    mov ecx, x
    sidt [ecx]
    ret
get_kernel_idt ENDP

set_kernel_gdt PROC public USES ECX x:dword
    xor ecx, ecx
    mov ecx, x
    lgdt fword ptr [ecx]
    ret
set_kernel_gdt ENDP

set_kernel_idt PROC public USES ECX x:dword
    xor ecx, ecx
    mov ecx, x
    lidt fword ptr [ecx]
    ret
set_kernel_idt ENDP

end
