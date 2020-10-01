/*
 * Copyright (c) 2020 Intel Corporation
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

#include "cpuid.h"

// To check the current compilers
// https://github.com/google/cpu_features/blob/7806502271dfe79b417f636839dd9de41a0be16f/include/cpu_features_macros.h#L83
#if defined(__GNUC__) || defined(__clang__)  // GCC or Clang
#include <cpuid.h>
#elif defined(_MSC_VER)  // MSVC
#include <intrin.h>
#else  // !defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER_)
#error "Unsupported compiler, only support either GCC, Clang or MSVC."
#endif  // defined(__GNUC__) || defined(__clang__)

#include "common.h"

namespace haxm {
namespace check_util {

Cpuid::Cpuid() {
    max_leaf0_ = Run(0).eax;
    max_leaf8_ = Run(0x80000000).eax;
}

// To wrap the CPUID instruction
CpuidResult Cpuid::Run(uint32_t eax) {
    CpuidResult leaf;
#if defined(__GNUC__) || defined(__clang__)  // GCC or Clang
    __cpuid(eax, leaf.eax, leaf.ebx, leaf.ecx, leaf.edx);

#elif defined(_MSC_VER)  // MSVC
    int data[4];
    __cpuid(data, eax);
    leaf.eax = data[0];
    leaf.ebx = data[1];
    leaf.ecx = data[2];
    leaf.edx = data[3];
#else  // !defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)
#error "Unsupported compiler, only support either GCC, Clang or MSVC."
#endif  // defined(__GNUC__) || defined(__clang__)
    return leaf;
}

std::string Cpuid::GetCpuVendor() const {
    CpuidResult leaf = Run(0);

    char vendor_temp[13] = {0};
    *reinterpret_cast<uint32_t*>(vendor_temp) = leaf.ebx;
    *reinterpret_cast<uint32_t*>(vendor_temp + 4) = leaf.edx;
    *reinterpret_cast<uint32_t*>(vendor_temp + 8) = leaf.ecx;
    vendor_temp[12] = '\0';

    std::string vendor = vendor_temp;

    return vendor;
}

bool Cpuid::IsVmxSupported() const {
    if (max_leaf0_ < 1) {
        return false;
    }
    CpuidResult leaf = Run(1);

    return IsBitSet(leaf.ecx, 5);
}

bool Cpuid::IsNxSupported() const {
    if (max_leaf8_ < 0x80000001) {
        return false;
    }
    CpuidResult leaf = Run(0x80000001);

    return IsBitSet(leaf.edx, 20);
}

bool Cpuid::IsLongModeSupported() const {
    if (max_leaf8_ < 0x80000001) {
        return false;
    }
    CpuidResult leaf = Run(0x80000001);

    return IsBitSet(leaf.edx, 29);
}

bool Cpuid::IsHypervisorPresent() const {
    if (max_leaf0_ < 1) {
        return false;
    }
    CpuidResult leaf = Run(1);

    return IsBitSet(leaf.ecx, 31);
}

}  // namespace check_util
}  // namespace haxm
