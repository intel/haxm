/*
 * Copyright (c) 2004-2010 Intel Corporation
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

#ifndef HAX_VCPU_STATE_H_
#define HAX_VCPU_STATE_H_

union interruptibility_state_t {
    uint32 raw;
    struct {
        uint32 sti_blocking   : 1;
        uint32 movss_blocking : 1;
        uint32 smi_blocking   : 1;
        uint32 nmi_blocking   : 1;
        uint32 reserved       : 28;
    };
    uint64_t pad;
} PACKED;

typedef union interruptibility_state_t interruptibility_state_t;

// Segment descriptor
struct segment_desc_t {
    uint16 selector;
    uint16 _dummy;
    uint32 limit;
    uint64 base;
    union {
        struct {
            uint32 type             : 4;
            uint32 desc             : 1;
            uint32 dpl              : 2;
            uint32 present          : 1;
            uint32                  : 4;
            uint32 available        : 1;
            uint32 long_mode        : 1;
            uint32 operand_size     : 1;
            uint32 granularity      : 1;
            uint32 null             : 1;
            uint32                  : 15;
        };
        uint32 ar;
    };
    uint32 ipad;
} PACKED;

typedef struct segment_desc_t segment_desc_t;

struct vcpu_state_t {
    union {
        uint64 _regs[16];
        struct {
            union {
                struct {
                    uint8 _al,
                          _ah;
                };
                uint16    _ax;
                uint32    _eax;
                uint64    _rax;
            };
            union {
                struct {
                    uint8 _cl,
                          _ch;
                };
                uint16    _cx;
                uint32    _ecx;
                uint64    _rcx;
            };
            union {
                struct {
                    uint8 _dl,
                          _dh;
                };
                uint16    _dx;
                uint32    _edx;
                uint64    _rdx;
            };
            union {
                struct {
                    uint8 _bl,
                          _bh;
                };
                uint16    _bx;
                uint32    _ebx;
                uint64    _rbx;
            };
            union {
                uint16    _sp;
                uint32    _esp;
                uint64    _rsp;
            };
            union {
                uint16    _bp;
                uint32    _ebp;
                uint64    _rbp;
            };
            union {
                uint16    _si;
                uint32    _esi;
                uint64    _rsi;
            };
            union {
                uint16    _di;
                uint32    _edi;
                uint64    _rdi;
            };

            uint64 _r8;
            uint64 _r9;
            uint64 _r10;
            uint64 _r11;
            uint64 _r12;
            uint64 _r13;
            uint64 _r14;
            uint64 _r15;
        };
    };

    union {
        uint32 _eip;
        uint64 _rip;
    };

    union {
        uint32 _eflags;
        uint64 _rflags;
    };

    segment_desc_t _cs;
    segment_desc_t _ss;
    segment_desc_t _ds;
    segment_desc_t _es;
    segment_desc_t _fs;
    segment_desc_t _gs;
    segment_desc_t _ldt;
    segment_desc_t _tr;

    segment_desc_t _gdt;
    segment_desc_t _idt;

    uint64 _cr0;
    uint64 _cr2;
    uint64 _cr3;
    uint64 _cr4;

    uint64 _dr0;
    uint64 _dr1;
    uint64 _dr2;
    uint64 _dr3;
    uint64 _dr6;
    uint64 _dr7;
    uint64 _pde;

    uint32 _efer;

    uint32 _sysenter_cs;
    uint64 _sysenter_eip;
    uint64 _sysenter_esp;

    uint32 _activity_state;
    uint32 pad;
    interruptibility_state_t _interruptibility_state;
} PACKED;

void dump(void);

#endif  // HAX_VCPU_STATE_H_
