# Intel Hardware Accelerated Execution Manager (HAXM)
[HAXM][intel-haxm] is a hardware-assisted virtualization engine (hypervisor)
that uses [Intel Virtualization Technology][intel-vt] to speed up IA (x86/
x86\_64) emulation on a host machine running Windows or macOS.
It started as an [Android SDK][android-studio] component, but has recently
transformed itself into a general accelerator for [QEMU][qemu].

HAXM can be built as either a kernel-mode driver for Windows or a kernel
extension (_kext_) for macOS.

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
`X:\path\to\haxm\obj\out\Win7\x64\` (or `X:\path\to\haxm\obj\out\Win7\x86\` if
`Platform="Win32"`), and will be able to run on Windows 7 and later.

## Testing on Windows
### System requirements
Note that these are requirements for the _test_ environment, which does not
have to be the same as the _build_ environment.

1. An Intel CPU that supports Intel VT-x with _Extended Page Tables_ (EPT).
   * [Here][intel-ept-cpus] is a list of CPUs that meet this requirement. As a
rule of thumb, if you have an Intel Core i3, i5, i7 or i9 (any generation), you
are good to go.
   * EPT is an advanced feature of Intel VT-x. CPUs that support EPT also
support _Unrestricted Guest_ (UG), which is another advanced feature of VT-x.
It may still be possible to run HAXM on very old (pre-2010) CPUs, e.g.
Intel Core 2 Duo, which implement an earlier version of VT-x that does not
include either EPT or UG. However, the legacy code that enables HAXM to work in
non-EPT and non-UG modes may be removed soon.
1. Windows 7 or later; both 32-bit and 64-bit Windows are supported.
   * Running HAXM in a nested virtualization setup, where Windows itself runs as
a guest OS on another hypervisor, may be possible, but this use case is not well
tested.

### One-time setup
The following steps prepare the test environment for installing a test-signed
`IntelHaxm.sys`, i.e. one that is built using the `Debug` configuration. For
more details, please read [this article][windows-test-driver-install].

1. Disable Hyper-V and enable _Test Mode_:
   1. Open an **elevated** (i.e. _Run as administrator_) Command Prompt.
   1. `bcdedit /set hypervisorlaunchtype off`
   1. `bcdedit /set testsigning off`
   1. Reboot.
1. Install the test certificate:
   1. Copy `IntelHaxm.cer` from the build environment to the test environment
(if the two are not the same). This file is generated alongside `IntelHaxm.sys`
by the `Debug` build configuration.
   1. In the test environment, open an **elevated** Command Prompt and run
`certmgr.exe /add X:\path\to\IntelHaxm.cer /s /r localMachine root`
1. Optionally, install [DebugView][debugview] to capture HAXM debug output.

### Loading and unloading the test driver
`HaxmLoader` is a small tool that can load and unload a test-signed driver
without using an INF file. You can download it from [Releases](./releases),
or building `HaxmLoader/HaxmLoader.sln` yourself using Visual Studio or EWDK.

Basically, kernel-mode drivers like HAXM are managed by Windows Service Control
Manager as services. Each such service has a unique name, a corresponding driver
file, and a state. For example, when the HAXM installer installs the
release-signed driver to `C:\Windows\System32\drivers\IntelHaxm.sys`, it also
creates a service for it. This service is named `intelhaxm` and is started at
boot time. `HaxmLoader` works in a similar manner: when loading a test driver,
it creates a temporary service and starts it; when unloading the test driver, it
stops and then deletes the service. 

To load the test driver:
1. Open an **elevated** Command Prompt.
1. Make sure no other HAXM driver is loaded.
   1. If `sc query intelhaxm` shows the `intelhaxm` service as `RUNNING`, you
must stop it first: `sc stop intelhaxm`
   1. Otherwise, unload the previously loaded test driver, if any:
`HaxmLoader.exe -u`
1. Load the test driver: `HaxmLoader.exe -i X:\path\to\IntelHaxm.sys`
   * Note that `HaxmLoader` can load a driver from any folder, so there is no
need to copy the test driver to `C:\Windows\System32\drivers\` first.

To unload the test driver:
1. Open an **elevated** Command Prompt.
1. `HaxmLoader.exe -u`
1. Optionally, you may want to restore the original, release-signed driver
(i.e. `C:\Windows\System32\drivers\IntelHaxm.sys`): `sc start intelhaxm`

### Capturing driver logs
1. Launch DebugView (`Dbgview.exe`) as administrator.
1. In the _Capture_ menu, select everything except _Log Boot_. DebugView will
now start capturing debug output from all kernel-mode drivers.
1. In order to filter out non-HAXM logs, go to _Edit_ > _Filter/Highlight..._,
enter `hax*` for _Include_, and click on _OK_.

## Building for macOS
### Prerequisites
* Xcode 7.2.1 or later
* OS X 10.10 SDK (archived [here][osx-sdks])
  * Use `xcodebuild -showsdks` to list installed SDKs.
  * It is also possible to build HAXM using a newer version of macOS SDK.
However, using an older SDK ensures that the generated kext is compatible with
older versions of macOS.
* NASM 2.11 or later
  * Install to `/usr/local/bin/` using Homebrew: `brew install nasm`
  * Note that Apple NASM (`/usr/bin/nasm`) cannot be used.

### Build steps
1. `cd /path/to/haxm/`
1. `cd darwin/hax_driver/com_intel_hax/`
1. `xcodebuild -configuration Debug`
   * Use `-sdk` to override the default macOS SDK version (10.10), e.g.
`-sdk macosx10.12` (to select SDK 10.12) or even `-sdk macosx` (to select the
latest SDK installed).
   * Use `Release` instead of `Debug` to build an optimized kext that is
suitable for release.

If successful, the kext (`intelhaxm.kext/`) will be generated in
`/path/to/haxm/darwin/hax_driver/com_intel_hax/build/Debug/`.

## Testing on macOS
### System requirements
Note that these are requirements for the _test_ environment, which does not
have to be the same as the _build_ environment.

1. Hardware requirements are the same as those for Windows.
1. OS X 10.10 or later.

### Loading and unloading the test kext
The `intelhaxm.kext` generated by the `Debug` and `Release` build configurations
is not signed. Unless you can sign it using a special kind of Apple Developer ID
Certificate, you must configure your test Mac to allow unsigned kexts to load:
* For OS X 10.10, the solution is to add the `kext-dev-mode=1` _boot-arg_. More
details can be found [here][macos-kext-dev-mode].
* For macOS 10.11 and later, the solution is to turn off System Integrity
Protection (SIP). More details can be found [here][macos-sip-disable]. Note that
this means every time you want to test an unsigned kext, you must reboot your
Mac into recovery mode.

To load the test kext:
1. Make sure no other HAXM kext is loaded. If the output of
`kextstat | grep intelhaxm` is not empty, you must unload the existing HAXM kext
first: `sudo kextunload -b com.intel.kext.intelhaxm`
1. `sudo chown -R root:wheel /path/to/intelhaxm.kext`
1. `sudo chmod -R 755 /path/to/intelhaxm.kext`
1. `sudo kextload /path/to/intelhaxm.kext`
   * Note that `kextload` can load a kext from any folder, so there is no need
to copy the test kext to `/Library/Extensions/` first.

To unload the test kext:
1. `sudo kextunload /path/to/intelhaxm.kext`
1. Optionally, you may want to restore the original HAXM kext, which is usually
the signed one installed to `/Library/Extensions/`:
`sudo kextload /Library/Extensions/intelhaxm.kext`

### Viewing kext logs
On macOS, HAXM debug output goes to the system log database, and can be
retrieved at almost any time.

* On OS X 10.10, HAXM log messages are written immediately to
`/var/log/system.log`. You can monitor this file for real-time updates using
_Console.app_ or the `syslog -w` command.
* On macOS 10.11 or later, HAXM log messages are no longer written to
`/var/log/system.log`, and there is no good way to capture them in real time.
However, you can still retrieve them at a later time using one of the following
methods:
  1. `log show --predicate 'sender == "intelhaxm"' --style syslog --last 1h`,
which is complex but very flexible. In this example, `--last 1h` indicates the
past hour, and can be replaced with other queries.
  1. `sudo dmesg | grep hax`, which is simple, but does not show the timestamp
of each message.

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
[intel-ept-cpus]: https://ark.intel.com/Search/FeatureFilter?productType=processors&ExtendedPageTables=true
[windows-test-driver-install]: https://docs.microsoft.com/en-us/windows-hardware/drivers/install/installing-test-signed-driver-packages
[debugview]: https://docs.microsoft.com/en-us/sysinternals/downloads/debugview
[osx-sdks]: https://github.com/phracker/MacOSX-SDKs
[macos-kext-dev-mode]: https://developer.apple.com/library/archive/documentation/Security/Conceptual/System_Integrity_Protection_Guide/KernelExtensions/KernelExtensions.html
[macos-sip-disable]: https://developer.apple.com/library/archive/documentation/Security/Conceptual/System_Integrity_Protection_Guide/ConfiguringSystemIntegrityProtection/ConfiguringSystemIntegrityProtection.html
[intel-security-email]: mailto:secure@intel.com
