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

#include <cstdint>
#include <cwchar>
#include <windows.h>
#include <VersionHelpers.h>

#include "cpuid.h"
#include "os_windows.h"

namespace haxm {
namespace check_util {

#define HAX_DEVICE_TYPE 0x4000
#define HAX_IOCTL_CAPABILITY \
        CTL_CODE(HAX_DEVICE_TYPE, 0x910, METHOD_BUFFERED, FILE_ANY_ACCESS)

// The definition of hax_capability refers to the source code of HAXM project.
// <haxm>/include/hax_interface.h
struct hax_capabilityinfo {
    int16_t wstatus;
    uint16_t winfo;
    uint32_t win_refcount;
    uint64_t mem_quota;
};

OsImpl::OsImpl() {}

CheckResult OsImpl::CheckVmxEnabled() const {
    bool vmx_enabled_res = IsVirtFeaturePresent(PF_VIRT_FIRMWARE_ENABLED);

    return vmx_enabled_res ? kPass : kFail;
}

CheckResult OsImpl::CheckEptSupported() const {
    bool ept_supported_res =
            IsVirtFeaturePresent(PF_SECOND_LEVEL_ADDRESS_TRANSLATION);

    return ept_supported_res ? kPass : kFail;
}

CheckResult OsImpl::CheckNxEnabled() const {
    bool nx_enabled_res = IsVirtFeaturePresent(PF_NX_ENABLED);

    return nx_enabled_res ? kPass : kFail;
}

CheckResult OsImpl::CheckHyperVDisabled() const {
    // Check whether hypervisor is present or not
    if (!cpuid_.IsHypervisorPresent()) {
        // Hypervisor is not present
        return kPass;
    }

    // Hypervisor is present
    CpuidResult leaf = cpuid_.Run(0x40000000);
    uint32_t max_leaf4_ = leaf.eax;

    // TO check if hypervisor vendor is "MicrosoftHv"
    // Leaf = 0x40000000 EBX = "Micr" ECX = "osof" EDX = "t HV"
    // According to: Hypervisor Top Level Functional Specification v5.0C page 5
    char vendor_temp[13] = {0};
    *reinterpret_cast<uint32_t*>(vendor_temp) = leaf.ebx;
    *reinterpret_cast<uint32_t*>(vendor_temp + 4) = leaf.ecx;
    *reinterpret_cast<uint32_t*>(vendor_temp + 8) = leaf.edx;
    vendor_temp[12] = '\0';

    std::string vendor_str = vendor_temp;

    if (vendor_str != "Microsoft Hv") {
        return kPass;
    }

    if (max_leaf4_ < 0x40000003) {
        return kPass;
    }

    // CPUID function 0x40000003 will return Hypervisor feature
    // identification
    leaf = cpuid_.Run(0x40000003);

    // EBX includes information of flags specified at partition creation.
    // Partition privileges must be identical for all virtual processors in
    // a partition and "CreatePartitions" can make any other hypercall that
    // is restricted to operating on children.
    // So EBX[0] whose description is CreatePartitions can indicate Hyper-V
    // enabled.
    // According to: Requirements for Implementing the Microsoft Hypervisor
    // Interface
    // According to: Hypervisor Top Level Functional Specification v5.0C page
    // 34 Also refer to:
    // http://jason-x64.itwn.intel.com:8080/sdk/xref/emu-master-dev/external/qemu/android/android-emu/android/emulation/CpuAccelerator.cpp#1054
    return IsBitSet(leaf.ebx, 0) ? kFail : kPass;
}

typedef NTSTATUS (WINAPI *RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);

CheckResult OsImpl::CheckVersion(OsVersion* os_ver) const {
    CheckResult res;
    HMODULE module;

    // Refer to the table:
    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/operating-system-version
    res = IsWindows7SP1OrGreater() ? kPass : kFail;
    if (os_ver == nullptr)
        return res;

    module = ::GetModuleHandle(TEXT("ntdll.dll"));

    if (module == NULL)
        return kError;

    RtlGetVersionFunc RtlGetVersion =
            (RtlGetVersionFunc)::GetProcAddress(module, "RtlGetVersion");

    if (RtlGetVersion == NULL)
        return kError;

    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);

    RtlGetVersion(&rovi);

    os_ver->type         = kWindows;
    os_ver->major        = rovi.dwMajorVersion;
    os_ver->minor        = rovi.dwMinorVersion;
    os_ver->build_number = rovi.dwBuildNumber;

    return res;
}

CheckResult OsImpl::CheckArchitecture(OsArchitecture* os_arch) const {
    SYSTEM_INFO si;
    OsArchitecture os_arch_temp;

    GetNativeSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        os_arch_temp = kX86_64;
    } else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        os_arch_temp = kX86;
    } else {
        os_arch_temp = kOtherArch;
    }

    if (os_arch != nullptr) {
        *os_arch = os_arch_temp;
    }

    return os_arch_temp == kOtherArch ? kFail : kPass;
}

CheckResult OsImpl::CheckGuestOccupied(uint32_t* occupied_count) const {
    int ret;
    HANDLE hax_handle;
    DWORD size = 0;
    hax_capabilityinfo ci = {0, 0, 0, 0};

    if (occupied_count != nullptr) {
        *occupied_count = 0;
    }

    hax_handle = CreateFile("\\\\.\\HAX", GENERIC_READ | GENERIC_WRITE, 0,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hax_handle == INVALID_HANDLE_VALUE) {
        return kPass;
    }

    ret = DeviceIoControl(hax_handle, HAX_IOCTL_CAPABILITY, NULL, 0, &ci,
                          sizeof(ci), &size, NULL);

    CloseHandle(hax_handle);

    if (!ret) {
        return kError;
    }

    if (occupied_count != nullptr) {
        // Subtracting one here is in order not to count the occupancy of this
        // program itself.
        *occupied_count = ci.win_refcount - 1;
    }

    if (ci.win_refcount > 1) {
        return kFail;
    }

    return kPass;
}

bool OsImpl::IsVirtFeaturePresent(uint32_t virt_feature) const {
    OsVersion os_ver;

    CheckVersion(&os_ver);
    if (os_ver.major == 6 && os_ver.minor == 1) {
        // The following processor features of IsProcessorFeaturePresent are
        // not supported by Windows 7.
        // An alternative command line tool Coreinfo from Microsoft can be
        // referred currently.
        // https://docs.microsoft.com/en-us/sysinternals/downloads/coreinfo
        if (virt_feature == PF_VIRT_FIRMWARE_ENABLED ||
            virt_feature == PF_SECOND_LEVEL_ADDRESS_TRANSLATION)
            return true;
    }

    return IsProcessorFeaturePresent(virt_feature);
}

}  // namespace check_util
}  // namespace haxm
