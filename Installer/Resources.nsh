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

!ifndef RESOURCES_NSH_
!define RESOURCES_NSH_

# Strings
!define PRODUCT_NAME          "HAXM"
!define PRODUCT_FULL_NAME     "Hardware Accelerated Execution Manager"
!define PRODUCT_VERSION       "7.6.5"
!define PRODUCT_YEAR          "2020"
!define PRODUCT_PUBLISHER     "Intel Corporation"
!define PRODUCT_BRAND         "Intel${U+00AE}"
!define PRODUCT_WEBSITE       "https://github.com/intel/haxm"
!define PRODUCT_WIKIPAGE      "/wiki/Windows-System-Configurations"
!define PRODUCT_GUID          "AAA802A8DF574F4CA0489512D2D91818"

!define PROGRAM_DIR           "\Intel\${PRODUCT_NAME}"

!define SERVICE_NAME          "IntelHaxm"
!define SERVICE_DISPLAY_NAME  "Intel(R) ${PRODUCT_FULL_NAME} Service"

!define REG_ROOT_KEY             "HKLM"
!define REG_KEY_CURRENT_VERSION  "SOFTWARE\Microsoft\Windows\CurrentVersion"
!define REG_KEY_USER_DATA        "${REG_KEY_CURRENT_VERSION}\Installer\UserData\
                                 \S-1-5-18\Products\${PRODUCT_GUID}"
!define REG_KEY_INSTALL          "${REG_KEY_USER_DATA}\InstallProperties"
!define REG_KEY_UNINSTALL        "${REG_KEY_CURRENT_VERSION}\Uninstall"
!define REG_KEY_PRODUCT          "${REG_KEY_UNINSTALL}\${PRODUCT_NAME}"

!define DRIVER_DIR       "$SYSDIR\drivers"
!define DRIVER_FILE      "${SERVICE_NAME}.sys"
!define DRIVER_WIN10_64  "assets\win10\x64\${DRIVER_FILE}"
!define DRIVER_WIN10_32  "assets\win10\x86\${DRIVER_FILE}"
!define DRIVER_WIN7_64   "assets\win7\x64\${DRIVER_FILE}"
!define DRIVER_WIN7_32   "assets\win7\x86\${DRIVER_FILE}"

!define DLG_SYS_ERROR    "The system requirements are not satisfied."
!define DLG_GUEST_ERROR  "${PRODUCT_NAME} is being used by running virtual \
        machines now."
!define DLG_WARNING      "The ${PRODUCT_NAME} driver can be installed on this \
        computer. However, it is required to further configure the system to \
        make it usable. Then ${PRODUCT_NAME} will be automatically loaded for \
        use."
!define DLG_DOWNGRADE    "The existing version is greater than the version to \
        be installed. Please uninstall the existing version manually before \
        proceeding."
!define DLG_REINSTALL    "${PRODUCT_NAME} v${PRODUCT_VERSION} has already been \
        installed. Are you sure to continue?"
!define DLG_UNINSTALL    "Are you sure you want to remove $(^Name)?"
!define LOG_REINSTALL    "To reinstall the current version"
!define LOG_UNINSTALL    "To uninstall the current version"
!define LOG_UPGRADE      "To upgrade version"

# Constants
!define ENV_FLAG_CPU_SUPPORTED     0x00000001
!define ENV_FLAG_VMX_SUPPORTED     0x00000002
!define ENV_FLAG_NX_SUPPORTED      0x00000004
!define ENV_FLAG_EM64T_SUPPORTED   0x00000008
!define ENV_FLAG_EPT_SUPPORTED     0x00000010
!define ENV_FLAG_VMX_ENABLED       0x00000100
!define ENV_FLAG_NX_ENABLED        0x00000200
!define ENV_FLAG_EM64T_ENABLED     0x00000400
!define ENV_FLAG_OSVER_SUPPORTED   0x00010000
!define ENV_FLAG_OSARCH_SUPPORTED  0x00020000
!define ENV_FLAG_HYPERV_DISABLED   0x00040000
!define ENV_FLAG_SANDBOX_DISABLED  0x00080000
!define ENV_FLAG_GUEST_UNOCCUPIED  0x01000000

# Hardware supports:
#   ENV_FLAG_CPU_SUPPORTED, ENV_FLAG_VMX_SUPPORTED, ENV_FLAG_NX_SUPPORTED,
#   ENV_FLAG_EM64T_SUPPORTED, ENV_FLAG_EPT_SUPPORTED
# OS supports:
#   ENV_FLAG_OSVER_SUPPORTED, ENV_FLAG_OSARCH_SUPPORTED
!define ENV_FLAGS_SYS_SUPPORTED    0x000300ff
# BIOS settings:
#   ENV_FLAG_VMX_ENABLED, ENV_FLAG_NX_ENABLED, ENV_FLAG_EM64T_ENABLED
# OS settings:
#   ENV_FLAG_HYPERV_DISABLED, ENV_FLAG_SANDBOX_DISABLED
!define ENV_FLAGS_HOST_READY       0x00fcff00
# Guest status:
#   ENV_FLAG_GUEST_UNOCCUPIED
!define ENV_FLAGS_GUEST_READY      0xff000000

!endif  # RESOURCES_NSH_
