/*
 * Copyright (c) 2018 Alexandro Sanchez Bach <alexandro@phi.nz>
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

#ifndef HAX_CORE_EMULATE_H_
#define HAX_CORE_EMULATE_H_

#include "../../include/hax_types.h"

#include "emulate_ops.h"

typedef enum {
    EM_CONTINUE      =  0,  /* Emulation completed successfully */
    EM_EXIT_MMIO     =  1,  /* Emulation requires external MMIO handling */
    EM_ERROR         = -1,  /* Emulation failed */
} em_status_t;

typedef enum {
    EM_MODE_REAL,    /* Real mode */
    EM_MODE_PROT16,  /* Protected mode (16-bit) */
    EM_MODE_PROT32,  /* Protected mode (32-bit) */
    EM_MODE_PROT64,  /* Protected mode (64-bit) */
} em_mode_t;

typedef enum {
    OP_NONE,
    OP_REG,          /* Register operand */
    OP_MEM,          /* Memory operand */
    OP_IMM,          /* Immediate operand */
    OP_ACC,          /* Accumulator: AL, AX, EAX, RAX */
} em_operand_type_t;

struct em_context_t;
struct em_operand_t;

/* Interface */
#define REG_RAX   0
#define REG_RCX   1
#define REG_RDX   2
#define REG_RBX   3
#define REG_RSP   4
#define REG_RBP   5
#define REG_RSI   6
#define REG_RDI   7
#define REG_R8    8
#define REG_R9    9
#define REG_R10   10
#define REG_R11   11
#define REG_R12   12
#define REG_R13   13
#define REG_R14   14
#define REG_R15   15

#define SEG_NONE  0
#define SEG_CS    1
#define SEG_SS    2
#define SEG_DS    3
#define SEG_ES    4
#define SEG_FS    5
#define SEG_GS    6

#define RFLAGS_CF  (1 <<  0)
#define RFLAGS_PF  (1 <<  2)
#define RFLAGS_AF  (1 <<  4)
#define RFLAGS_ZF  (1 <<  6)
#define RFLAGS_SF  (1 <<  7)
#define RFLAGS_DF  (1 << 10)
#define RFLAGS_OF  (1 << 11)

#define RFLAGS_MASK_OSZAPC \
    (RFLAGS_CF | RFLAGS_PF | RFLAGS_AF | RFLAGS_ZF | RFLAGS_SF | RFLAGS_OF)

/* Emulator interface flags */
#define EM_OPS_NO_TRANSLATION  (1 << 0)

// Instructions are never longer than 15 bytes:
//   http://wiki.osdev.org/X86-64_Instruction_Encoding
#define INSTR_MAX_LEN          15

typedef struct em_vcpu_ops_t {
    uint64_t (*read_gpr)(void *vcpu, uint32_t reg_index);
    void (*write_gpr)(void *vcpu, uint32_t reg_index, uint64_t value);
    uint64_t (*read_rflags)(void *vcpu);
    void (*write_rflags)(void *vcpu, uint64_t value);
    uint64_t (*get_segment_base)(void *vcpu, uint32_t segment);
    void (*advance_rip)(void *vcpu, uint64_t len);
    em_status_t (*read_memory)(void *vcpu, uint64_t ea, uint64_t *value,
                               uint32_t size, uint32_t flags);
    em_status_t (*read_memory_post)(void *vcpu,
                               uint64_t *value, uint32_t size);
    em_status_t (*write_memory)(void *vcpu, uint64_t ea, uint64_t *value,
                                uint32_t size, uint32_t flags);
} em_vcpu_ops_t;

typedef em_status_t (em_operand_decoder_t)(struct em_context_t *ctxt,
                                           struct em_operand_t *op);

typedef struct em_opcode_t {
    union {
        void *handler;
        const struct em_opcode_t *group;
    };
    em_operand_decoder_t *decode_dst;
    em_operand_decoder_t *decode_src1;
    em_operand_decoder_t *decode_src2;
    uint64_t flags;
} em_opcode_t;

/* Context */
typedef struct em_operand_t {
    uint32_t size;
    uint32_t flags;
    em_operand_type_t type;
    union {
        struct operand_mem_t {
            uint64_t ea;
            uint32_t seg;
        } mem;
        struct operand_reg_t {
            uint32_t index;
            uint32_t shift;
        } reg;
    };
    uint64_t value;
} em_operand_t;

typedef struct em_context_t {
    void *vcpu;
    const struct em_vcpu_ops_t *ops;
    bool finished;

    em_mode_t mode;
    const uint8_t *insn;
    uint32_t operand_size;
    uint32_t address_size;
    uint32_t len;
    uint64_t rip;

    int rep;
    int lock;
    int override_segment;
    int override_operand_size;
    int override_address_size;

    struct em_opcode_t opcode;
    struct em_operand_t dst;
    struct em_operand_t src1;
    struct em_operand_t src2;
    uint64_t rflags;

    /* Cache */
    uint64_t gpr_cache[16];
    uint16_t gpr_cache_r;
    uint16_t gpr_cache_w;

    /* Decoder */
    uint8_t b;
    struct {
        uint8_t prefix;
        union {
            struct {
                uint8_t m : 5;
                uint8_t b : 1;
                uint8_t x : 1;
                uint8_t r : 1;
                uint8_t p : 2;
                uint8_t l : 1;
                uint8_t v : 4;
                uint8_t w : 1;
            };
            uint16_t value;
        };
    } vex;
    union {
        struct {
            uint8_t b : 1;
            uint8_t x : 1;
            uint8_t r : 1;
            uint8_t w : 1;
            uint8_t   : 4;
        };
        uint8_t value;
    } rex;
    union {
        struct {
            uint8_t rm  : 3;
            uint8_t reg : 3;
            uint8_t mod : 2;
        };
        struct {
            uint8_t     : 3;
            uint8_t opc : 3;
            uint8_t     : 2;
        };
        uint8_t value;
    } modrm;
    union {
        struct {
            uint8_t base  : 3;
            uint8_t index : 3;
            uint8_t scale : 2;
        };
        uint8_t value;
    } sib;
} em_context_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAX_ARCH_X86_32
#define EMCALL __stdcall
#else  // !HAX_ARCH_X86_32
#define EMCALL
#endif  // HAX_ARCH_X86_32

em_status_t EMCALL em_decode_insn(struct em_context_t *ctxt,
                                  const uint8_t *insn);

em_status_t EMCALL em_emulate_insn(struct em_context_t *ctxt);

#ifdef __cplusplus
}
#endif

#endif /* HAX_CORE_EMULATE_H_ */
