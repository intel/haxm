---
name: Bug report
about: Create a report to help us improve
title: ''
labels: ''
assignees: ''

---

<!--
We really appreciate your bug report, but we ask you to search for similar issues before creating one. If you believe you have run into a new bug, please fill out the following form to the best of your knowledge.

This template formats text using Markdown annotations. Basics of the Markdown syntax are covered here: https://guides.github.com/features/mastering-markdown/

HTML-style comments like this are instructions. Before answering each question, please read the comment right above it.

Before clicking on the "Submit new issue" button, you may want to switch to the Preview tab and make sure everything looks good.
-->

**Describe the Bug**

<!-- A clear and concise description of what the bug is. -->
Summary: 


**Host Environment**

<!-- E.g. 7.3.2. If you built HAXM from source, provide the Git commit ID. -->
* HAXM version: 
<!-- E.g. Windows 10 Pro version 1809, macOS 10.14.1, etc. -->
* Host OS version: 
<!-- Either x86\_64 (64-bit) or x86 (32-bit) -->
* Host OS architecture: x86\_64
<!-- E.g. Intel Core i7-8550U -->
* Host CPU model: 
<!-- E.g. 8 GB -->
* Host RAM size: 
<!-- E.g. Dell Inspiron 14 7472, MacBookPro15,2, etc. -->
* (Optional) Host computer model: 


**Guest Environment**

<!-- Complete this section if you are trying to run Android Emulator or QEMU with HAXM acceleration. -->

<!-- E.g. Android Emulator 28.0.20, QEMU 3.1.0, etc. -->
* Android Emulator or QEMU version: 
<!-- E.g. Ubuntu 18.04. In Android Emulator terminology, this is the AVD system image version, e.g. Android 9.0 (Google Play). -->
* Guest OS version: 
<!-- Either x86 or x86\_64. In Android Emulator terminology, this is the AVD system image ABI. -->
* Guest OS architecture: 


**To Reproduce**

<!-- If applicable, provide the Android Emulator or QEMU command line, e.g. `emulator -avd Nexus_5X_API_28 -verbose -show-kernel`, `qemu-system-x86_64 -accel hax`, etc. -->
Steps to reproduce the behavior:
1. 
2. 
3. 


**Expected Behavior**

<!-- A clear and concise description of what you expected to happen. -->
Expectation: 


**Reproducibility**

<!-- The probability of getting the issue reproduced after following the above steps, e.g. 100%, 50%, etc. -->


**Diagnostic Information**

<!--
If you get a host crash, i.e. Windows Blue Screen of Death (BSOD) or macOS kernel panic, provide the crash dump (by attaching it to this GitHub issue or sharing it with us in private).
Windows BSOD minidumps can be found in `C:\Windows\Minidump\`.
macOS kernel panic reports can be found in `/Library/Logs/DiagnosticReports/`.
-->
Host crash dump: 

<!--
If you get a guest crash, i.e. QEMU error `VCPU shutdown request`, provide the HAXM driver log.
Instructions for Windows hosts: https://github.com/intel/haxm/blob/master/docs/manual-windows.md#capturing-driver-logs
Instructions for macOS hosts: https://github.com/intel/haxm/blob/master/docs/manual-macos.md#viewing-kext-logs
-->
HAXM log: 

<!-- If you see any error or warning message printed to the console (stdout/stderr) by Android Emulator or QEMU, provide the entire console output.  -->
Android Emulator or QEMU log: 

<!-- If applicable, add screenshots to help explain your problem. -->
Screenshots: 


**Additional context**

<!-- Add any other context about the problem here. -->
None
