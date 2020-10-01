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

!ifndef UI_NSH_
!define UI_NSH_

!include "MUI.nsh"

!include 'Strings.nsh'

RequestExecutionLevel  admin
ShowInstDetails        show
ShowUnInstDetails      show

Name              "${PRODUCT_BRAND} ${PRODUCT_FULL_NAME}"
BrandingText      "${PRODUCT_BRAND} ${PRODUCT_NAME} v${PRODUCT_VERSION}"
Caption           "${PRODUCT_NAME} - Open Source Cross-Platform Hypervisor"
UninstallCaption  "${PRODUCT_BRAND} ${PRODUCT_NAME} Uninstall"
!ifdef INSTALL
OutFile           "haxm-${PRODUCT_VERSION}-setup.exe"
!else
OutFile           "assets\uninstall-maker.exe"
!endif

# Version information
VIProductVersion  "${PRODUCT_VERSION}.0"
VIAddVersionKey   CompanyName      "${PRODUCT_PUBLISHER}"
VIAddVersionKey   FileDescription  "${PRODUCT_BRAND} ${PRODUCT_FULL_NAME} \
                                    Installer"
VIAddVersionKey   FileVersion      "${PRODUCT_VERSION}"
VIAddVersionKey   LegalCopyright   "${U+00A9} ${PRODUCT_YEAR} \
                                    ${PRODUCT_PUBLISHER}"
VIAddVersionKey   ProductName      "${PRODUCT_BRAND} ${PRODUCT_FULL_NAME}"
VIAddVersionKey   ProductVersion   "${PRODUCT_VERSION}"

!define MUI_ABORTWARNING
!ifdef INSTALL
!define MUI_CUSTOMFUNCTION_ABORT      onAbort
!endif
!define MUI_ICON                      "res\haxm_logo.ico"
!define MUI_UNICON                    "res\haxm_logo.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP  "res\cover.bmp"

# Welcome page
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the \
        installation of $(^Name) (${PRODUCT_NAME}) ${PRODUCT_VERSION}. \
        ${PRODUCT_NAME} is a hardware-assisted virtualization engine \
        (hypervisor), widely used as an accelerator for Android Emulator and \
        QEMU.\
        \r\n\r\nImportant: ${PRODUCT_NAME} requires an Intel CPU with certain \
        hardware features, including ${PRODUCT_BRAND} Virtualization \
        Technology (${PRODUCT_BRAND} VT), etc. This installer will check \
        whether your computer can run ${PRODUCT_NAME}."

# License page
!define MUI_LICENSEPAGE_TEXT_BOTTOM "If you accept the terms of the license \
        agreement, click Install to continue $(^Name) setup."
!define MUI_LICENSEPAGE_BUTTON "&Install"

# Finish page
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_LINK_LOCATION ${PRODUCT_WEBSITE}
!define MUI_FINISHPAGE_LINK "${PRODUCT_NAME} Homepage: ${PRODUCT_WEBSITE}"

# Wizard dialogs
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "assets\LICENSE"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!ifndef INSTALL
!insertmacro MUI_UNPAGE_INSTFILES
!endif

!insertmacro MUI_LANGUAGE "English"

!endif  # UI_NSH_
