# Check Tool for Intel Hardware Accelerated Execution Manager

It is required that the host operating system to meet the environmental
requirements to install HAXM. These requirements include Intel CPU verdor,
enabling VMX, disabling Hyper-V, etc. Only when all the requirements are met,
HAXM can be installed and executed properly. These wiki pages (installation
instructions on [Windows][install-on-windows] and [macOS][install-on-macos])
describe the configuration methods of HAXM installation prerequisites.

This utility is a command line tool for system checking for HAXM. It is used to
help user to check the status of each condition in the current system
environment, so as to determine whether the hardware configuration meets the
requirements or which system settings need to be changed. This software
currently supports running on Windows and macOS.

## Downloads

The latest release of HAXM **Check Tool** for Windows and macOS hosts are
available [here][checktool-release].

## Windows

### Usage

1. `cd X:\path\to\CheckTool`
1. `checktool.exe --verbose`

The output will be as below.

    CPU vendor          *  GenuineIntel
    Intel64 supported   *  Yes
    VMX supported       *  Yes
    VMX enabled         *  Yes
    EPT supported       *  Yes
    NX supported        *  Yes
    NX enabled          *  Yes
    Hyper-V disabled    -  No
    OS version          *  Windows 10.0.18363
    OS architecture     *  x86_64
    Guest unoccupied    *  Yes. 0 guest(s)

"*" represents the item is passed, while "-" represents the item is failed.

### Build

#### Prerequisites

[Visual Studio][visualstudio] 2017 or later

Install the following components: **Desktop development with C++** (**C++ CMake
tools for Windows** is included)

#### Build steps

**Option A (Visual Studio)**

1. Open _CheckTool_ project in Visual Studio.

   **File** > **Open** > **Folder...** > **Select Folder** "CheckTool"
   (containing _CMakeLists.txt_)
1. Select proper configuration, e.g., "x86-Debug".
1. Build project.

   **Build** > **Build All**

The executable program (_checktool.exe_) will be generated in
_X:\path\to\CheckTool\build\x86-Debug\\_. The 32-bit executable can run on both
32-bit and 64-bit Windows, while the 64-bit executable can run on 64-bit Windows
only.

**Option B (CMake)**

1. `set PATH=C:\Program Files (x86)\Microsoft Visual `
`Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%`
1. `cd X:\path\to\CheckTool`
1. `mkdir build && cd build && cmake .. && cmake --build .`

The executable program (_checktool.exe_) will be generated in
_X:\path\to\CheckTool\build\Debug\\_. It is easy to clean the build just by
removing the _build_ folder manually.

The full matrix for building 32-bit/64-bit Windows with Debug/Release
configuration can be referred as below.

|           | 32-bit Build                                 | 64-bit Build
----------- | -------------------------------------------- | ------------------------------------------
|           | `cmake -A Win32 -B build\Win32`              | `cmake -A x64 -B build\x64`
**Debug**   | `cmake --build build\Win32 --config Debug`   | `cmake --build build\x64 --config Debug`
**Release** | `cmake --build build\Win32 --config Release` | `cmake --build build\x64 --config Release`

The path in the first step is the CMake default extension path installed in
Visual Studio 2019. If [CMake][cmake] (3.17 or later) has been installed
independently and added to the system path, the first step of setting path can
be omitted.

## macOS

### Usage

1. `cd /path/to/CheckTool`
1. `./checktool --verbose`

### Build

#### Prerequisites

* [Xcode][xcode] 7.2.1 or later
* [CMake][cmake] 3.17 or later

#### Build steps

1. `cd /path/to/CheckTool`
1. `mkdir build && cd build && cmake .. && make`

The binary (_checktool_) will be generated in _/path/to/CheckTool/build/_. It is
easy to clean the build just by removing the _build_ folder manually.

The full list for building Debug/Release configuration can be referred as
below.

| Debug
| :--------------------------------------------------
| `cmake -DCMAKE_BUILD_TYPE=Debug -B build/Debug`
| `make -C build/Debug`
| **Release**
| `cmake -DCMAKE_BUILD_TYPE=Release -B build/Release`
| `make -C build/Release`

[checktool-release]: https://github.com/intel/haxm/releases/tag/checktool-v1.0.0
[cmake]: https://cmake.org/download/
[install-on-macos]:
https://github.com/intel/haxm/wiki/Installation-Instructions-on-macOS
[install-on-windows]:
https://github.com/intel/haxm/wiki/Installation-Instructions-on-Windows
[visualstudio]: https://www.visualstudio.com/downloads/
[xcode]: https://developer.apple.com/xcode/
