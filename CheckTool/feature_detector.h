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

#ifndef HAXM_CHECK_FEATURE_DETECTOR_H_
#define HAXM_CHECK_FEATURE_DETECTOR_H_

#include <string>

#include "common.h"
#include "cpuid.h"
#include "os.h"

namespace haxm {
namespace check_util {

class FeatureDetector {
public:
    FeatureDetector();
    ~FeatureDetector();
    CheckResult Detect() const;
    void Print() const;

private:
    CheckResult CheckCpuVendor(std::string* vendor = nullptr) const;
    CheckResult CheckLongModeSupported() const;  // Long Mode = Intel64
    CheckResult CheckVmxSupported() const;
    CheckResult CheckVmxEnabled() const;
    CheckResult CheckEptSupported() const;
    CheckResult CheckNxSupported() const;
    CheckResult CheckNxEnabled() const;
    CheckResult CheckHyperVDisabled() const;
    CheckResult CheckOsVersion(OsVersion* os_ver = nullptr) const;
    CheckResult CheckOsArchitecture(OsArchitecture* os_arch = nullptr) const;
    CheckResult CheckGuestOccupied(uint32_t* occupied_count = nullptr) const;
    static std::string ToString(CheckResult res);
    static std::string ToString(OsType os_type);
    static std::string ToString(OsArchitecture os_arch);
    Cpuid cpuid_;
    Os* os_;
};

}  // namespace check_util
}  // namespace haxm

#endif  // HAXM_CHECK_FEATURE_DETECTOR_H_
