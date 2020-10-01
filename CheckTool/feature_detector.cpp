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

#include <cassert>
#include <iostream>
#include <string>

#include "common.h"
#include "cpuid.h"
#include "feature_detector.h"
#if defined(_WIN32) || defined(_WIN64)
#include "os_windows.h"
#elif defined(__APPLE__)
#include "os_darwin.h"
#endif

namespace haxm {
namespace check_util {

FeatureDetector::FeatureDetector() {
    os_ = new OsImpl();
}

FeatureDetector::~FeatureDetector() {
    delete os_;
}

CheckResult FeatureDetector::CheckCpuVendor(std::string* vendor) const {
    std::string temp;
    temp = cpuid_.GetCpuVendor();

    if (vendor != nullptr) {
        *vendor = temp;
    }

    return temp == "GenuineIntel" ? kPass : kFail;
}

CheckResult FeatureDetector::CheckLongModeSupported() const {
    return cpuid_.IsLongModeSupported() ? kPass : kFail;
}

CheckResult FeatureDetector::CheckVmxSupported() const {
    return cpuid_.IsVmxSupported() ? kPass : kFail;
}

CheckResult FeatureDetector::CheckVmxEnabled() const {
    return os_->CheckVmxEnabled();
}

CheckResult FeatureDetector::CheckEptSupported() const {
    return os_->CheckEptSupported();
}

CheckResult FeatureDetector::CheckNxSupported() const {
    return cpuid_.IsNxSupported() ? kPass : kFail;
}

CheckResult FeatureDetector::CheckNxEnabled() const {
    return os_->CheckNxEnabled();
}

CheckResult FeatureDetector::CheckHyperVDisabled() const {
    return os_->CheckHyperVDisabled();
}

CheckResult FeatureDetector::CheckOsVersion(OsVersion* os_ver) const {
    return os_->CheckVersion(os_ver);
}

CheckResult FeatureDetector::CheckOsArchitecture(
        OsArchitecture* os_arch) const {
    return os_->CheckArchitecture(os_arch);
}

CheckResult FeatureDetector::CheckGuestOccupied(uint32_t* occupied_count)
        const {
    return os_->CheckGuestOccupied(occupied_count);
}

std::string FeatureDetector::ToString(CheckResult res) {
    switch (res) {
        case kUnknown:
            return "Unknown";
        case kPass:
            return "Yes";
        case kFail:
            return "No";
        case kNotApplicable:
            return "Not Applicable";
        case kError:
            return "An error occurred";
        default:
            assert(false);
            return nullptr;
    }
}

std::string FeatureDetector::ToString(OsArchitecture os_arch) {
    switch (os_arch) {
        case kX86:
            return "x86";
        case kX86_64:
            return "x86_64";
        default:
            return "Unrecognized";
    }
}

std::string FeatureDetector::ToString(OsType os_type) {
    switch (os_type) {
        case kWindows:
            return "Windows";
        case kDarwin:
            return "macOS";
        default:
            return "Unrecognized";
    }
}

CheckResult FeatureDetector::Detect() const {
    CheckResult res[11];

    res[0] = CheckCpuVendor();
    res[1] = CheckLongModeSupported();
    res[2] = CheckNxSupported();
    res[3] = CheckNxEnabled();
    res[4] = CheckOsVersion();
    res[5] = CheckOsArchitecture();
    res[6] = CheckGuestOccupied();
    res[7] = CheckHyperVDisabled();
    res[8] = CheckVmxSupported();
    res[9] = CheckVmxEnabled();
    res[10] = CheckEptSupported();

    int check_num = 11;
    // When Hyper-V is enabled, it will affect the checking results of VMX
    // supported, VMX enabled and EPT supported, so only the first 8 items are
    // checked. When Hyper-V is disabled, all items are checked.
    if (res[7] == kFail) {
        check_num = 8;
    }

    int detector[5] = {};

    for (int i = 0; i < check_num; ++i) {
        ++detector[static_cast<int>(res[i])];
    }

    if (detector[static_cast<int>(kError)] > 0) {
        return kError;
    }

    if (detector[static_cast<int>(kUnknown)] > 0) {
        return kUnknown;
    }

    if (detector[static_cast<int>(kFail)] > 0) {
        return kFail;
    }

    return kPass;
}

void FeatureDetector::Print() const {
    CheckResult res;
    std::string item;
    const int kCol = 20;

    std::string vendor;
    res = CheckCpuVendor(&vendor);
    item = "CPU vendor";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << vendor << std::endl;

    res = CheckLongModeSupported();
    item = "Intel64 supported";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    res = CheckVmxSupported();
    item = "VMX supported";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    res = CheckVmxEnabled();
    item = "VMX enabled";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    res = CheckEptSupported();
    item = "EPT supported";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    res = CheckNxSupported();
    item = "NX supported";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    res = CheckNxEnabled();
    item = "NX enabled";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    res = CheckHyperVDisabled();
    item = "Hyper-V disabled";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res != kFail ? '*' : '-') << "  " << ToString(res)
              << std::endl;

    OsVersion os_ver;
    res = CheckOsVersion(&os_ver);
    item = "OS version";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(os_ver.type)
              << " " << os_ver.major << "." << os_ver.minor << "."
              << os_ver.build_number << std::endl;

    OsArchitecture os_arch;
    res = CheckOsArchitecture(&os_arch);
    item = "OS architecture";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(os_arch)
              << std::endl;

    uint32_t occupied_count;
    res = CheckGuestOccupied(&occupied_count);
    item = "Guest unoccupied";
    std::cout << item << std::string(kCol - item.size(), ' ')
              << (res == kPass ? '*' : '-') << "  " << ToString(res) << ". "
              << occupied_count << " guest(s)" << std::endl;
}

}  // namespace check_util
}  // namespace haxm
