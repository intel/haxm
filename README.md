# Intel Hardware Accelerated Execution Manager (HAXM)
[HAXM][intel-haxm] is a hardware-assisted virtualization engine (hypervisor)
that uses [Intel Virtualization Technology][intel-vt] to speed up IA (x86/
x86\_64) emulation on a host machine running Windows or macOS.
It started as an [Android SDK][android-studio] component, but has recently
transformed itself into a general accelerator for [QEMU][qemu].

HAXM can be built as either a kernel-mode driver for Windows or a kernel
extension for macOS.

## Building for Windows
### Prerequisites

**Option A (Visual Studio)**
* [Visual Studio 2017][visualstudio]
  * Install at least the following components:
_Universal Windows Platform development_, _Desktop development with C++_.
* [Windows SDK for Windows 10][sdk10]
* [Windows Driver Kit (WDK) for Windows 10][wdk10]

Note that the version/build number of Windows SDK must match that of WDK.
In particular, the Windows 10 SDK installed by Visual Studio 2017 (version 1709,
build 10.0.16299 as of this writing) may not be the latest version. If you want
to use the latest WDK (version 1803 as of this writing), you may need to
download and install the latest Windows 10 SDK (version 1803, build 10.0.17134
as of this writing).

**Option B (EWDK)**
* [Enterprise WDK (EWDK) 10][ewdk10] with Visual Studio Build Tools 15.6
  * Install the downloaded ISO image by mounting it or extracting it to an empty
folder.
* [NuGet CLI tool][nuget] (`nuget.exe`) version 4.x or later

### Build steps
**Option A (Visual Studio)**
1. Open `HaxmDriver.sln` in Visual Studio 2017.
1. Select either `Debug` or `Release` configuration.
   * The `Debug` configuration also signs the driver with a test certificate.
The `Release` configuration does not do that.
1. Select either `x64` or `Win32` platform.
1. Build solution.

**Option B (EWDK)**
1. `cd X:\path\to\EWDK\`
1. `LaunchBuildEnv.cmd`
1. `cd X:\path\to\haxm\`
1. `X:\path\to\nuget.exe restore`
1. `msbuild HaxmDriver.sln /p:Configuration="Debug" /p:Platform="x64"`
   * Use `Release` instead of `Debug` to build an optimized driver that is
suitable for release. Note that the `Release` configuration does not sign the
driver with a test certificate.
   * Use `Win32` instead of `x64` to build a 32-bit driver that works on 32-bit
Windows.
   * Add `/t:rebuild` for a clean rebuild instead of an incremental build.

If successful, the driver binary (`IntelHaxm.sys`) will be generated in
`X:\path\to\haxm\obj\out\win7\x64\` (or `X:\path\to\haxm\obj\out\win7\x86\` if
`Platform="Win32"`), and will be able to run on Windows 7 and later.

## Building for macOS
### Prerequisites
* Xcode 7.2.1 or later
* OS X 10.10 SDK (archived [here][osx-sdks])
* NASM 2.11 or later
  * Install to `/usr/local/bin/` using Homebrew: `brew install nasm`
  * Note that Apple NASM (`/usr/bin/nasm`) cannot be used.

### Build steps
1. `cd /path/to/haxm/`
1. `cd darwin/hax_driver/com_intel_hax/`
1. `xcodebuild -configuration Debug`
   * Use `-sdk` to override the default macOS SDK version (10.10), e.g.
`-sdk macosx10.12`.
   * Use `Release` instead of `Debug` to build an optimized KEXT that is
suitable for release.

If successful, the kext (`intelhaxm.kext/`) will be generated in
`/path/to/haxm/darwin/hax_driver/com_intel_hax/build/Debug/`.

## Reporting an Issue
You are welcome to file a GitHub issue if you discover a general HAXM bug or
have a feature request.

However, please do not use the GitHub issue tracker to report security
vulnerabilities. If you have information about a security issue or vulnerability
with HAXM, please send an email to [secure@intel.com][intel-security-email], and
use the PGP key located at https://www.intel.com/security to encrypt any
sensitive information.

[intel-haxm]: https://software.intel.com/en-us/android/articles/intel-hardware-accelerated-execution-manager
[intel-vt]: https://www.intel.com/content/www/us/en/virtualization/virtualization-technology/intel-virtualization-technology.html
[android-studio]: https://developer.android.com/studio/index.html
[qemu]: https://www.qemu.org/
[visualstudio]: https://www.visualstudio.com/downloads/
[ewdk10]: https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/using-the-enterprise-wdk
[sdk10]: https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk
[wdk10]: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
[nuget]: https://www.nuget.org/downloads
[osx-sdks]: https://github.com/phracker/MacOSX-SDKs
[intel-security-email]: mailto:secure@intel.com
