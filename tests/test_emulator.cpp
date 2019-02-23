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

#include <vector>
#include <type_traits>

#include "gtest/gtest.h"
#include "keystone/keystone.h"

#include "../core/include/emulate.h"

/* Immediate types */
template <int N>
struct imm_t;
template <> struct imm_t<8>  { int8_t  val; };
template <> struct imm_t<16> { int16_t val; };
template <> struct imm_t<32> { int32_t val; };
template <> struct imm_t<64> { int32_t val; };

template <int N>
using imm = decltype(imm_t<N>::val);

/* Emulator operations */
struct test_cpu_t {
    uint64_t gpr[16];
    uint64_t rip;
    uint64_t flags;
    uint8_t mem[0x100];
};

uint64_t test_read_gpr(void* obj, uint32_t reg_index) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    if (reg_index >= 16) {
        throw std::exception("Register index OOB");
    }
    return vcpu->gpr[reg_index];
}

void test_write_gpr(void* obj, uint32_t reg_index, uint64_t value) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    if (reg_index >= 16) {
        throw std::exception("Register index OOB");
    }
    vcpu->gpr[reg_index] = value;
}

uint64_t test_read_rflags(void* obj) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    return vcpu->flags;
}

void test_write_rflags(void* obj, uint64_t value) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    vcpu->flags = value;
}

static uint64_t test_get_segment_base(void* obj, uint32_t segment) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    return 0ULL;
}

void test_advance_rip(void* obj, uint64_t len) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    vcpu->rip += len;
}

em_status_t test_read_memory(void* obj, uint64_t ea, uint64_t* value,
                             uint32_t size, uint32_t flags) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    if (ea + size >= 0x100) {
        return EM_ERROR;
    }
    switch (size) {
    case 1:
        *value = *(uint8_t*)(&vcpu->mem[ea]);
        break;
    case 2:
        *value = *(uint16_t*)(&vcpu->mem[ea]);
        break;
    case 4:
        *value = *(uint32_t*)(&vcpu->mem[ea]);
        break;
    case 8:
        *value = *(uint64_t*)(&vcpu->mem[ea]);
        break;
    default:
        return EM_ERROR;
    }
    return EM_CONTINUE;
}

em_status_t test_write_memory(void* obj, uint64_t ea, uint64_t* value,
                              uint32_t size, uint32_t flags) {
    test_cpu_t* vcpu = reinterpret_cast<test_cpu_t*>(obj);
    if (ea + size > 0x100) {
        return EM_ERROR;
    }
    switch (size) {
    case 1:
        *(uint8_t*)(&vcpu->mem[ea]) = (uint8_t)*value;
        break;
    case 2:
        *(uint16_t*)(&vcpu->mem[ea]) = (uint16_t)*value;
        break;
    case 4:
        *(uint32_t*)(&vcpu->mem[ea]) = (uint32_t)*value;
        break;
    case 8:
        *(uint64_t*)(&vcpu->mem[ea]) = (uint64_t)*value;
        break;
    default:
        return EM_ERROR;
    }
    return EM_CONTINUE;
}

/* Test class */
class EmulatorTest : public testing::Test {
private:
    ks_engine* ks_x86_16;
    ks_engine* ks_x86_32;
    ks_engine* ks_x86_64;
    test_cpu_t vcpu;
    em_context_t em_ctxt;
    em_vcpu_ops_t em_ops;

protected:
    virtual void SetUp() {
        // Initialize assembler
        ASSERT_EQ(KS_ERR_OK, ks_open(KS_ARCH_X86, KS_MODE_16, &ks_x86_16));
        ASSERT_EQ(KS_ERR_OK, ks_open(KS_ARCH_X86, KS_MODE_32, &ks_x86_32));
        ASSERT_EQ(KS_ERR_OK, ks_open(KS_ARCH_X86, KS_MODE_64, &ks_x86_64));
        ASSERT_EQ(KS_ERR_OK, ks_option(ks_x86_16, KS_OPT_SYNTAX, KS_OPT_SYNTAX_INTEL));
        ASSERT_EQ(KS_ERR_OK, ks_option(ks_x86_32, KS_OPT_SYNTAX, KS_OPT_SYNTAX_INTEL));
        ASSERT_EQ(KS_ERR_OK, ks_option(ks_x86_64, KS_OPT_SYNTAX, KS_OPT_SYNTAX_INTEL));

        // Initialize emulator
        em_ops.read_gpr = test_read_gpr;
        em_ops.write_gpr = test_write_gpr;
        em_ops.read_rflags = test_read_rflags;
        em_ops.write_rflags = test_write_rflags;
        em_ops.get_segment_base = test_get_segment_base;
        em_ops.advance_rip = test_advance_rip;
        em_ops.read_memory = test_read_memory;
        em_ops.write_memory = test_write_memory;
        em_ctxt.ops = &em_ops;
        em_ctxt.vcpu = &vcpu;
        em_ctxt.rip = 0;
    }

    void assemble_decode(em_mode_t mode,
                         const char* insn,
                         uint64_t rip,
                         size_t* size,
                         size_t* count,
                         em_status_t* decode_status) {
        ks_engine* ks = nullptr;
        uint8_t* code;

        switch (mode) {
        case EM_MODE_PROT16:
            ks = ks_x86_16;
            break;
        case EM_MODE_PROT32:
            ks = ks_x86_32;
            break;
        case EM_MODE_PROT64:
            ks = ks_x86_64;
            break;
        default:
            GTEST_FAIL();
        }
        ASSERT_EQ(KS_ERR_OK, ks_asm(ks, insn, 0, &code, size, count));
        EXPECT_NE(*size, 0);
        EXPECT_NE(*count, 0);

        em_ctxt.rip = rip;
        em_ctxt.mode = mode;
        *decode_status = em_decode_insn(&em_ctxt, code);
        // code == em_ctxt->insn should never be used after em_decode_insn()
        ks_free(code);
    }

    void run_mode(em_mode_t mode,
                  const char* insn,
                  const test_cpu_t& vcpu_original,
                  const test_cpu_t& vcpu_expected) {
        size_t count;
        size_t size;
        em_status_t ret = EM_ERROR;

        vcpu = vcpu_original;
        assemble_decode(mode, insn, vcpu.rip, &size, &count, &ret);
        ASSERT_NE(ret, EM_ERROR)
            << "em_decode_insn failed on: " << insn << "\n";
        ret = em_emulate_insn(&em_ctxt);
        ASSERT_NE(ret, EM_ERROR)
            << "em_emulate_insn failed on: " << insn << "\n";

        // Verify results
        EXPECT_EQ(vcpu.rip, vcpu_original.rip + size)
            << "Instruction pointer mismatch on: " << insn << "\n";
        vcpu.rip = vcpu_expected.rip;
        verify(insn, vcpu, vcpu_expected);
    }

    void run_prot16(const char* insn,
                    const test_cpu_t& vcpu_original,
                    const test_cpu_t& vcpu_expected) {
        run_mode(EM_MODE_PROT16, insn, vcpu_original, vcpu_expected);
    }

    void run_prot32(const char* insn,
                    const test_cpu_t& vcpu_original,
                    const test_cpu_t& vcpu_expected) {
        run_mode(EM_MODE_PROT32, insn, vcpu_original, vcpu_expected);
    }

    void run_prot64(const char* insn,
                    const test_cpu_t& vcpu_original,
                    const test_cpu_t& vcpu_expected) {
        run_mode(EM_MODE_PROT64, insn, vcpu_original, vcpu_expected);
    }

    void run(const char* insn,
             const test_cpu_t& vcpu_original,
             const test_cpu_t& vcpu_expected) {
        // Default to x86-64 protected mode
        run_prot64(insn, vcpu_original, vcpu_expected);
    }

    void verify(const char* insn,
                const test_cpu_t& vcpu_obtained,
                const test_cpu_t& vcpu_expected) {
        // Helpers
#define PRINT_U64(value) \
        std::hex << std::uppercase << std::setfill('0') << std::setw(16) << value
#define VERIFY_GPR(reg) \
        if (vcpu_obtained.gpr[reg] != vcpu_expected.gpr[reg]) \
            std::cerr << "Register mismatch on: " << insn << "\n" \
                << "vcpu_obtained.gpr[" #reg "]: 0x" << PRINT_U64(vcpu_obtained.gpr[reg]) << "\n" \
                << "vcpu_expected.gpr[" #reg "]: 0x" << PRINT_U64(vcpu_expected.gpr[reg]) << "\n";

        // Verify GPRs
        VERIFY_GPR(REG_RAX);
        VERIFY_GPR(REG_RCX);
        VERIFY_GPR(REG_RDX);
        VERIFY_GPR(REG_RBX);
        VERIFY_GPR(REG_RSP);
        VERIFY_GPR(REG_RBP);
        VERIFY_GPR(REG_RSI);
        VERIFY_GPR(REG_RDI);
        VERIFY_GPR(REG_R8);
        VERIFY_GPR(REG_R9);
        VERIFY_GPR(REG_R10);
        VERIFY_GPR(REG_R11);
        VERIFY_GPR(REG_R12);
        VERIFY_GPR(REG_R13);
        VERIFY_GPR(REG_R14);
        VERIFY_GPR(REG_R15);

        // Verify RFLAGS
        if (vcpu_obtained.flags != vcpu_expected.flags)
            std::cerr << "Flags mismatch on: " << insn << "\n"
                << "vcpu_obtained.flags: 0x" << PRINT_U64(vcpu_obtained.flags) << "\n"
                << "vcpu_expected.flags: 0x" << PRINT_U64(vcpu_expected.flags) << "\n";

#undef PRINT_U64
#undef VERIFY_GPR

        // Ensure identical state
        ASSERT_EQ(memcmp(&vcpu, &vcpu_expected, sizeof(test_cpu_t)), 0);
    }

    /* Test cases */
    struct test_alu_2op_t {
        uint64_t in_dst;
        uint64_t in_src;
        uint64_t in_flags;
        uint64_t out_dst;
        uint64_t out_flags;
    };

    struct test_alu_3op_t {
        uint64_t in_dst;
        uint64_t in_src1;
        uint64_t in_src2;
        uint64_t in_flags;
        uint64_t out_dst;
        uint64_t out_flags;
    };

    /* Test helpers */
    template <int N>
    const char* gpr(int reg) {
        size_t index = 0;
        switch (N) {
        case 8:   index = 0; break;
        case 16:  index = 1; break;
        case 32:  index = 2; break;
        case 64:  index = 3; break;
        default:
            break;
        }
        std::vector<char*> regs;
        switch (reg) {
        case REG_RAX:  regs = { "al",   "ax",   "eax",  "rax" }; break;
        case REG_RCX:  regs = { "cl",   "cx",   "ecx",  "rcx" }; break;
        case REG_RDX:  regs = { "dl",   "dx",   "edx",  "rdx" }; break;
        case REG_RBX:  regs = { "bl",   "bx",   "ebx",  "rbx" }; break;
        case REG_RSP:  regs = { "spl",  "sp",   "esp",  "rsp" }; break;
        case REG_RBP:  regs = { "bpl",  "bp",   "ebp",  "rbp" }; break;
        case REG_RSI:  regs = { "sil",  "si",   "esi",  "rsi" }; break;
        case REG_RDI:  regs = { "dil",  "di",   "edi",  "rdi" }; break;
        case REG_R8:   regs = { "r8b",  "r8w",  "r8d",  "r8"  }; break;
        case REG_R9:   regs = { "r9b",  "r9w",  "r9d",  "r9"  }; break;
        case REG_R10:  regs = { "r10b", "r10w", "r10d", "r10" }; break;
        case REG_R11:  regs = { "r11b", "r11w", "r11d", "r11" }; break;
        case REG_R12:  regs = { "r12b", "r12w", "r12d", "r12" }; break;
        case REG_R13:  regs = { "r13b", "r13w", "r13d", "r13" }; break;
        case REG_R14:  regs = { "r14b", "r14w", "r14d", "r14" }; break;
        case REG_R15:  regs = { "r15b", "r15w", "r15d", "r15" }; break;
        }
        return regs[index];
    }

    template <int N>
    const char* mem() {
        size_t index = 0;
        switch (N) {
        case 8:   return "byte";
        case 16:  return "word";
        case 32:  return "dword";
        case 64:  return "qword";
        default:
            return "";
        }
    }

    void test_insn_unimpl(const char* insn) {
        size_t size;
        size_t count;
        em_status_t ret = EM_CONTINUE;

        assemble_decode(EM_MODE_PROT64, insn, 0, &size, &count, &ret);
        // Decoding should fail
        EXPECT_LT(ret, 0);
    }

    template <int N, int M=N>
    void test_insn_rN_rM(const char* insn_name,
                         const std::vector<test_alu_2op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;
        snprintf(insn, sizeof(insn), "%s %s, %s", insn_name,
                 gpr<N>(REG_RDX), gpr<M>(REG_RCX));

        // Run tests
        for (const auto& test : tests) {
            vcpu_original = {};
            vcpu_original.gpr[REG_RDX] = test.in_dst;
            vcpu_original.gpr[REG_RCX] = test.in_src;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            vcpu_expected.gpr[REG_RDX] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N, int M=N>
    void test_insn_rN_iM(const char* insn_name,
                         const std::vector<test_alu_2op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;

        // Run tests
        for (const auto& test : tests) {
            snprintf(insn, sizeof(insn), "%s %s, %d", insn_name,
                     gpr<N>(REG_RAX), (imm<M>)test.in_src);
            vcpu_original = {};
            vcpu_original.gpr[REG_RAX] = test.in_dst;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            vcpu_expected.gpr[REG_RAX] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N, int M=N>
    void test_insn_mN_iM(const char* insn_name,
                         const std::vector<test_alu_2op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;

        // Run tests
        for (const auto& test : tests) {
            snprintf(insn, sizeof(insn), "%s %s ptr [edx + 2*ecx + 0x10], %d",
                     insn_name, mem<N>(), (imm<M>)test.in_src);
            vcpu_original = {};
            vcpu_original.gpr[REG_RDX] = 0x20;
            vcpu_original.gpr[REG_RCX] = 0x08;
            (uint64_t&)vcpu_original.mem[0x40] = test.in_dst;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            (uint64_t&)vcpu_expected.mem[0x40] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N, int M=N>
    void test_insn_rN_mM(const char* insn_name,
                         const std::vector<test_alu_2op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;
        snprintf(insn, sizeof(insn), "%s %s, %s ptr [edx + 2*ecx + 0x10]",
                 insn_name, gpr<N>(REG_RAX), mem<M>());

        // Run tests
        for (const auto& test : tests) {
            vcpu_original = {};
            vcpu_original.gpr[REG_RDX] = 0x20;
            vcpu_original.gpr[REG_RCX] = 0x08;
            (uint64_t&)vcpu_original.mem[0x40] = test.in_src;
            vcpu_original.gpr[REG_RAX] = test.in_dst;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            vcpu_expected.gpr[REG_RAX] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N, int M=N>
    void test_insn_mN_rM(const char* insn_name,
                         const std::vector<test_alu_2op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;
        snprintf(insn, sizeof(insn), "%s %s ptr [edx + 2*ecx + 0x10], %s",
                 insn_name, mem<N>(), gpr<M>(REG_RAX));

        // Run tests
        for (const auto& test : tests) {
            vcpu_original = {};
            vcpu_original.gpr[REG_RDX] = 0x20;
            vcpu_original.gpr[REG_RCX] = 0x08;
            vcpu_original.gpr[REG_RAX] = test.in_src;
            (uint64_t&)vcpu_original.mem[0x40] = test.in_dst;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            (uint64_t&)vcpu_expected.mem[0x40] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N>
    void test_insn_rN_rN_rN(const char* insn_name,
                         const std::vector<test_alu_3op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;
        snprintf(insn, sizeof(insn), "%s %s, %s, %s", insn_name,
                 gpr<N>(REG_RAX), gpr<N>(REG_RCX), gpr<N>(REG_RDX));

        // Run tests
        for (const auto& test : tests) {
            vcpu_original = {};
            vcpu_original.gpr[REG_RAX] = test.in_dst;
            vcpu_original.gpr[REG_RCX] = test.in_src1;
            vcpu_original.gpr[REG_RDX] = test.in_src2;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            vcpu_expected.gpr[REG_RAX] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N>
    void test_insn_rN_mN_rN(const char* insn_name,
                            const std::vector<test_alu_3op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;
        snprintf(insn, sizeof(insn), "%s %s, %s ptr [edx + 2*ecx + 0x10], %s",
                 insn_name, gpr<N>(REG_RAX), mem<N>(), gpr<N>(REG_RBX));

        // Run tests
        for (const auto& test : tests) {
            vcpu_original = {};
            vcpu_original.gpr[REG_RAX] = test.in_dst;
            vcpu_original.gpr[REG_RDX] = 0x20;
            vcpu_original.gpr[REG_RCX] = 0x08;
            (uint64_t&)vcpu_original.mem[0x40] = test.in_src1;
            vcpu_original.gpr[REG_RBX] = test.in_src2;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            vcpu_expected.gpr[REG_RAX] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N>
    void test_insn_rN_rN_mN(const char* insn_name,
                            const std::vector<test_alu_3op_t>& tests) {
        char insn[256];
        test_cpu_t vcpu_original;
        test_cpu_t vcpu_expected;
        snprintf(insn, sizeof(insn), "%s %s, %s, %s ptr [edx + 2*ecx + 0x10]",
                 insn_name, gpr<N>(REG_RAX), gpr<N>(REG_RBX), mem<N>());

        // Run tests
        for (const auto& test : tests) {
            vcpu_original = {};
            vcpu_original.gpr[REG_RAX] = test.in_dst;
            vcpu_original.gpr[REG_RBX] = test.in_src1;
            vcpu_original.gpr[REG_RDX] = 0x20;
            vcpu_original.gpr[REG_RCX] = 0x08;
            (uint64_t&)vcpu_original.mem[0x40] = test.in_src2;
            vcpu_original.flags = test.in_flags;
            vcpu_expected = vcpu_original;
            vcpu_expected.gpr[REG_RAX] = test.out_dst;
            vcpu_expected.flags = test.out_flags;
            run(insn, vcpu_original, vcpu_expected);
        }
    }

    template <int N>
    void test_alu_2op(const char* insn_name,
                      const std::vector<test_alu_2op_t>& tests) {
        if (N == 64 && sizeof(void*) < 8) {
            return;
        }
        test_insn_rN_rM<N>(insn_name, tests);
        test_insn_rN_iM<N>(insn_name, tests);
        test_insn_mN_iM<N>(insn_name, tests);
        test_insn_rN_mM<N>(insn_name, tests);
        test_insn_mN_rM<N>(insn_name, tests);
    }

    template <int N>
    void test_bt(const char* insn_name,
                 const std::vector<test_alu_2op_t>& tests) {
        if (N == 64 && sizeof(void*) < 8) {
            return;
        }
        // Only bit-test variants that implicitly use bit offset modulo width.
        test_insn_rN_rM<N>(insn_name, tests);
        test_insn_rN_iM<N,8>(insn_name, tests);
        test_insn_mN_iM<N,8>(insn_name, tests);
    }

    template <int N>
    void test_test(const std::vector<test_alu_2op_t>& tests) {
        if (N == 64 && sizeof(void*) < 8) {
            return;
        }
        // TEST is similar to AND, except that:
        // a) The destination operand is read-only.
        // b) Not all operand combinations are possible/implemented.
        test_insn_mN_iM<N>("test", tests);
        test_insn_mN_rM<N>("test", tests);
    }
};

TEST_F(EmulatorTest, insn_unimpl_primary) {
    // Opcode 0x87 (XCHG r/mN, rN) is unimplemented
    test_insn_unimpl("xchg dword ptr [edx + 2*ecx + 0x10], esi");
}

TEST_F(EmulatorTest, insn_unimpl_secondary) {
    // Opcode 0xF7 /4 (MUL r/mN) is unimplemented
    test_insn_unimpl("mul dword ptr [edx + 2*ecx + 0x10]");
}

TEST_F(EmulatorTest, insn_add) {
    test_alu_2op<8>("add", {
        { 0x04, 0x05, 0,
          0x09, RFLAGS_PF },
        { 0xFF, 0x01, 0,
          0x00, RFLAGS_CF | RFLAGS_PF | RFLAGS_AF | RFLAGS_ZF },
    });
    test_alu_2op<16>("add", {
        { 0x0001, 0x1234, 0,
          0x1235, RFLAGS_PF },
        { 0xF000, 0x1001, 0,
          0x0001, RFLAGS_CF },
    });
    test_alu_2op<32>("add", {
        { 0x55555555, 0x11111111, RFLAGS_CF,
          0x66666666, RFLAGS_PF },
        { 0xF0000000, 0x10000000, 0,
          0x00000000, RFLAGS_CF | RFLAGS_PF | RFLAGS_ZF },
    });
    test_alu_2op<64>("add", {
        { 0x2'000000FFULL, 0x0'01010002ULL, RFLAGS_CF,
          0x2'01010101ULL, RFLAGS_AF },
        { 0x0'F0000000ULL, 0x0'10000001ULL, 0,
          0x1'00000001ULL, 0 },
    });
}

TEST_F(EmulatorTest, insn_and) {
    test_alu_2op<8>("and", {
        { 0x55, 0xF0, RFLAGS_CF,
          0x50, RFLAGS_PF },
        { 0xF0, 0x0F, RFLAGS_OF,
          0x00, RFLAGS_PF | RFLAGS_ZF },
    });
    test_alu_2op<16>("and", {
        { 0x0001, 0xF00F, RFLAGS_CF | RFLAGS_OF,
          0x0001, 0 },
        { 0xFF00, 0xF0F0, 0,
          0xF000, RFLAGS_PF | RFLAGS_SF },
    });
    test_alu_2op<32>("and", {
        { 0xFFFF0001, 0xFFFF0001, 0,
          0xFFFF0001, RFLAGS_SF },
    });
    test_alu_2op<64>("and", {
        { 0x0000FFFF'F0F0FFFFULL, 0xFFFFFFFF'FFFF0000ULL, 0,
          0x0000FFFF'F0F00000ULL, RFLAGS_PF },
    });
}

TEST_F(EmulatorTest, insn_andn) {
    const std::vector<test_alu_3op_t> tests32 = {
        { 0x00000000'00000000, 0xF0F0F0F0'F0F0F0F0, 0xFF00FF00'FF00FF00, 0,
          0x00000000'0F000F00, 0 },
        { 0x00000000'00000000, 0xF0F0F0F0'0000FFFF, 0x00000000'FFFF0000, 0,
          0x00000000'FFFF0000, RFLAGS_SF }};
    test_insn_rN_rN_rN<32>("andn", tests32);
    test_insn_rN_rN_mN<32>("andn", tests32);

    const std::vector<test_alu_3op_t> tests64 = {
        { 0x00000000'00000000, 0xF0F0F0F0'F0F0F0F0, 0xFF00FF00'FF00FF00, 0,
          0x0F000F00'0F000F00, 0 },
        { 0xFFFF0000'FFFF0000, 0xF0F0F0F0'0000FFFF, 0xF0F0F0F0'0000FFFF, 0,
          0x00000000'00000000, RFLAGS_ZF }};
    test_insn_rN_rN_rN<64>("andn", tests64);
    test_insn_rN_rN_mN<64>("andn", tests64);
}

TEST_F(EmulatorTest, insn_bextr) {
    const std::vector<test_alu_3op_t> tests32 = {
        { 0x00000000'00000000, 0x00000000'12345678, 0x00000000'00000C04, 0,
          0x00000000'00000567, 0 },
        { 0x00000000'F0F0F0F0, 0xFFFFFFFF'00112233, 0x00000000'0000101C, 0,
          0x00000000'00000000, RFLAGS_ZF }};
    test_insn_rN_rN_rN<32>("bextr", tests32);
    test_insn_rN_mN_rN<32>("bextr", tests32);

    const std::vector<test_alu_3op_t> tests64 = {
        { 0xFFFFFFFF'FFFFFFFF, 0x11223344'55667788, 0x00000000'00002814, 0,
          0x00000012'23344556, 0 },
        { 0x00000000'F0F0F0F0, 0xFFFFFFFF'00112233, 0x00000000'0000101C, 0,
          0x00000000'0000FFF0, 0 }};
    test_insn_rN_rN_rN<64>("bextr", tests64);
    test_insn_rN_mN_rN<64>("bextr", tests64);
}

TEST_F(EmulatorTest, insn_bt) {
    test_bt<16>("bt", {
        { 0xFFFE, 0x00, RFLAGS_CF,
          0xFFFE, 0 },
        { 0x0200, 0x09, 0,
          0x0200, RFLAGS_CF },
        });
    test_bt<32>("bt", {
        { 0xFF7FFFFF, 0x17, 0,
          0xFF7FFFFF, 0 },
        { 0xFFFF0000, 0x3F, RFLAGS_CF,
          0xFFFF0000, RFLAGS_CF },
        });
    test_bt<64>("bt", {
        { 0x00000000'FFFFFFFFULL, 0x20, RFLAGS_CF,
          0x00000000'FFFFFFFFULL, 0 },
        { 0x80000000'00000000ULL, 0xFF, 0,
          0x80000000'00000000ULL, RFLAGS_CF },
        });

    // Variant `bt mN,rN`: Modifies the EA based on the bit offset.
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;
    vcpu_original = {};
    vcpu_original.gpr[REG_RCX] = 0ULL;
    (uint64_t&)vcpu_original.mem[0x00] = 0x00020000'00000000;
    (uint64_t&)vcpu_original.mem[0x08] = 0x00000000'00000000;
    (uint64_t&)vcpu_original.mem[0x10] = 0xFFFFFFFF'FFFFFFFE;
    
    vcpu_original.gpr[REG_RAX] = -15LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags |= RFLAGS_CF;
    run("bt [ecx + 0x08], eax", vcpu_original, vcpu_expected);
    vcpu_original.gpr[REG_RAX] = +64LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags &= ~RFLAGS_CF;
    run("bt [rcx + 0x08], rax", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_btc) {
    test_bt<16>("btc", {
        { 0xFFFE, 0x00, RFLAGS_CF,
          0xFFFF, 0 },
        { 0x0200, 0x09, 0,
          0x0000, RFLAGS_CF },
        });
    test_bt<32>("btc", {
        { 0xFF7FFFFF, 0x17, 0,
          0xFFFFFFFF, 0 },
        { 0xFFFF0000, 0x3F, RFLAGS_CF,
          0x7FFF0000, RFLAGS_CF },
        });
    test_bt<64>("btc", {
        { 0x00000000'FFFFFFFFULL, 0x20, RFLAGS_CF,
          0x00000001'FFFFFFFFULL, 0 },
        { 0x80000000'00000000ULL, 0xFF, 0,
          0x00000000'00000000ULL, RFLAGS_CF },
        });

    // Variant `btc mN,rN`: Modifies the EA based on the bit offset.
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;
    vcpu_original = {};
    vcpu_original.gpr[REG_RCX] = 0ULL;
    (uint64_t&)vcpu_original.mem[0x00] = 0x00020000'00000000;
    (uint64_t&)vcpu_original.mem[0x08] = 0x00000000'00000000;
    (uint64_t&)vcpu_original.mem[0x10] = 0xFFFFFFFF'FFFFFFFE;

    vcpu_original.gpr[REG_RAX] = -15LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags |= RFLAGS_CF;
    (uint64_t&)vcpu_expected.mem[0x00] = 0x00000000'00000000;
    run("btc [ecx + 0x08], eax", vcpu_original, vcpu_expected);
    vcpu_original.gpr[REG_RAX] = +64LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags &= ~RFLAGS_CF;
    (uint64_t&)vcpu_expected.mem[0x10] = 0xFFFFFFFF'FFFFFFFF;
    run("btc [rcx + 0x08], rax", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_btr) {
    test_bt<16>("btr", {
        { 0xFFFE, 0x00, RFLAGS_CF,
          0xFFFE, 0 },
        { 0x0200, 0x09, 0,
          0x0000, RFLAGS_CF },
        });
    test_bt<32>("btr", {
        { 0xFF7FFFFF, 0x17, 0,
          0xFF7FFFFF, 0 },
        { 0xFFFF0000, 0x3F, RFLAGS_CF,
          0x7FFF0000, RFLAGS_CF },
        });
    test_bt<64>("btr", {
        { 0x00000000'FFFFFFFFULL, 0x20, RFLAGS_CF,
          0x00000000'FFFFFFFFULL, 0 },
        { 0x80000000'00000000ULL, 0xFF, 0,
          0x00000000'00000000ULL, RFLAGS_CF },
        });

    // Variant `btr mN,rN`: Modifies the EA based on the bit offset.
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;
    vcpu_original = {};
    vcpu_original.gpr[REG_RCX] = 0ULL;
    (uint64_t&)vcpu_original.mem[0x00] = 0x00020000'00000000;
    (uint64_t&)vcpu_original.mem[0x08] = 0x00000000'00000000;
    (uint64_t&)vcpu_original.mem[0x10] = 0xFFFFFFFF'FFFFFFFE;

    vcpu_original.gpr[REG_RAX] = -15LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags |= RFLAGS_CF;
    (uint64_t&)vcpu_expected.mem[0x00] = 0x00000000'00000000;
    run("btr [ecx + 0x08], eax", vcpu_original, vcpu_expected);
    vcpu_original.gpr[REG_RAX] = +64LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags &= ~RFLAGS_CF;
    run("btr [rcx + 0x08], rax", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_bts) {
    test_bt<16>("bts", {
        { 0xFFFE, 0x00, RFLAGS_CF,
          0xFFFF, 0 },
        { 0x0200, 0x09, 0,
          0x0200, RFLAGS_CF },
        });
    test_bt<32>("bts", {
        { 0xFF7FFFFF, 0x17, 0,
          0xFFFFFFFF, 0 },
        { 0xFFFF0000, 0x3F, RFLAGS_CF,
          0xFFFF0000, RFLAGS_CF },
        });
    test_bt<64>("bts", {
        { 0x00000000'FFFFFFFFULL, 0x20, RFLAGS_CF,
          0x00000001'FFFFFFFFULL, 0 },
        { 0x80000000'00000000ULL, 0xFF, 0,
          0x80000000'00000000ULL, RFLAGS_CF },
        });

    // Variant `bts mN,rN`: Modifies the EA based on the bit offset.
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;
    vcpu_original = {};
    vcpu_original.gpr[REG_RCX] = 0ULL;
    (uint64_t&)vcpu_original.mem[0x00] = 0x00020000'00000000;
    (uint64_t&)vcpu_original.mem[0x08] = 0x00000000'00000000;
    (uint64_t&)vcpu_original.mem[0x10] = 0xFFFFFFFF'FFFFFFFE;

    vcpu_original.gpr[REG_RAX] = -15LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags |= RFLAGS_CF;
    run("bts [ecx + 0x08], eax", vcpu_original, vcpu_expected);
    vcpu_original.gpr[REG_RAX] = +64LL;
    vcpu_expected = vcpu_original;
    vcpu_expected.flags &= ~RFLAGS_CF;
    (uint64_t&)vcpu_expected.mem[0x10] = 0xFFFFFFFF'FFFFFFFF;
    run("bts [rcx + 0x08], rax", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_cmps) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    // Test: cmpsw, without-rep, with-df
    vcpu_original = {};
    vcpu_original.gpr[REG_RSI] = 0x10;
    vcpu_original.gpr[REG_RDI] = 0x50;
    vcpu_original.flags = RFLAGS_DF;
    (uint16_t&)vcpu_original.mem[0x10] = 0x1234;
    (uint16_t&)vcpu_original.mem[0x50] = 0x1234;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSI] -= 2;
    vcpu_expected.gpr[REG_RDI] -= 2;
    vcpu_expected.flags = RFLAGS_DF | RFLAGS_ZF | RFLAGS_PF;
    run("cmpsw", vcpu_original, vcpu_expected);

    // Test: cmpsw, with-rep, without-df
    vcpu_original = {};
    vcpu_original.gpr[REG_RSI] = 0x20;
    vcpu_original.gpr[REG_RDI] = 0x80;
    vcpu_original.gpr[REG_RCX] = 0x3;
    vcpu_original.mem[0x20] = 0x11;
    vcpu_original.mem[0x21] = 0x22;
    vcpu_original.mem[0x80] = 0x11;
    vcpu_original.mem[0x81] = 0x33;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSI] += 0x2;
    vcpu_expected.gpr[REG_RDI] += 0x2;
    vcpu_expected.gpr[REG_RCX] = 0x1;
    vcpu_expected.flags = RFLAGS_PF;
    run("repe cmpsb", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_mov) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    // Test: mov r8, r/m8
    vcpu_original = {};
    vcpu_original.gpr[REG_RDX] = 0x88;
    vcpu_original.mem[0x88] = 0x44;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RCX] = 0x4400;
    run("mov ch, [rdx]", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_movs) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    // Test: movsb, without-rep, without-df
    vcpu_original = {};
    vcpu_original.gpr[REG_RSI] = 0x10;
    vcpu_original.gpr[REG_RDI] = 0x50;
    vcpu_original.mem[0x10] = 0x77;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSI] += 1;
    vcpu_expected.gpr[REG_RDI] += 1;
    vcpu_expected.mem[0x50] = 0x77;
    run("movsb", vcpu_original, vcpu_expected);

    // Test: movsw, with-rep, with-df
    vcpu_original = {};
    vcpu_original.gpr[REG_RSI] = 0x24;
    vcpu_original.gpr[REG_RDI] = 0x64;
    vcpu_original.gpr[REG_RCX] = 0x3;
    vcpu_original.flags = RFLAGS_DF;
    (uint16_t&)vcpu_original.mem[0x24] = 0x1122;
    (uint16_t&)vcpu_original.mem[0x22] = 0x3344;
    (uint16_t&)vcpu_original.mem[0x20] = 0x5566;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSI] -= 0x6;
    vcpu_expected.gpr[REG_RDI] -= 0x6;
    vcpu_expected.gpr[REG_RCX] = 0x0;
    (uint16_t&)vcpu_expected.mem[0x64] = 0x1122;
    (uint16_t&)vcpu_expected.mem[0x62] = 0x3344;
    (uint16_t&)vcpu_expected.mem[0x60] = 0x5566;
    run("rep movsw", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_movzx) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    vcpu_original = {};
    vcpu_original.gpr[REG_RAX] = 0xFFFFFFFF'FFFFFFFF;
    vcpu_original.gpr[REG_RCX] = 0xF0F1F2F3'F4F5F6F7;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RAX] = 0x00000000'0000F6F7;
    run("movzx eax, cx", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_movsx) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    vcpu_original = {};
    vcpu_original.gpr[REG_RAX] = 0xFFFFFFFF'FFFFFFFF;
    vcpu_original.gpr[REG_RCX] = 0xF0F1F2F3'F4F5F6F7;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RAX] = 0x00000000'FFFFF6F7;
    run("movsx eax, cx", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_or) {
    test_alu_2op<8>("or", {
        { 0x55, 0xF0, RFLAGS_CF,
          0xF5, RFLAGS_PF | RFLAGS_SF },
        { 0xF0, 0x0E, RFLAGS_OF,
          0xFE, RFLAGS_SF },
    });
}

TEST_F(EmulatorTest, insn_pop) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    // Test: pop, prot32, 16-bit, to-mem
    vcpu_original = {};
    vcpu_original.gpr[REG_RAX] = 0x20;
    vcpu_original.gpr[REG_RSP] = 0x40;
    (uint16_t&)vcpu_original.mem[0x40] = 0x1234;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSP] += 2;
    (uint16_t&)vcpu_expected.mem[0x20] = 0x1234;
    run_prot32("pop word ptr [eax]", vcpu_original, vcpu_expected);

    // Test: pop, prot16, 16-bit, to-reg
    vcpu_original = {};
    vcpu_original.gpr[REG_RSP] = 0x80;
    (uint16_t&)vcpu_original.mem[0x80] = 0x1234;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RAX] = 0x1234;
    vcpu_expected.gpr[REG_RSP] += 2;
    run_prot16("pop ax", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_push) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    // Test: push, prot64, 16-bit, from-mem
    vcpu_original = {};
    vcpu_original.gpr[REG_RAX] = 0x20;
    vcpu_original.gpr[REG_RSP] = 0x42;
    (uint16_t&)vcpu_original.mem[0x20] = 0x1234;
    (uint16_t&)vcpu_original.mem[0x22] = 0x4578;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSP] -= 2;
    (uint16_t&)vcpu_expected.mem[0x40] = 0x1234;
    run_prot64("push word ptr [rax]", vcpu_original, vcpu_expected);

    // Test: push, prot32, 32-bit, from-reg
    vcpu_original = {};
    vcpu_original.gpr[REG_RAX] = 0x1122334455667788ULL;
    vcpu_original.gpr[REG_RSP] = 0x80;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RSP] -= 4;
    (uint32_t&)vcpu_expected.mem[0x7C] = 0x55667788;
    run_prot32("push eax", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_stos) {
    test_cpu_t vcpu_original;
    test_cpu_t vcpu_expected;

    // Test: stosb, without-rep, with-df
    vcpu_original = {};
    vcpu_original.gpr[REG_RAX] = 0x77;
    vcpu_original.gpr[REG_RDI] = 0x20;
    vcpu_original.flags = RFLAGS_DF;
    vcpu_expected = vcpu_original;
    vcpu_expected.gpr[REG_RDI] -= 1;
    vcpu_expected.mem[0x20] = 0x77;
    run("stosb", vcpu_original, vcpu_expected);
}

TEST_F(EmulatorTest, insn_test) {
    test_test<8>({
        { 0x55, 0xF0, RFLAGS_CF,
          0x55, RFLAGS_PF },
        { 0xF0, 0x0F, RFLAGS_OF,
          0xF0, RFLAGS_PF | RFLAGS_ZF },
        });
    test_test<16>({
        { 0x0001, 0xF00F, RFLAGS_CF | RFLAGS_OF,
          0x0001, 0 },
        { 0xFF00, 0xF0F0, 0,
          0xFF00, RFLAGS_PF | RFLAGS_SF },
        });
    test_test<32>({
        { 0xFFFF0001, 0xFFFF0001, 0,
          0xFFFF0001, RFLAGS_SF },
        });
    test_test<64>({
        { 0x0000FFFF'F0F0FFFFULL, 0xFFFF0000'0F0F0000ULL, 0,
          0x0000FFFF'F0F0FFFFULL, RFLAGS_PF | RFLAGS_ZF },
        });
}

TEST_F(EmulatorTest, insn_xor) {
    test_alu_2op<8>("xor", {
        { 0x0F, 0xF0, RFLAGS_CF,
          0xFF, RFLAGS_PF| RFLAGS_SF },
        { 0xFF, 0xFF, RFLAGS_OF,
          0x00, RFLAGS_PF | RFLAGS_ZF },
    });
}
