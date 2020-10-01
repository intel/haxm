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

#include <cstdio>
#include <iostream>

#include "os_darwin.h"

namespace haxm {
namespace check_util {

OsImpl::OsImpl() {}

CheckResult OsImpl::CheckHvfSupported() const {
    static CheckResult hvf_supported = kNotApplicable;

    if (hvf_supported != kNotApplicable) {
        return hvf_supported;
    }

    FILE* hvf_pipe;
    char hvf_buffer[256];

    hvf_pipe = popen("sysctl kern.hv_support", "r");

    if (hvf_pipe == NULL) {
        return kError;
    }

    if (fgets(hvf_buffer, sizeof(hvf_buffer), hvf_pipe) == NULL) {
        return kError;
    }

    pclose(hvf_pipe);

    std::string hvf_status(hvf_buffer);
    hvf_status = hvf_status[hvf_status.size() - 2];

    hvf_supported = hvf_status == "1" ? kPass : kUnknown;

    return hvf_supported;
}

CheckResult OsImpl::CheckVmxEnabled() const {
    return CheckHvfSupported();
}

CheckResult OsImpl::CheckEptSupported() const {
    return CheckHvfSupported();
}

CheckResult OsImpl::CheckNxEnabled() const {
    return CheckHvfSupported();
}

CheckResult OsImpl::CheckHyperVDisabled() const {
    return kNotApplicable;
}

CheckResult OsImpl::CheckVersion(OsVersion* os_ver) const {
    OsVersion temp_os_ver = {kDarwin, 0, 0, 0};
    FILE* os_ver_pipe;
    char os_ver_buffer[256];

    os_ver_pipe = popen("sw_vers -productVersion", "r");

    if (os_ver_pipe == NULL) {
        return kError;
    }

    if (fgets(os_ver_buffer, sizeof(os_ver_buffer), os_ver_pipe) == NULL) {
        return kError;
    }

    const char* os_ver_info = static_cast<const char*>(os_ver_buffer);

    std::sscanf(os_ver_info, "%d.%d.%d", &temp_os_ver.major, &temp_os_ver.minor,
                &temp_os_ver.build_number);

    if (os_ver != nullptr) {
        *os_ver = temp_os_ver;
    }

    if (temp_os_ver.major < 10) {
        return kFail;
    }

    if (temp_os_ver.major == 10 && temp_os_ver.minor < 10) {
        return kFail;
    }

    return kPass;
}

CheckResult OsImpl::CheckArchitecture(OsArchitecture* os_arch) const {
    if (os_arch != nullptr) {
        *os_arch = kX86_64;
    }

    return kPass;
}

CheckResult OsImpl::CheckGuestOccupied(uint32_t* occupied_count) const {
    if (occupied_count != nullptr) {
        *occupied_count = 0;
    }

    return kPass;
}

}  // namespace check_util
}  // namespace haxm
