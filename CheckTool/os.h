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

#ifndef HAXM_CHECK_OS_H_
#define HAXM_CHECK_OS_H_

#include "common.h"

namespace haxm {
namespace check_util {

enum OsType {
    kWindows = 0,
    kDarwin,
    kOtherOs,
};

struct OsVersion {
    OsType type;
    int major;
    int minor;
    int build_number;
};

enum OsArchitecture {
    kX86 = 0,
    kX86_64,
    kOtherArch,
};

class Os {  // abstract class
public:
    virtual ~Os() {}
    virtual CheckResult CheckVmxEnabled() const = 0;
    virtual CheckResult CheckEptSupported() const = 0;
    virtual CheckResult CheckNxEnabled() const = 0;
    virtual CheckResult CheckHyperVDisabled() const = 0;
    virtual CheckResult CheckVersion(OsVersion* os_ver = nullptr) const = 0;
    virtual CheckResult CheckArchitecture(
            OsArchitecture* os_arch = nullptr) const = 0;
    virtual CheckResult CheckGuestOccupied(uint32_t* occupied_count = nullptr)
            const = 0;
};

}  // namespace check_util
}  // namespace haxm

#endif  // HAXM_CHECK_OS_H_
