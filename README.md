# Intel Hardware Accelerated Execution Manager (HAXM)
[HAXM][intel-haxm] is a hardware-assisted virtualization engine (hypervisor)
that uses [Intel Virtualization Technology][intel-vt] to speed up IA (x86/
x86\_64) emulation on a host machine running Windows or macOS.
It started as an [Android SDK][android-studio] component, but has recently
transformed itself into a general accelerator for [QEMU][qemu].

HAXM can be built as either a kernel-mode driver for Windows or a kernel
extension for macOS.

## Building for Windows
Prerequisites:
* [Enterprise WDK (EWDK) 10][ewdk10]

   Alternatively, install [all of the following][wdk10] instead of EWDK 10:
   * Visual Studio 2015
   * Windows Driver Kit (WDK) for Windows 10
   * Windows SDK for Windows 10

Build steps:
1. `cd X:\path\to\EWDK\`
1. `LaunchBuildEnv.cmd`
   * Or, if Visual Studio 2015 is installed, launch *Developer Command Prompt
for VS2015* from *Start* > *All apps* > *Visual Studio 2015* instead.
1. `cd X:\path\to\haxm\`
1. `msbuild HaxmDriver.sln /p:Configuration="Win7 Debug" /p:Platform="x64"`
   * The `Win7` configuration ensures the driver is compatible with Windows 7
and later.
   * The `Debug` configuration also signs the driver with a test certificate.
The `Release` configuration does not do that.
   * Use `Win32` instead of `x64` to build a 32-bit driver that works on 32-bit
Windows.
   * Add `/t:rebuild` for a clean rebuild instead of an incremental build.

If successful, the driver binary (`IntelHaxm.sys`) will be generated in
`X:\path\to\haxm\obj\out\win7\x64\` (or `X:\path\to\haxm\obj\out\win7\x86\` if
`Platform="Win32"`).

## Building for macOS
Prerequisites:
* Xcode 7.2.1 or later
* OS X 10.10 SDK (archived [here][osx-sdks])
* NASM 2.11 or later (`brew install nasm`)

Build steps:
1. `cd /path/to/haxm/`
1. `cd darwin/hax_driver/com_intel_hax/`
1. `xcodebuild -config Release`
   * Use `-sdk` to override the default macOS SDK version (10.10), e.g.
`-sdk macosx10.12`.

If successful, the kext (`intelhaxm.kext/`) will be generated in
`/path/to/haxm/darwin/hax_driver/com_intel_hax/build/Release/`.

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
[ewdk10]: https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/installing-the-enterprise-wdk
[wdk10]: https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit
[osx-sdks]: https://github.com/phracker/MacOSX-SDKs
[intel-security-email]: mailto:secure@intel.com
