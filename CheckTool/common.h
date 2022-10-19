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

#ifndef HAXM_CHECK_COMMON_H_
#define HAXM_CHECK_COMMON_H_

#include <cstdint>

namespace haxm {
namespace check_util {

#define APP_VERSION    "1.1.0"
#define APP_COPYRIGHT  "Copyright (C) 2020 Intel Corporation"

enum CheckResult {
    kUnknown = 0,
    kPass,
    kFail,
    kNotApplicable,  // e.g., CheckHypervDisabled() on macOS
    kError,
};

// Source:
// https://github.com/google/cpu_features/blob/master/include/internal/bit_utils.h
inline static bool IsBitSet(uint32_t reg, int bit) {
    return (reg >> bit) & 0x1;
}

CheckResult ParseArguments(int &argc, char* argv[]);
int Check();

}  // namespace check_util
}  // namespace haxm

#endif  // HAXM_CHECK_COMMON_H_
