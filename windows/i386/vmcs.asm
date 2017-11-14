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

VCPU_STATE_32 STRUCT
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
VCPU_STATE_32 ENDS

INVEPT_DESC_32 STRUCT
    eptp    DWORD   ?
    pad1    DWORD   0
    rsvd    DWORD   0
    pad2    DWORD   0
INVEPT_DESC_32 ENDS

.data
vmx_fail_mask word 41h
;

.code
start:

__vmxon PROC public x:qword
    xor eax, eax
    vmxon x
    pushf
    pop ax
    and ax, vmx_fail_mask
    ret
__vmxon ENDP

__vmxoff PROC public
    xor eax, eax
    vmxoff
    pushf
    pop ax
    and ax, vmx_fail_mask
    ret
__vmxoff ENDP

__vmclear PROC public x:qword
    xor eax, eax
    vmclear x
    pushf
    pop ax
    and ax, vmx_fail_mask
    ret
__vmclear ENDP

__vmptrld PROC public x:qword
    xor eax, eax
    vmptrld x
    pushf
    pop ax
    and ax, vmx_fail_mask
    ret
__vmptrld ENDP

asm_vmptrst PROC public USES EAX x:ptr qword
    xor eax, eax
    mov eax, x
    vmptrst qword ptr [eax]
    pushf
    pop ax
    ret
asm_vmptrst ENDP

ia32_asm_vmread PROC public USES EBX x:dword
    xor eax, eax
    xor ebx, ebx
    mov ebx, x
    vmread eax, ebx
    ret
ia32_asm_vmread ENDP

ia32_asm_vmwrite PROC public USES EAX EBX x:dword, y:dword
    xor eax, eax
    xor ebx, ebx
    mov eax, x
    mov ebx, y
    vmwrite eax, ebx
    ret
ia32_asm_vmwrite ENDP

__vmx_run PROC public x:ptr VCPU_STATE_32, y:word
    pushfd
    push ecx
    push edx
    push esi
    push edi
    push ebp
    push eax
    push ebx
    ; write host rsp
    mov ebx, 6c14h
    mov eax, esp
    sub eax, 4h
    vmwrite ebx, eax
    pop ebx
    pop eax
    push eax
    push ebx
    ; push the state
    mov eax, x
    mov dx, y
    push eax
    cmp dx, 1h
    mov ecx, [eax].VCPU_STATE_32._ecx
    mov edx, [eax].VCPU_STATE_32._edx
    mov ebx, [eax].VCPU_STATE_32._ebx
    mov ebp, [eax].VCPU_STATE_32._ebp
    mov esi, [eax].VCPU_STATE_32._esi
    mov edi, [eax].VCPU_STATE_32._edi
    mov eax, [eax].VCPU_STATE_32._eax
    je RESUME
    vmlaunch
    jmp EXIT_ENTRY_FAIL
    RESUME:
    vmresume
    jmp EXIT_ENTRY_FAIL
    EXIT_ENTRY::
    push edi
    mov edi, [esp+4]
    mov [edi].VCPU_STATE_32._eax, eax
    mov [edi].VCPU_STATE_32._ecx, ecx
    mov [edi].VCPU_STATE_32._edx, edx
    pop ecx
    mov [edi].VCPU_STATE_32._ebx, ebx
    mov [edi].VCPU_STATE_32._ebp, ebp
    mov [edi].VCPU_STATE_32._esi, esi
    mov [edi].VCPU_STATE_32._edi, ecx
    EXIT_ENTRY_FAIL:
    ; pop the state
    pop eax
    pop ebx
    pop eax
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pushfd
    pop eax
    popfd
    ret
__vmx_run ENDP

get_rip PROC public
    xor eax, eax
    lea eax, EXIT_ENTRY
    ret
get_rip ENDP

; Unimplemented
__invept PROC PUBLIC x:dword, y:ptr INVEPT_DESC_32
    ; Just return an error
    or ax, vmx_fail_mask
    ret
__invept ENDP

end
