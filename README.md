# Intel Hardware Accelerated Execution Manager (HAXM)
[HAXM][intel-haxm] is a hardware-assisted virtualization engine (hypervisor)
that uses [Intel Virtualization Technology][intel-vt] to speed up IA (x86/
x86\_64) emulation on a host machine running Windows or macOS.
It started as an [Android SDK][android-studio] component, but has recently
transformed itself into a general accelerator for [QEMU][qemu].

HAXM can be built as either a kernel-mode driver for Windows or a kernel
extension (_kext_) for macOS. If you are interested in building HAXM from the
source code, please read on. If you are just looking for the latest HAXM
release, you can get it [here][github-haxm-latest-release].

## Usage

Detailed instructions for building and testing HAXM can be found at:
* [Manual for macOS](docs/manual-macos.md)
* [Manual for Windows](docs/manual-windows.md)

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
[github-haxm-latest-release]: https://github.com/intel/haxm/releases/latest
[intel-security-email]: mailto:secure@intel.com
