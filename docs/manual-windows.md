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
1. Open `platforms\windows\haxm.sln` in Visual Studio 2017.
1. Select either `Debug` or `Release` configuration.
   * The `Debug` configuration also signs the driver with a test certificate.
The `Release` configuration does not do that.
1. Select either `x64` or `Win32` platform.
1. Build solution.

**Option B (EWDK)**
1. `cd X:\path\to\EWDK\`
1. `LaunchBuildEnv.cmd`
1. `cd X:\path\to\haxm\`
1. `cd platforms\windows`
1. `X:\path\to\nuget.exe restore`
1. `msbuild haxm.sln /p:Configuration="Debug" /p:Platform="x64"`
   * Use `Release` instead of `Debug` to build an optimized driver that is
suitable for release. Note that the `Release` configuration does not sign the
driver with a test certificate.
   * Use `Win32` instead of `x64` to build a 32-bit driver that works on 32-bit
Windows.
   * Add `/t:rebuild` for a clean rebuild instead of an incremental build.

If successful, the driver binary (`IntelHaxm.sys`) will be generated in
`X:\path\to\haxm\platforms\windows\build\out\x64\{Debug,Release}\` (or
`X:\path\to\haxm\platforms\windows\build\out\Win32\{Debug,Release}\` if
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
      * Note: In some cases, this command is not enough to completely disable
Hyper-V on Windows 10. See below for a more reliable method.
   1. `bcdedit /set testsigning on`
   1. Reboot.
1. Install the test certificate:
   1. Copy `IntelHaxm.cer` from the build environment to the test environment
(if the two are not the same). This file is generated alongside `IntelHaxm.sys`
by the `Debug` build configuration.
   1. In the test environment, open an **elevated** Command Prompt and run
`certmgr /add X:\path\to\IntelHaxm.cer /s /r localMachine root`
1. Optionally, install [DebugView][debugview] to capture HAXM debug output.

#### Disabling Hyper-V on Windows 10
Certain advanced Windows 10 features, such as _Device Guard_ (in particular,
_Hypervisor-protected code integrity_ or HVCI) and _Credential Guard_, can
prevent Hyper-V from being completely disabled. In other words, when any of
these features are enabled, so is Hyper-V, even though Windows may report
otherwise.

The _Device Guard and Credential Guard hardware readiness tool_ released by
Microsoft can disable the said Windows 10 features along with Hyper-V:
1. Download the latest version of the tool from [here][dgreadiness-tool]. The
following steps assume version 3.6.
1. Unzip.
1. Open an **elevated** (i.e. _Run as administrator_) Command Prompt.
1. `@powershell -ExecutionPolicy RemoteSigned -Command "X:\path\to\dgreadiness_v3.6\DG_Readiness_Tool_v3.6.ps1 -Disable"`
1. Reboot.

### Loading and unloading the test driver
`HaxmLoader` is a small tool that can load and unload a test-signed driver
without using an INF file. You can download it from the
[Releases][github-haxm-releases] page, or building `HaxmLoader/HaxmLoader.sln`
yourself using Visual Studio or EWDK.

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

[visualstudio]: https://www.visualstudio.com/downloads/
[ewdk10]: https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/using-the-enterprise-wdk
[sdk10]: https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk
[wdk10]: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
[nuget]: https://www.nuget.org/downloads
[intel-ept-cpus]: https://ark.intel.com/Search/FeatureFilter?productType=processors&ExtendedPageTables=true
[windows-test-driver-install]: https://docs.microsoft.com/en-us/windows-hardware/drivers/install/installing-test-signed-driver-packages
[debugview]: https://docs.microsoft.com/en-us/sysinternals/downloads/debugview
[dgreadiness-tool]: https://www.microsoft.com/en-us/download/details.aspx?id=53337
[github-haxm-releases]: https://github.com/intel/haxm/releases
