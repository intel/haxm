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
    uint32_t raw;
    struct {
        uint32_t sti_blocking   : 1;
        uint32_t movss_blocking : 1;
        uint32_t smi_blocking   : 1;
        uint32_t nmi_blocking   : 1;
        uint32_t reserved       : 28;
    };
    uint64_t pad;
} PACKED;

typedef union interruptibility_state_t interruptibility_state_t;

// Segment descriptor
struct segment_desc_t {
    uint16_t selector;
    uint16_t _dummy;
    uint32_t limit;
    uint64_t base;
    union {
        struct {
            uint32_t type             : 4;
            uint32_t desc             : 1;
            uint32_t dpl              : 2;
            uint32_t present          : 1;
            uint32_t                  : 4;
            uint32_t available        : 1;
            uint32_t long_mode        : 1;
            uint32_t operand_size     : 1;
            uint32_t granularity      : 1;
            uint32_t null             : 1;
            uint32_t                  : 15;
        };
        uint32_t ar;
    };
    uint32_t ipad;
} PACKED;

typedef struct segment_desc_t segment_desc_t;

struct vcpu_state_t {
    union {
        uint64_t _regs[16];
        struct {
            union {
                struct {
                    uint8_t _al,
                            _ah;
                };
                uint16_t    _ax;
                uint32_t    _eax;
                uint64_t    _rax;
            };
            union {
                struct {
                    uint8_t _cl,
                            _ch;
                };
                uint16_t    _cx;
                uint32_t    _ecx;
                uint64_t    _rcx;
            };
            union {
                struct {
                    uint8_t _dl,
                            _dh;
                };
                uint16_t    _dx;
                uint32_t    _edx;
                uint64_t    _rdx;
            };
            union {
                struct {
                    uint8_t _bl,
                            _bh;
                };
                uint16_t    _bx;
                uint32_t    _ebx;
                uint64_t    _rbx;
            };
            union {
                uint16_t    _sp;
                uint32_t    _esp;
                uint64_t    _rsp;
            };
            union {
                uint16_t    _bp;
                uint32_t    _ebp;
                uint64_t    _rbp;
            };
            union {
                uint16_t    _si;
                uint32_t    _esi;
                uint64_t    _rsi;
            };
            union {
                uint16_t    _di;
                uint32_t    _edi;
                uint64_t    _rdi;
            };

            uint64_t _r8;
            uint64_t _r9;
            uint64_t _r10;
            uint64_t _r11;
            uint64_t _r12;
            uint64_t _r13;
            uint64_t _r14;
            uint64_t _r15;
        };
    };

    union {
        uint32_t _eip;
        uint64_t _rip;
    };

    union {
        uint32_t _eflags;
        uint64_t _rflags;
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

    uint64_t _cr0;
    uint64_t _cr2;
    uint64_t _cr3;
    uint64_t _cr4;

    uint64_t _dr0;
    uint64_t _dr1;
    uint64_t _dr2;
    uint64_t _dr3;
    uint64_t _dr6;
    uint64_t _dr7;
    uint64_t _pde;

    uint32_t _efer;

    uint32_t _sysenter_cs;
    uint64_t _sysenter_eip;
    uint64_t _sysenter_esp;

    uint32_t _activity_state;
    uint32_t pad;
    interruptibility_state_t _interruptibility_state;
} PACKED;

void dump(void);

#endif  // HAX_VCPU_STATE_H_
