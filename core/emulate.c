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

#include "include/emulate.h"

/* Instruction flags */
/* Instruction does not read from destination */
#define INSN_DST_NR  ((uint64_t)1 <<  0)
/* Instruction does not write to destination */
#define INSN_DST_NW  ((uint64_t)1 <<  1)
/* Instruction expects ModRM byte */
#define INSN_MODRM   ((uint64_t)1 <<  2)
/* Instruction accesses 1-byte registers */
#define INSN_BYTEOP  ((uint64_t)1 <<  3)
/* Instruction opcode is extended via ModRM byte */
#define INSN_GROUP   ((uint64_t)1 <<  4)
/* Instruction supports REP prefixes */
#define INSN_REP     ((uint64_t)1 <<  5)
/* Instruction supports REPE/REPNE prefixes */
#define INSN_REPX    ((uint64_t)1 <<  6)
/* Instruction ignores flags */
#define INSN_NOFLAGS ((uint64_t)1 <<  7)
/* Instruction has two memory operands */
#define INSN_TWOMEM  ((uint64_t)1 <<  8)
/* Instruction takes bit test operands */
#define INSN_BITOP   ((uint64_t)1 <<  9)
/* Instruction accesses the stack */
#define INSN_STACK   ((uint64_t)1 << 10)
/* String instruction */
#define INSN_STRING  (INSN_REP|INSN_REPX)

// Implementation flags
#define INSN_NOTIMPL ((uint64_t)1 << 32)
#define INSN_FASTOP  ((uint64_t)1 << 33)

/* Operand flags */
#define OP_READ_PENDING        (1 <<  0)
#define OP_READ_FINISHED       (1 <<  1)
#define OP_WRITE_PENDING       (1 <<  2)
#define OP_WRITE_FINISHED      (1 <<  3)

/* Prefixes */
#define PREFIX_LOCK   0xF0
#define PREFIX_REPNE  0xF2
#define PREFIX_REPE   0xF3

#define  X1(...)  __VA_ARGS__
#define  X2(...)  X1(__VA_ARGS__), X1(__VA_ARGS__)
#define  X3(...)  X2(__VA_ARGS__), X1(__VA_ARGS__)
#define  X4(...)  X2(__VA_ARGS__), X2(__VA_ARGS__)
#define  X5(...)  X4(__VA_ARGS__), X1(__VA_ARGS__)
#define  X6(...)  X4(__VA_ARGS__), X2(__VA_ARGS__)
#define  X7(...)  X4(__VA_ARGS__), X3(__VA_ARGS__)
#define  X8(...)  X4(__VA_ARGS__), X4(__VA_ARGS__)
#define X16(...)  X8(__VA_ARGS__), X8(__VA_ARGS__)

/* Emulator ops */
#define BX (uint16_t)(gpr_read(ctxt, REG_RBX, 2))
#define BP (uint16_t)(gpr_read(ctxt, REG_RBP, 2))
#define SI (uint16_t)(gpr_read(ctxt, REG_RSI, 2))
#define DI (uint16_t)(gpr_read(ctxt, REG_RDI, 2))

/* Operand decoders */
#define DECL_DECODER(name) \
    static em_status_t decode_##name(em_context_t *, em_operand_t *)
DECL_DECODER(op_none);
DECL_DECODER(op_modrm_reg);
DECL_DECODER(op_modrm_rm);
DECL_DECODER(op_modrm_rm8);
DECL_DECODER(op_modrm_rm16);
DECL_DECODER(op_vex_reg);
DECL_DECODER(op_opc_reg);
DECL_DECODER(op_moffs);
DECL_DECODER(op_simm);
DECL_DECODER(op_simm8);
DECL_DECODER(op_acc);
DECL_DECODER(op_di);
DECL_DECODER(op_si);

#define \
    N { \
        .flags      = INSN_NOTIMPL \
    }
#define \
    I(_handler, _dec_dst, _dec_src1, _dec_src2, _flags) { \
        .handler      = &_handler,            \
        .decode_dst   = &decode_##_dec_dst,   \
        .decode_src1  = &decode_##_dec_src1,  \
        .decode_src2  = &decode_##_dec_src2,  \
        .flags        = _flags                \
    }
#define \
    G(_group, _dec_dst, _dec_src1, _dec_src2, _flags) { \
        .group        = _group,               \
        .decode_dst   = &decode_##_dec_dst,   \
        .decode_src1  = &decode_##_dec_src1,  \
        .decode_src2  = &decode_##_dec_src2,  \
        .flags        = _flags | INSN_GROUP | INSN_MODRM \
    }

#define \
    F(_handler, _dec_dst, _dec_src1, _dec_src2, _flags) \
    I(_handler, _dec_dst, _dec_src1, _dec_src2, _flags | INSN_FASTOP)

#define I2_BV(_handler, _dec_dst, _dec_src1, _dec_src2, _flags) \
    I(_handler, _dec_dst, _dec_src1, _dec_src2, (_flags | INSN_BYTEOP)), \
    I(_handler, _dec_dst, _dec_src1, _dec_src2, (_flags))
#define F2_BV(_handler, _dec_dst, _dec_src1, _dec_src2, _flags) \
    F(_handler, _dec_dst, _dec_src1, _dec_src2, (_flags | INSN_BYTEOP)), \
    F(_handler, _dec_dst, _dec_src1, _dec_src2, (_flags))

#define F6_ALU(_handler, _flags) \
    F2_BV(_handler, op_modrm_rm, op_modrm_reg, op_none, (_flags | INSN_MODRM)), \
    F2_BV(_handler, op_modrm_reg, op_modrm_rm, op_none, (_flags | INSN_MODRM)), \
    F2_BV(_handler, op_acc, op_simm, op_none, (_flags))

/* Soft-emulation */
static em_status_t em_andn_soft(struct em_context_t *ctxt);
static em_status_t em_bextr_soft(struct em_context_t *ctxt);
static em_status_t em_mov(struct em_context_t *ctxt);
static em_status_t em_movzx(struct em_context_t *ctxt);
static em_status_t em_movsx(struct em_context_t *ctxt);
static em_status_t em_push(struct em_context_t *ctxt);
static em_status_t em_pop(struct em_context_t *ctxt);
static em_status_t em_xchg(struct em_context_t *ctxt);

static const struct em_opcode_t opcode_group1[8] = {
    F(em_add, op_none, op_none, op_none, 0),
    F(em_or,  op_none, op_none, op_none, 0),
    F(em_adc, op_none, op_none, op_none, 0),
    F(em_sbb, op_none, op_none, op_none, 0),
    F(em_and, op_none, op_none, op_none, 0),
    F(em_sub, op_none, op_none, op_none, 0),
    F(em_xor, op_none, op_none, op_none, 0),
    F(em_cmp, op_none, op_none, op_none, 0),
};

static const struct em_opcode_t opcode_group3[8] = {
    F(em_test, op_modrm_rm, op_simm, op_none, INSN_DST_NW),
    F(em_test, op_modrm_rm, op_simm, op_none, INSN_DST_NW),
    F(em_not, op_modrm_rm, op_none, op_none, 0),
    F(em_neg, op_modrm_rm, op_none, op_none, 0),
};

static const struct em_opcode_t opcode_group5[8] = {
    X4(N),
    X2(N),
    I(em_push, op_none, op_modrm_rm, op_none, INSN_TWOMEM | INSN_STACK),
    X1(N),
};

static const struct em_opcode_t opcode_group8[8] = {
    X4(N),
    F(em_bt, op_none, op_none, op_none, 0),
    F(em_bts, op_none, op_none, op_none, 0),
    F(em_btr, op_none, op_none, op_none, 0),
    F(em_btc, op_none, op_none, op_none, 0),
};

static const struct em_opcode_t opcode_group11[8] = {
    I(em_mov, op_none, op_none, op_none, INSN_DST_NR),
};

static const struct em_opcode_t opcode_group1A[8] = {
    I(em_pop, op_modrm_rm, op_none, op_none, INSN_DST_NR | INSN_TWOMEM | INSN_STACK),
};

static const struct em_opcode_t opcode_table[256] = {
    /* 0x00 - 0x07 */
    F6_ALU(em_add, 0), X2(N),
    /* 0x08 - 0x0F */
    F6_ALU(em_or, 0),  X2(N),
    /* 0x10 - 0x17 */
    F6_ALU(em_adc, 0), X2(N),
    /* 0x18 - 0x1F */
    F6_ALU(em_sbb, 0), X2(N),
    /* 0x20 - 0x27 */
    F6_ALU(em_and, 0), X2(N),
    /* 0x28 - 0x2F */
    F6_ALU(em_sub, 0), X2(N),
    /* 0x30 - 0x37 */
    F6_ALU(em_xor, 0), X2(N),
    /* 0x38 - 0x3F */
    F6_ALU(em_cmp, INSN_DST_NW), X2(N),
    /* 0x40 - 0x47 */
    X8(F(em_inc, op_modrm_reg, op_none, op_none, 0)),
    /* 0x48 - 0x4F */
    X8(F(em_dec, op_modrm_reg, op_none, op_none, 0)),
    /* 0x50 - 0x5F */
    X8(I(em_push, op_none, op_opc_reg, op_none, INSN_STACK)),
    X8(I(em_pop,  op_opc_reg, op_none, op_none, INSN_STACK)),
    /* 0x60 - 0x7F */
    X16(N), X16(N),
    /* 0x80 - 0x8F */
    G(opcode_group1, op_modrm_rm, op_simm, op_none, INSN_BYTEOP),
    G(opcode_group1, op_modrm_rm, op_simm, op_none, 0),
    G(opcode_group1, op_modrm_rm, op_simm, op_none, INSN_BYTEOP),
    G(opcode_group1, op_modrm_rm, op_simm8, op_none, 0),
    F2_BV(em_test, op_modrm_rm, op_modrm_reg, op_none, INSN_MODRM | INSN_DST_NW),
    X2(N),  /* TODO: 0x86 & 0x87 (XCHG) */
    I2_BV(em_mov, op_modrm_rm, op_modrm_reg, op_none, INSN_MODRM | INSN_DST_NR),
    I2_BV(em_mov, op_modrm_reg, op_modrm_rm, op_none, INSN_MODRM | INSN_DST_NR),
    N, N, N,
    G(opcode_group1A, op_none, op_none, op_none, 0),
    /* 0x90 - 0x9F */
    X16(N),
    /* 0xA0 - 0xAF */
    I2_BV(em_mov, op_acc, op_moffs, op_none, INSN_DST_NR),
    I2_BV(em_mov, op_moffs, op_acc, op_none, INSN_DST_NR),
    I2_BV(em_mov, op_di, op_si, op_none, INSN_DST_NR | INSN_REP | INSN_TWOMEM), /* movs{b,w,d,q} */
    F2_BV(em_cmp, op_di, op_si, op_none, INSN_DST_NW | INSN_REPX | INSN_TWOMEM), /* cmps{b,w,d,q} */
    X2(N),
    I2_BV(em_mov, op_di, op_acc, op_none, INSN_DST_NR | INSN_REP), /* stos{b,w,d,q} */
    I2_BV(em_mov, op_acc, op_si, op_none, INSN_DST_NR | INSN_REP), /* lods{b,w,d,q} */
    X2(N),
    /* 0xB0 - 0xBF */
    X16(N),
    /* 0xC0 - 0xCF */
    X4(N),
    X2(N),
    G(opcode_group11, op_modrm_rm, op_simm, op_none, INSN_BYTEOP),
    G(opcode_group11, op_modrm_rm, op_simm, op_none, 0),
    X8(N),
    /* 0xD0 - 0xEF */
    X16(N), X16(N),
    /* 0xF0 - 0xFF */
    X4(N),
    X2(N),
    G(opcode_group3, op_none, op_none, op_none, INSN_BYTEOP),
    G(opcode_group3, op_none, op_none, op_none, 0),
    X4(N),
    X2(N),
    X1(N),
    G(opcode_group5, op_none, op_none, op_none, 0),
};

static const struct em_opcode_t opcode_table_0F[256] = {
    /* 0x00 - 0x9F */
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N),
    /* 0xA0 - 0xAF */
    X3(N),
    F(em_bt, op_modrm_rm, op_modrm_reg, op_none, INSN_MODRM | INSN_BITOP),
    X7(N),
    F(em_bts, op_modrm_rm, op_modrm_reg, op_none, INSN_MODRM | INSN_BITOP),
    X4(N),
    /* 0xB0 - 0xBF */
    X3(N),
    F(em_btr, op_modrm_rm, op_modrm_reg, op_none, INSN_MODRM | INSN_BITOP),
    X2(N),
    I(em_movzx, op_modrm_reg, op_modrm_rm8, op_none, INSN_MODRM | INSN_DST_NR),
    I(em_movzx, op_modrm_reg, op_modrm_rm16, op_none, INSN_MODRM | INSN_DST_NR),
    X2(N),
    G(opcode_group8, op_modrm_rm, op_simm8, op_none, INSN_BITOP),
    F(em_btc, op_modrm_rm, op_modrm_reg, op_none, INSN_MODRM | INSN_BITOP),
    X2(N),
    I(em_movsx, op_modrm_reg, op_modrm_rm8, op_none, INSN_MODRM | INSN_DST_NR),
    I(em_movsx, op_modrm_reg, op_modrm_rm16, op_none, INSN_MODRM | INSN_DST_NR),
    /* 0xC0 - 0xFF */
    X16(N), X16(N), X16(N), X16(N),
};

static const struct em_opcode_t opcode_table_0F38[256] = {
    /* 0x00 - 0xEF */
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N),
    /* 0xF0 - 0xFF */
    X2(N),
    I(em_andn_soft, op_modrm_reg, op_vex_reg, op_modrm_rm, INSN_MODRM),
    X4(N),
    I(em_bextr_soft, op_modrm_reg, op_modrm_rm, op_vex_reg, INSN_MODRM),
    X8(N),
};

static const struct em_opcode_t opcode_table_0F3A[256] = {
    /* 0x00 - 0xFF */
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N), X16(N),
    X16(N), X16(N), X16(N), X16(N),
};

static uint64_t gpr_read_shifted(struct em_context_t *ctxt, unsigned index,
                                 size_t size, uint32_t shift)
{
    uint64_t value = 0;
    uint8_t *value_ptr;

    if (!(ctxt->gpr_cache_r & (1 << index))) {
        ctxt->gpr_cache[index] = ctxt->ops->read_gpr(ctxt->vcpu, index);
        ctxt->gpr_cache_r |= (1 << index);
    }
    value_ptr = (uint8_t *)&ctxt->gpr_cache[index];
    memcpy(&value, &value_ptr[shift], size);
    return value;
}

static void gpr_write_shifted(struct em_context_t *ctxt, unsigned index,
                              size_t size, uint64_t value, uint32_t shift)
{
    uint8_t *value_ptr;

    if (!(ctxt->gpr_cache_r & (1 << index))) {
        ctxt->gpr_cache[index] = ctxt->ops->read_gpr(ctxt->vcpu, index);
        ctxt->gpr_cache_r |= (1 << index);
    }
    ctxt->gpr_cache_w |= (1 << index);
    value_ptr = (uint8_t *)&ctxt->gpr_cache[index];
    memcpy(&value_ptr[shift], &value, size);
}

static uint64_t gpr_read(struct em_context_t *ctxt,
                         unsigned index, size_t size)
{
    return gpr_read_shifted(ctxt, index, size, 0);
}

static void gpr_write(struct em_context_t *ctxt,
                      unsigned index, size_t size, uint64_t value)
{
    gpr_write_shifted(ctxt, index, size, value, 0);
}

static void gpr_cache_flush(struct em_context_t *ctxt)
{
    unsigned i;

    for (i = 0; i < 16; i++) {
        if (ctxt->gpr_cache_w & (1 << i))
            ctxt->ops->write_gpr(ctxt->vcpu, i, ctxt->gpr_cache[i]);
    }
    ctxt->gpr_cache_w = 0;
}

static void gpr_cache_invalidate(struct em_context_t *ctxt)
{
    ctxt->gpr_cache_r = 0;
    ctxt->gpr_cache_w = 0;
}

/* Emulate accesses to guest memory */
static bool is_translation_required(struct em_context_t *ctxt)
{
    const struct em_opcode_t *opcode = &ctxt->opcode;

    if (opcode->flags & INSN_TWOMEM) {
        return true;
    }
    if (ctxt->rep) {
        return true;
    }
    return false;
}

static uint64_t get_canonical_address(struct em_context_t *ctxt,
                                      uint64_t addr, uint vaddr_bits)
{
    return ((int64_t)addr << (64 - vaddr_bits)) >> (64 - vaddr_bits);
}

static em_status_t get_linear_address(struct em_context_t *ctxt,
                                      struct operand_mem_t *mem,
                                      uint64_t *la)
{
    uint64_t addr;

    switch (ctxt->address_size) {
    case 2:
        addr = (uint16_t)mem->ea;
        break;
    case 4:
        addr = (uint32_t)mem->ea;
        break;
    case 8:
        addr = (uint64_t)mem->ea;
        break;
    default:
        return EM_ERROR;
    }

    if (ctxt->mode == EM_MODE_PROT64) {
        if (mem->seg == SEG_FS || mem->seg == SEG_GS) {
            addr += ctxt->ops->get_segment_base(ctxt->vcpu, mem->seg);
        }
        addr = get_canonical_address(ctxt, addr, 48);
    } else if (ctxt->mode == EM_MODE_REAL) {
        // Add segment base and truncate to 20-bits
        addr += ctxt->ops->get_segment_base(ctxt->vcpu, mem->seg);
        addr &= 0xFFFFF;
    } else {
        // Add segment base and truncate to 32-bits
        addr += ctxt->ops->get_segment_base(ctxt->vcpu, mem->seg);
        addr &= 0xFFFFFFFF;
    }
    *la = addr;
    return EM_CONTINUE;
}

static em_status_t segmented_read(struct em_context_t *ctxt,
                                  struct operand_mem_t *mem,
                                  void *data, unsigned size)
{
    uint32_t flags = 0;
    uint64_t la;
    em_status_t rc;

    rc = get_linear_address(ctxt, mem, &la);
    if (rc != EM_CONTINUE) {
        return rc;
    }
    if (!is_translation_required(ctxt)) {
        flags |= EM_OPS_NO_TRANSLATION;
    }
    return ctxt->ops->read_memory(ctxt->vcpu, la, data, size, flags);
}

static em_status_t segmented_write(struct em_context_t *ctxt,
                                   struct operand_mem_t *mem,
                                   void *data, unsigned size)
{
    uint32_t flags = 0;
    uint64_t la;
    em_status_t rc;

    rc = get_linear_address(ctxt, mem, &la);
    if (rc != EM_CONTINUE) {
        return rc;
    }
    if (!is_translation_required(ctxt)) {
        flags |= EM_OPS_NO_TRANSLATION;
    }
    return ctxt->ops->write_memory(ctxt->vcpu, la, data, size, flags);
}

static em_status_t operand_read_bitop(struct em_context_t *ctxt)
{
    em_status_t rc;
    struct em_operand_t *base = &ctxt->dst;
    struct em_operand_t *offs = &ctxt->src1;
    int64_t offset;

    if (base->flags & OP_READ_PENDING) {
        rc = ctxt->ops->read_memory_post(ctxt->vcpu, &base->value, base->size);
        return rc;
    }

    offset = gpr_read(ctxt, offs->reg.index, offs->size);
    switch (offs->size) {
    case 2:
        offset = (int16_t)offset;
        break;
    case 4:
        offset = (int32_t)offset;
        break;
    case 8:
        offset = (int64_t)offset;
        break;
    default:
        return EM_ERROR;
    }
    base->mem.ea += (offset >> 3);
    offs->value = (offset & 0x7);
    rc = segmented_read(ctxt, &base->mem, &base->value, base->size);
    if (rc != EM_CONTINUE) {
        base->flags |= OP_READ_PENDING;
    }
    return rc;
}

static em_status_t operand_read(struct em_context_t *ctxt,
                                struct em_operand_t *op)
{
    em_status_t rc;
    if (op->flags & OP_READ_FINISHED) {
        return EM_CONTINUE;
    }

    switch (op->type) {
    case OP_NONE:
        // Exit directly to prevent writing flags
        return EM_CONTINUE;
    case OP_IMM:
        rc = EM_CONTINUE;
        break;
    case OP_REG:
        op->value = gpr_read_shifted(ctxt, op->reg.index, op->size, op->reg.shift);
        rc = EM_CONTINUE;
        break;
    case OP_MEM:
        if (op->flags & OP_READ_PENDING) {
            rc = ctxt->ops->read_memory_post(ctxt->vcpu, &op->value, op->size);
        } else {
            rc = segmented_read(ctxt, &op->mem, &op->value, op->size);
        }
        break;
    default:
        rc = EM_ERROR;
        break;
    }

    if (rc == EM_CONTINUE) {
        op->flags |= OP_READ_FINISHED;
    } else {
        op->flags |= OP_READ_PENDING;
    }
    return rc;
}

static em_status_t operand_write(struct em_context_t *ctxt,
                                 struct em_operand_t *op)
{
    size_t size;
    em_status_t rc;
    if (op->flags & OP_WRITE_FINISHED) {
        return EM_CONTINUE;
    }

    switch (op->type) {
    case OP_NONE:
        // Exit directly to prevent writing flags
        return EM_CONTINUE;
    case OP_IMM:
        rc = EM_CONTINUE;
        break;
    case OP_REG:
        size = (op->size == 4) ? 8 : op->size;
        gpr_write_shifted(ctxt, op->reg.index, size, op->value, op->reg.shift);
        rc = EM_CONTINUE;
        break;
    case OP_MEM:
        if (op->flags & OP_WRITE_PENDING) {
            rc = EM_CONTINUE;
        } else {
            rc = segmented_write(ctxt, &op->mem, &op->value, op->size);
        }
        break;
    default:
        rc = EM_ERROR;
        break;
    }

    if (rc == EM_CONTINUE) {
        op->flags |= OP_WRITE_FINISHED;
    } else {
        op->flags |= OP_WRITE_PENDING;
    }
    return rc;
}

static void register_add(struct em_context_t *ctxt,
                         int reg_index, uint64_t addend)
{
    uint64_t value = gpr_read(ctxt, reg_index, 8) + addend;
    gpr_write(ctxt, reg_index, 8, value);
}

static uint8_t insn_fetch_u8(struct em_context_t *ctxt)
{
    uint8_t result;

    if (ctxt->len >= INSTR_MAX_LEN)
        return 0;

    result = *(uint8_t *)(&ctxt->insn[ctxt->len]);
    ctxt->len += 1;
    return result;
}

static uint16_t insn_fetch_u16(struct em_context_t *ctxt)
{
    uint16_t result;

    if (ctxt->len >= INSTR_MAX_LEN)
        return 0;

    result = *(uint16_t *)(&ctxt->insn[ctxt->len]);
    ctxt->len += 2;
    return result;
}

static uint32_t insn_fetch_u32(struct em_context_t *ctxt)
{
    uint32_t result;

    if (ctxt->len >= INSTR_MAX_LEN)
        return 0;

    result = *(uint32_t *)(&ctxt->insn[ctxt->len]);
    ctxt->len += 4;
    return result;
}

static uint64_t insn_fetch_u64(struct em_context_t *ctxt)
{
    uint64_t result;

    if (ctxt->len >= INSTR_MAX_LEN)
        return 0;

    result = *(uint64_t *)(&ctxt->insn[ctxt->len]);
    ctxt->len += 8;
    return result;
}

static int8_t insn_fetch_s8(struct em_context_t *ctxt)
{
    return (int8_t)insn_fetch_u8(ctxt);
}

static int16_t insn_fetch_s16(struct em_context_t *ctxt)
{
    return (int16_t)insn_fetch_u16(ctxt);
}

static int32_t insn_fetch_s32(struct em_context_t *ctxt)
{
    return (int32_t)insn_fetch_u32(ctxt);
}

static int64_t insn_fetch_s64(struct em_context_t *ctxt)
{
    return (int64_t)insn_fetch_u64(ctxt);
}

static void decode_prefixes(struct em_context_t *ctxt)
{
    uint8_t b;

    /* Intel SDM Vol. 2A: 2.1.1 Instruction Prefixes  */
    while (true) {
        b = insn_fetch_u8(ctxt);
        switch (b) {
        /* Group 1: Lock and repeat prefixes */
        case PREFIX_LOCK:
            // Ignored (is it possible to emulate atomic operations?)
            ctxt->lock = b;
            break;
        case PREFIX_REPNE:
        case PREFIX_REPE:
            ctxt->rep = b;
            break;
        /* Group 2: Segment override prefixes */
        case 0x2E:
            ctxt->override_segment = SEG_CS;
            break;
        case 0x36:
            ctxt->override_segment = SEG_SS;
            break;
        case 0x3E:
            ctxt->override_segment = SEG_DS;
            break;
        case 0x26:
            ctxt->override_segment = SEG_ES;
            break;
        case 0x64:
            ctxt->override_segment = SEG_FS;
            break;
        case 0x65:
            ctxt->override_segment = SEG_GS;
            break;
        /* Group 3: Operand-size override prefix */
        case 0x66:
            ctxt->override_operand_size = 1;
            break;
        /* Group 4: Address-size override prefix */
        case 0x67:
            ctxt->override_address_size = 1;
            break;
        default:
            ctxt->len--;
            return;
        }
    }
}

static em_status_t decode_operands(struct em_context_t *ctxt)
{
    const struct em_opcode_t *opcode = &ctxt->opcode;
    em_status_t rc;

    if (opcode->decode_dst) {
        ctxt->dst.flags = 0;
        rc = opcode->decode_dst(ctxt, &ctxt->dst);
        if (rc != EM_CONTINUE)
            return rc;
    }
    if (opcode->decode_src1) {
        ctxt->src1.flags = 0;
        rc = opcode->decode_src1(ctxt, &ctxt->src1);
        if (rc != EM_CONTINUE)
            return rc;
    }
    if (opcode->decode_src2) {
        ctxt->src2.flags = 0;
        rc = opcode->decode_src2(ctxt, &ctxt->src2);
        if (rc != EM_CONTINUE)
            return rc;
    }
    return EM_CONTINUE;
}

static em_status_t decode_op_none(em_context_t *ctxt,
                                  em_operand_t *op)
{
    op->type = OP_NONE;
    return EM_CONTINUE;
}

static em_status_t decode_op_modrm_reg(em_context_t *ctxt,
                                       em_operand_t *op)
{
    op->type = OP_REG;
    op->size = ctxt->operand_size;
    op->reg.index = ctxt->modrm.reg | (ctxt->rex.r << 3);
    op->reg.shift = 0;

    if (op->size == 1 && !ctxt->rex.value) {
        op->reg.shift = op->reg.index >> 2;
        op->reg.index &= 0x3;
    }
    return EM_CONTINUE;
}

static em_status_t decode_op_modrm_rm(em_context_t *ctxt,
                                      em_operand_t *op)
{
    // Register operand
    if (ctxt->modrm.mod == 3) {
        op->type = OP_REG;
        op->size = ctxt->operand_size;
        op->reg.index = ctxt->modrm.rm | (ctxt->rex.b << 3);
        op->reg.shift = 0;

        if (op->size == 1 && !ctxt->rex.value) {
            op->reg.shift = op->reg.index >> 2;
            op->reg.index &= 0x3;
        }
        return EM_CONTINUE;
    }

    // Memory operand
    op->type = OP_MEM;
    op->size = ctxt->operand_size;
    op->mem.ea = 0;
    op->mem.seg = SEG_DS;

    if (ctxt->address_size == 2) {
        /* Intel SDM Vol. 2A:
         * Table 2-1. 16-Bit Addressing Forms with the ModR/M Byte */
        switch (ctxt->modrm.rm) {
        case 0:
            op->mem.ea = BX + SI;
            break;
        case 1:
            op->mem.ea = BX + DI;
            break;
        case 2:
            op->mem.ea = BP + SI;
            op->mem.seg = SEG_SS;
            break;
        case 3:
            op->mem.ea = BP + DI;
            op->mem.seg = SEG_SS;
            break;
        case 4:
            op->mem.ea = SI;
            break;
        case 5:
            op->mem.ea = DI;
            break;
        case 6:
            if (ctxt->modrm.mod == 0) {
                op->mem.ea = insn_fetch_u16(ctxt);
            } else {
                op->mem.ea = BP;
                op->mem.seg = SEG_SS;
            }
            break;
        case 7:
            op->mem.ea = BX;
            break;
        }

        // Displacement
        if (ctxt->modrm.mod == 1) {
            op->mem.ea += insn_fetch_s8(ctxt);
        } else if (ctxt->modrm.mod == 2) {
            op->mem.ea += insn_fetch_u16(ctxt);
        }
    } else if (ctxt->address_size == 4 || ctxt->address_size == 8) {
        /* Intel SDM Vol. 2A:
         * Table 2-2. 32-Bit Addressing Forms with the ModR/M Byte */
        if (ctxt->modrm.rm == 4) {
            uint32_t reg_base;
            uint32_t reg_index;

            /* Intel SDM Vol. 2A:
             * Table 2-3. 32-Bit Addressing Forms with the SIB Byte */
            ctxt->sib.value = insn_fetch_u8(ctxt);
            reg_base  = ctxt->sib.base  | (ctxt->rex.b << 3);
            reg_index = ctxt->sib.index | (ctxt->rex.x << 3);
            if ((reg_base & 7) == 5 && ctxt->modrm.mod == 0) {
                op->mem.ea += insn_fetch_s32(ctxt);
            } else {
                op->mem.ea += gpr_read(ctxt, reg_base, ctxt->address_size);
            }
            /* Added scaled index register unless register is RSP/ESP/SP.
             * Note that we avoid masking with 0x7, as that would prevent
             * R12/R12D/R12W from being a valid index register. */
            if (reg_index != 4) {
                uint8_t scale = 1 << ctxt->sib.scale;
                op->mem.ea += gpr_read(ctxt, reg_index, ctxt->address_size) * scale;
            }
        } else if (ctxt->modrm.mod == 0 && ctxt->modrm.rm == 5) {
            op->mem.ea += insn_fetch_s32(ctxt);
        } else {
            op->mem.ea += gpr_read(ctxt, ctxt->modrm.rm, ctxt->address_size);
        }

        // Displacement
        if (ctxt->modrm.mod == 1) {
            op->mem.ea += insn_fetch_s8(ctxt);
        } else if (ctxt->modrm.mod == 2) {
            op->mem.ea += insn_fetch_s32(ctxt);
        }
    }

    if (ctxt->override_segment != SEG_NONE) {
        op->mem.seg = ctxt->override_segment;
    }
    return EM_CONTINUE;
}

static em_status_t decode_op_modrm_rm8(em_context_t *ctxt,
                                       em_operand_t *op)
{
    em_status_t rc;
    rc = decode_op_modrm_rm(ctxt, op);
    op->size = 1;
    return rc;
}

static em_status_t decode_op_modrm_rm16(em_context_t *ctxt,
                                        em_operand_t *op)
{
    em_status_t rc;
    rc = decode_op_modrm_rm(ctxt, op);
    op->size = 2;
    return rc;
}

static em_status_t decode_op_vex_reg(em_context_t *ctxt,
                                     em_operand_t *op)
{
    op->type = OP_REG;
    op->size = ctxt->operand_size;
    op->reg.index = ~ctxt->vex.v & 0xF;
    op->reg.shift = 0;
    return EM_CONTINUE;
}

static em_status_t decode_op_opc_reg(em_context_t *ctxt,
                                     em_operand_t *op)
{
    op->type = OP_REG;
    op->size = ctxt->operand_size;
    op->reg.index = (ctxt->b & 0x7) | (ctxt->rex.b << 3);
    op->reg.shift = 0;

    if (op->size == 1 && !ctxt->rex.value) {
        op->reg.shift = op->reg.index >> 2;
        op->reg.index &= 0x3;
    }
    return EM_CONTINUE;
}

static em_status_t decode_op_moffs(em_context_t *ctxt,
                                   em_operand_t *op)
{
    op->type = OP_MEM;
    op->size = ctxt->operand_size;
    op->mem.ea = 0;
    op->mem.seg = SEG_DS;
    switch (ctxt->address_size) {
    case 2:
        op->mem.ea = insn_fetch_u16(ctxt);
        break;
    case 4:
        op->mem.ea = insn_fetch_u32(ctxt);
        break;
    case 8:
        op->mem.ea = insn_fetch_u64(ctxt);
        break;
    default:
        return EM_ERROR;
    }
    if (ctxt->override_segment != SEG_NONE) {
        op->mem.seg = ctxt->override_segment;
    }
    return EM_CONTINUE;
}

static em_status_t decode_op_simm(em_context_t *ctxt,
                                 em_operand_t *op)
{
    op->type = OP_IMM;
    op->size = ctxt->operand_size;
    switch (op->size) {
    case 1:
        op->value = insn_fetch_s8(ctxt);
        break;
    case 2:
        op->value = insn_fetch_s16(ctxt);
        break;
    case 4:
        op->value = insn_fetch_s32(ctxt);
        break;
    case 8:
        op->value = insn_fetch_s32(ctxt);
        break;
    default:
        return EM_ERROR;
    }
    return EM_CONTINUE;
}

static em_status_t decode_op_simm8(em_context_t *ctxt,
                                   em_operand_t *op)
{
    op->type = OP_IMM;
    op->size = 1;
    op->value = (int64_t)(insn_fetch_s8(ctxt));
    return EM_CONTINUE;
}

static em_status_t decode_op_acc(em_context_t *ctxt,
                                 em_operand_t *op)
{
    op->type = OP_REG;
    op->size = ctxt->operand_size;
    op->reg.index = REG_RAX;
    op->reg.shift = 0;
    return EM_CONTINUE;
}

static em_status_t decode_op_di(em_context_t *ctxt,
                                em_operand_t *op)
{
    op->type = OP_MEM;
    op->size = ctxt->operand_size;
    op->mem.ea = gpr_read(ctxt, REG_RDI, ctxt->address_size);
    op->mem.seg = SEG_ES;
    return EM_CONTINUE;
}

static em_status_t decode_op_si(em_context_t *ctxt,
                                em_operand_t *op)
{
    op->type = OP_MEM;
    op->size = ctxt->operand_size;
    op->mem.ea = gpr_read(ctxt, REG_RSI, ctxt->address_size);
    op->mem.seg = SEG_DS;
    if (ctxt->override_segment != SEG_NONE) {
        op->mem.seg = ctxt->override_segment;
    }
    return EM_CONTINUE;
}

/* Soft-emulation */
static em_status_t em_andn_soft(struct em_context_t *ctxt)
{
    uint64_t temp;
    int signbit = 63;
    temp = ~ctxt->src1.value & ctxt->src2.value;
    if (ctxt->operand_size == 4) {
        temp &= 0xFFFFFFFF;
        signbit = 31;
    }
    ctxt->dst.value = temp;
    ctxt->rflags &= ~RFLAGS_MASK_OSZAPC;
    ctxt->rflags |= (temp == 0) ? RFLAGS_ZF : 0;
    ctxt->rflags |= (temp >> signbit) ? RFLAGS_SF : 0;
    return EM_CONTINUE;
}

static em_status_t em_bextr_soft(struct em_context_t *ctxt)
{
    uint8_t start, len;
    uint64_t temp;

    start = ctxt->src2.value & 0xFF;
    len = (ctxt->src2.value >> 8) & 0xFF;
    temp = ctxt->src1.value;
    if (ctxt->operand_size == 4) {
        temp &= 0xFFFFFFFF;
    }
    temp >>= start;
    temp &= ~(~0ULL << len);
    ctxt->dst.value = temp;
    ctxt->rflags &= ~RFLAGS_MASK_OSZAPC;
    ctxt->rflags |= (temp == 0) ? RFLAGS_ZF : 0;
    return EM_CONTINUE;
}

static em_status_t em_mov(struct em_context_t *ctxt)
{
    memcpy(&ctxt->dst.value, &ctxt->src1.value, ctxt->operand_size);
    return EM_CONTINUE;
}

static em_status_t em_movzx(struct em_context_t *ctxt)
{
    ctxt->dst.value = 0;
    memcpy(&ctxt->dst.value, &ctxt->src1.value, ctxt->src1.size);
    return EM_CONTINUE;
}

static em_status_t em_movsx(struct em_context_t *ctxt)
{
    uint64_t extension = 0;
    if (ctxt->src1.value & (1ULL << ((8 * ctxt->src1.size) - 1))) {
        extension = ~0ULL;
    }
    ctxt->dst.value = 0;
    memcpy(&ctxt->dst.value, &extension, ctxt->dst.size);
    memcpy(&ctxt->dst.value, &ctxt->src1.value, ctxt->src1.size);
    return EM_CONTINUE;
}

static em_status_t em_push(struct em_context_t *ctxt)
{
    em_status_t rc;
    em_operand_t *sp = &ctxt->dst;

    // Pre-decrement stack pointer
    register_add(ctxt, REG_RSP, -(int64_t)ctxt->operand_size);

    // TODO: The stack address size can be different from ctxt->address_size.
    // We need to check the B-bit from GUEST_SS_AR to determine the actual size.
    sp->type = OP_MEM;
    sp->size = ctxt->operand_size;
    sp->mem.ea = gpr_read(ctxt, REG_RSP, ctxt->address_size);
    sp->mem.seg = SEG_SS;
    sp->value = ctxt->src1.value;

    rc = operand_write(ctxt, sp);
    return rc;
}

static em_status_t em_pop(struct em_context_t *ctxt)
{
    em_status_t rc;
    em_operand_t *sp = &ctxt->src1;

    // TODO: The stack address size can be different from ctxt->address_size.
    // We need to check the B-bit from GUEST_SS_AR to determine the actual size.
    sp->type = OP_MEM;
    sp->size = ctxt->operand_size;
    sp->mem.ea = gpr_read(ctxt, REG_RSP, ctxt->address_size);
    sp->mem.seg = SEG_SS;

    // Post-increment stack pointer
    register_add(ctxt, REG_RSP, +(int64_t)ctxt->operand_size);

    rc = operand_read(ctxt, sp);
    if (rc != EM_CONTINUE)
        return rc;
    ctxt->dst.value = sp->value;
    return rc;
}

static em_status_t em_xchg(struct em_context_t *ctxt)
{
    em_status_t rc;
    uint64_t src1, src2;

    src1 = ctxt->src1.value;
    src2 = ctxt->src2.value;
    ctxt->src1.value = src2;
    ctxt->src2.value = src1;
    rc = operand_write(ctxt, &ctxt->src1);
    if (rc != EM_CONTINUE)
        return rc;
    return operand_write(ctxt, &ctxt->src2);
}

em_status_t EMCALL em_decode_insn(struct em_context_t *ctxt, const uint8_t *insn)
{
    uint8_t b;
    uint64_t flags;
    struct em_opcode_t *opcode;

    switch (ctxt->mode) {
    case EM_MODE_REAL:
    case EM_MODE_PROT16:
        ctxt->operand_size = 2;
        ctxt->address_size = 2;
        break;
    case EM_MODE_PROT32:
        ctxt->operand_size = 4;
        ctxt->address_size = 4;
        break;
    case EM_MODE_PROT64:
        ctxt->operand_size = 4;
        ctxt->address_size = 8;
        break;
    default:
        return EM_ERROR;
    }
    ctxt->gpr_cache_r = 0;
    ctxt->gpr_cache_w = 0;
    ctxt->override_segment = SEG_NONE;
    ctxt->override_operand_size = 0;
    ctxt->override_address_size = 0;
    ctxt->insn = insn;
    ctxt->lock = 0;
    ctxt->rep = 0;
    ctxt->len = 0;
    decode_prefixes(ctxt);

    /* Apply legacy prefixes */
    if (ctxt->override_operand_size) {
        ctxt->operand_size ^= (2 | 4);
    }
    if (ctxt->override_address_size) {
        ctxt->address_size ^= (ctxt->mode != EM_MODE_PROT64) ? (2 | 4) : (4 | 8);
    }

    /* Intel SDM Vol. 2A: 2.2.1 REX Prefixes */
    ctxt->rex.value = 0;
    b = insn_fetch_u8(ctxt);
    if (ctxt->mode == EM_MODE_PROT64 && b >= 0x40 && b <= 0x4F) {
        ctxt->rex.value = b;
        if (ctxt->rex.w) {
            ctxt->operand_size = 8;
        }
        b = insn_fetch_u8(ctxt);
    }

    /* Intel SDM Vol. 2A: 2.1.2 Opcodes */
    opcode = &ctxt->opcode;
    *opcode = opcode_table[b];
    switch (b) {
    case 0x0F:
        b = insn_fetch_u8(ctxt);
        switch (b) {
        case 0x38:
            b = insn_fetch_u8(ctxt);
            *opcode = opcode_table_0F38[b];
            break;
        case 0x3A:
            b = insn_fetch_u8(ctxt);
            *opcode = opcode_table_0F3A[b];
            break;
        default:
            *opcode = opcode_table_0F[b];
        }
        break;

    case 0xC4:
    case 0xC5:
        /* Intel SDM Vol. 2A: 2.3.5 The VEX Prefix */
        ctxt->vex.prefix = b;
        if (b == 0xC4) {
            ctxt->vex.value = insn_fetch_u16(ctxt);
        } else {
            ctxt->vex.value = insn_fetch_u8(ctxt) << 8;
            ctxt->vex.r = ctxt->vex.w;
            ctxt->vex.w = 1;
            ctxt->vex.x = 1;
            ctxt->vex.b = 1;
            ctxt->vex.m = 1;
        }
        if (ctxt->mode != EM_MODE_PROT64 && (!ctxt->vex.r || !ctxt->vex.x)) {
            // TODO: Reinterpret as the LES (0xC4) or LDS (0xC5) instruction.
            return EM_ERROR;
        }
        if (ctxt->lock || ctxt->rep || ctxt->override_operand_size || ctxt->rex.value) {
            /* Intel SDM Vol. 2A: 2.3.2 VEX and the LOCK prefix */
            /* Intel SDM Vol. 2A: 2.3.3 VEX and the 66H, F2H, and F3H prefixes */
            /* Intel SDM Vol. 2A: 2.3.4 VEX and the REX prefix */
            // TODO: Should throw #UD
            return EM_ERROR;
        }

        ctxt->rex.r = ~ctxt->vex.r;
        ctxt->rex.x = ~ctxt->vex.x;
        ctxt->rex.b = ~ctxt->vex.b;
        if (ctxt->mode == EM_MODE_PROT64 && ctxt->vex.w) {
            ctxt->operand_size = 8;
        }

        b = insn_fetch_u8(ctxt);
        switch (ctxt->vex.m) {
        case 1:
            *opcode = opcode_table_0F[b];
            break;
        case 2:
            *opcode = opcode_table_0F38[b];
            break;
        case 3:
            *opcode = opcode_table_0F3A[b];
            break;
        default:
            // TODO: Should throw #UD
            return EM_ERROR;
        }
        break;
    }
    ctxt->b = b;

    /* Intel SDM Vol. 2A: 2.1.3 ModR/M and SIB Bytes */
    flags = opcode->flags;
    if (flags & INSN_MODRM) {
        ctxt->modrm.value = insn_fetch_u8(ctxt);
    }

    /* Apply flags */
    if (flags & INSN_GROUP) {
        const struct em_opcode_t *group_opcode;

        if (!opcode->group) {
            /* Should never happen */
            return EM_ERROR;
        }
        group_opcode = &opcode->group[ctxt->modrm.opc];
        opcode->handler = group_opcode->handler;
        opcode->flags |= group_opcode->flags;
        flags = opcode->flags;
        if (group_opcode->decode_dst != decode_op_none) {
            opcode->decode_dst = group_opcode->decode_dst;
        }
        if (group_opcode->decode_src1 != decode_op_none) {
            opcode->decode_src1 = group_opcode->decode_src1;
        }
        if (group_opcode->decode_src2 != decode_op_none) {
            opcode->decode_src2 = group_opcode->decode_src2;
        }
    }
    if (flags & INSN_BYTEOP) {
        ctxt->operand_size = 1;
    }
    if (flags & INSN_STACK) {
        if (ctxt->operand_size == 4 && ctxt->mode == EM_MODE_PROT64)
            ctxt->operand_size = 8;
    }

    /* Some unimplemented opcodes have an all-zero em_opcode_t */
    if ((opcode->flags & INSN_NOTIMPL) || !opcode->handler) {
        return EM_ERROR;
    }

    if (ctxt->rep) {
        if (!(opcode->flags & INSN_STRING)) {
            /* Instruction does not support any REP* prefix */
            // TODO: Should throw #UD
            return EM_ERROR;
        }
        // NOTE: Specifications at Intel SDM Vol. 2B: 2.1.1 Instruction Prefixes
        // mention that REP prefixes are encoded via 0xF3 (PREFIX_REPE), however
        // they remain ambiguous as to whether 0xF2 (PREFIX_REPNE) is also valid.
        // Hardware tests reveal both values act as REP in REP-only instructions,
        // consequently we mimic this undocumented hardware behavior by skipping
        // the following check:
        // assert((opcode->flags & INSN_REPX) || (ctxt->rep != PREFIX_REPNE));
    }

    return decode_operands(ctxt);
}

em_status_t EMCALL em_emulate_insn(struct em_context_t *ctxt)
{
    const struct em_opcode_t *opcode = &ctxt->opcode;
    em_status_t rc;
    ctxt->finished = false;

restart:
    // TODO: Permissions, exceptions, etc.
    if (ctxt->rep) {
        if (gpr_read(ctxt, REG_RCX, ctxt->address_size) == 0) {
            goto done;
        }
    }

    // Input operands
    if (!(opcode->flags & INSN_NOFLAGS)) {
        ctxt->rflags = ctxt->ops->read_rflags(ctxt->vcpu);
    }
    if (opcode->flags & INSN_BITOP &&
        ctxt->dst.type == OP_MEM && ctxt->src1.type == OP_REG) {
        rc = operand_read_bitop(ctxt);
        if (rc != EM_CONTINUE)
            goto exit;
    } else {
        if (!(opcode->flags & INSN_DST_NR)) {
            rc = operand_read(ctxt, &ctxt->dst);
            if (rc != EM_CONTINUE)
                goto exit;
        }
        rc = operand_read(ctxt, &ctxt->src1);
        if (rc != EM_CONTINUE)
            goto exit;
        rc = operand_read(ctxt, &ctxt->src2);
        if (rc != EM_CONTINUE)
            goto exit;
    }

    // Emulate instruction
    if (opcode->flags & INSN_FASTOP) {
        void *fast_handler;
        uint64_t eflags = ctxt->rflags & RFLAGS_MASK_OSZAPC;
        fast_handler = (void*)((uintptr_t)opcode->handler
            + FASTOP_OFFSET(ctxt->dst.size));
        fastop_dispatch(fast_handler,
            &ctxt->dst.value,
            &ctxt->src1.value,
            &ctxt->src2.value,
            &eflags);
        ctxt->rflags &= ~RFLAGS_MASK_OSZAPC;
        ctxt->rflags |= eflags & RFLAGS_MASK_OSZAPC;
    } else {
        em_status_t (*soft_handler)(em_context_t *);
        soft_handler = opcode->handler;
        rc = soft_handler(ctxt);
        if (rc != EM_CONTINUE)
            goto exit;
    }

    // Output operands
    if (!(opcode->flags & INSN_NOFLAGS)) {
        ctxt->ops->write_rflags(ctxt->vcpu, ctxt->rflags);
    }
    if (!(opcode->flags & INSN_DST_NW)) {
        rc = operand_write(ctxt, &ctxt->dst);
        if (rc != EM_CONTINUE)
            goto exit;
    }

    if (opcode->decode_dst == decode_op_di) {
        register_add(ctxt, REG_RDI, ctxt->operand_size *
            ((ctxt->rflags & RFLAGS_DF) ? -1LL : +1LL));
    }
    if (opcode->decode_src1 == decode_op_si) {
        register_add(ctxt, REG_RSI, ctxt->operand_size *
            ((ctxt->rflags & RFLAGS_DF) ? -1LL : +1LL));
    }
    if (ctxt->rep) {
        register_add(ctxt, REG_RCX, -1LL);

        if (opcode->flags & INSN_REPX) {
            if ((ctxt->rep == PREFIX_REPNE && (ctxt->rflags & RFLAGS_ZF)) ||
                (ctxt->rep == PREFIX_REPE && !(ctxt->rflags & RFLAGS_ZF))) {
                goto done;
            }
        }

        /* Continue to emulate the next iteration of this REP*-prefixed string
         * instruction by jumping to the beginning of em_emulate_insn(). This
         * avoids exiting to user space and decoding the entire instruction
         * again. Nevertheless, we still need to rerun the operand decoders:
         *
         * a) To recompute the effective address for *SI and *DI operands.
         * b) To reset the flags for each operand (OP_READ_FINISHED, etc.).
         */
        rc = decode_operands(ctxt);
        if (rc != EM_CONTINUE)
            goto exit;
        gpr_cache_flush(ctxt);
        goto restart;
    }

done:
    rc = EM_CONTINUE;
    ctxt->finished = true;
    ctxt->ops->advance_rip(ctxt->vcpu, ctxt->len);
    gpr_cache_flush(ctxt);

exit:
    gpr_cache_invalidate(ctxt);
    return rc;
}
