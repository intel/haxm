## Building for Linux

**Disclaimer: Support for Linux is experimental.**

### Prerequisites
* Linux headers
* NASM 2.11 or later

### Build steps
1. `cd platforms/linux/`
1. `make`

## Testing on Linux
### System requirements
Note that these are requirements for the _test_ environment, which does not
have to be the same as the _build_ environment.

1. Hardware requirements are the same as those for Windows.
1. Linux 4.x or later.

### Loading and unloading the kernel module
To load the kernel module:
1. Make sure no other HAXM kernel module is loaded. If the output of
`lsmod | grep haxm` is not empty, you must unload the existing HAXM module
first: `sudo make uninstall`.
1. Run `sudo make install`.

To unload the kernel module:
1. Run `sudo make uninstall`.

Additionally, if you want to use HAXM as a non-privileged user,
you can enter the following command to make the current user
part of the *haxm* group (requires logging out and back in!):

```bash
sudo adduser `id -un` haxm
```

Note that in recent Linux distributions, you might get a `sign-file` error
since it kernel Makefiles will attempt to sign the kernel module with
`certs/signing_key.pem`. Unless driver signature enforcement has been enabled,
you can safely ignore this warning. Alternatively, you can follow
[this guide][linux-module-signing] to self-sign your drivers.

### Viewing logs
On Linux, HAXM debug output goes to the system log database, and can be
retrieved via `dmesg` (if supported, the `-w` flag will update the output).
You might filter these entries via: `dmesg | grep haxm`.

[linux-module-signing]: https://www.kernel.org/doc/html/v4.18/admin-guide/module-signing.html
