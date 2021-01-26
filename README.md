# Intel Hardware Accelerated Execution Manager (HAXM)

<img src="Installer/res/haxm_logo.ico" height="176px" align="right">

HAXM is a cross-platform hardware-assisted virtualization engine (hypervisor),
widely used as an accelerator for [Android Emulator][android-studio] and
[QEMU][qemu]. It has always supported running on Windows and macOS, and has been
ported to other host operating systems as well, such as Linux and NetBSD.

HAXM runs as a kernel-mode driver on the host operating system, and provides a
KVM-like interface to user space, thereby enabling applications like QEMU to
utilize the hardware virtualization capabilities built into modern Intel CPUs,
namely [Intel Virtualization Technology][intel-vt].

## Downloads
The latest HAXM release for Windows and macOS hosts are available
[here][github-haxm-releases].

## Contributing
Detailed instructions for building and testing HAXM can be found at:
* [Manual for Linux](docs/manual-linux.md)
* [Manual for macOS](docs/manual-macos.md)
* [Manual for Windows](docs/manual-windows.md)

If you would like to contribute a patch to the code base, please also read
[these guidelines](CONTRIBUTING.md).

## Reporting an Issue
You are welcome to file a [GitHub issue][github-haxm-issues] if you discover a
general HAXM bug or have a feature request.

However, please do not use the GitHub issue tracker to report security
vulnerabilities. If you have information about a security issue or vulnerability
with HAXM, please send an email to [secure@intel.com][intel-security-email], and
use the PGP key located at https://www.intel.com/security to encrypt any
sensitive information.

## Code of Conduct
This project has adopted the Contributor Covenant, in the hope of building a
welcoming and inclusive community. All participants in the project should adhere
to this [code of conduct](CODE_OF_CONDUCT.md).

[intel-vt]: https://www.intel.com/content/www/us/en/virtualization/virtualization-technology/intel-virtualization-technology.html
[android-studio]: https://developer.android.com/studio/index.html
[qemu]: https://www.qemu.org/
[github-haxm-releases]: https://github.com/intel/haxm/releases
[github-haxm-issues]: https://github.com/intel/haxm/issues
[intel-security-email]: mailto:secure@intel.com
