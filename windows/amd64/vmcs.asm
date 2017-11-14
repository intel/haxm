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

INVEPT_DESC STRUCT
    eptp    QWORD   ?
    rsvd    QWORD   0
INVEPT_DESC ENDS

.data
vmx_fail_mask dword 0041h

.code

__vmxon PROC public
    local x:qword
    xor rax, rax
    mov x, rcx
    vmxon x
    pushfq
    pop rax
    and eax, vmx_fail_mask
    ret
__vmxon ENDP

__vmxoff PROC public
    xor rax, rax
    vmxoff
    pushfq
    pop rax
    and eax, vmx_fail_mask
    ret
__vmxoff ENDP

__vmclear PROC public
    local x:qword
    xor rax, rax
    mov x, rcx
    vmclear x
    pushfq
    pop rax
    and eax, vmx_fail_mask
    ret
__vmclear ENDP

__vmptrld PROC public
    local x:qword
    xor rax, rax
    mov x, rcx
    vmptrld x
    pushfq
    pop rax
    and eax, vmx_fail_mask
    ret
__vmptrld ENDP

asm_vmptrst PROC public
    local x:qword
    vmptrst x
    mov rax, x
    mov [rcx], rax
    pushfq
    pop rax
    ret
asm_vmptrst ENDP

__vmread PROC public
    xor rax, rax
    vmread rax, rcx
    ret
__vmread ENDP

__vmwrite PROC public
    vmwrite rcx, rdx
    ret
__vmwrite ENDP

__vmx_run PROC public
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
    ;write host rsp
    xor rbx, rbx
    mov ebx, 6c14h
    mov rax, rsp
    sub rax, 8h
    vmwrite rbx, rax
    pop rbx
    pop rax
    push rax
    push rbx
    ;push the state
    push rcx
    cmp dx, 1h
    mov rax, rcx
    mov rcx, [rax].VCPU_STATE._rcx
    mov rdx, [rax].VCPU_STATE._rdx
    mov rbx, [rax].VCPU_STATE._rbx
    mov rbp, [rax].VCPU_STATE._rbp
    mov rsi, [rax].VCPU_STATE._rsi
    mov rdi, [rax].VCPU_STATE._rdi
    mov r8, [rax].VCPU_STATE._r8
    mov r9, [rax].VCPU_STATE._r9
    mov r10, [rax].VCPU_STATE._r10
    mov r11, [rax].VCPU_STATE._r11
    mov r12, [rax].VCPU_STATE._r12
    mov r13, [rax].VCPU_STATE._r13
    mov r14, [rax].VCPU_STATE._r14
    mov r15, [rax].VCPU_STATE._r15
    mov rax, [rax].VCPU_STATE._rax
    je RESUME
    vmlaunch
    jmp EXIT_ENTRY_FAIL
    RESUME:
    vmresume
    jmp EXIT_ENTRY_FAIL
    EXIT_ENTRY::
    push rdi
    mov rdi, [rsp+8]
    mov [rdi].VCPU_STATE._rax, rax
    mov [rdi].VCPU_STATE._rcx, rcx
    mov [rdi].VCPU_STATE._rdx, rdx
    pop rcx
    mov [rdi].VCPU_STATE._rbx, rbx
    mov [rdi].VCPU_STATE._rbp, rbp
    mov [rdi].VCPU_STATE._rsi, rsi
    mov [rdi].VCPU_STATE._rdi, rcx
    mov [rdi].VCPU_STATE._r8, r8
    mov [rdi].VCPU_STATE._r9, r9
    mov [rdi].VCPU_STATE._r10, r10
    mov [rdi].VCPU_STATE._r11, r11
    mov [rdi].VCPU_STATE._r12, r12
    mov [rdi].VCPU_STATE._r13, r13
    mov [rdi].VCPU_STATE._r14, r14
    mov [rdi].VCPU_STATE._r15, r15
    EXIT_ENTRY_FAIL:
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
    pushfq
    pop rax
    popfq
    ret
__vmx_run ENDP

get_rip PROC public
    xor rax, rax
    ;XXX is it right?
    lea rax, EXIT_ENTRY
    ret
get_rip ENDP

; 1st parameter (RCX): INVEPT type
; 2nd parameter (RDX): HVA of INVEPT descriptor
__invept PROC public
    xor rax, rax
    invept rcx, OWORD PTR [rdx]
    pushfq
    pop rax
    and eax, vmx_fail_mask
    ret
__invept ENDP

end
