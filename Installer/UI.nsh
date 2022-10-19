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

!include "MUI2.nsh"

!include 'Resources.nsh'

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

!define MUI_ICON                      "res\haxm_logo.ico"
!define MUI_UNICON                    "res\haxm_logo.ico"

!ifdef INSTALL
Var title
Var text
Var link
Var url

!define MUI_WELCOMEFINISHPAGE_BITMAP  "res\cover.bmp"
!define MUI_ABORTWARNING
!define MUI_CUSTOMFUNCTION_ABORT      onAbort

# Welcome page
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the \
        installation of $(^Name) (${PRODUCT_NAME}) ${PRODUCT_VERSION}. \
        ${PRODUCT_NAME} is a hardware-assisted virtualization engine \
        (hypervisor), widely used as an accelerator for Android Emulator and \
        QEMU.$\r$\n$\r$\nImportant: ${PRODUCT_NAME} requires an Intel CPU with \
        certain hardware features, including ${PRODUCT_BRAND} Virtualization \
        Technology (${PRODUCT_BRAND} VT), etc. This installer will check \
        whether your computer can run ${PRODUCT_NAME}."

# License page
!define MUI_LICENSEPAGE_TEXT_BOTTOM "If you accept the terms of the license \
        agreement, click Install to continue $(^Name) setup."
!define MUI_LICENSEPAGE_BUTTON "&Install"

# Finish page
!define MUI_FINISHPAGE_TITLE "$title"
!define MUI_FINISHPAGE_TEXT "$text$\r$\n$\r$\nClick the Finish button to exit \
        the Setup Wizard."
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_TEXT_LARGE
!define MUI_FINISHPAGE_LINK "$link"
!define MUI_FINISHPAGE_LINK_LOCATION "$url"

# Wizard dialogs
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "assets\LICENSE"
!insertmacro MUI_PAGE_INSTFILES
!define MUI_PAGE_CUSTOMFUNCTION_PRE onFinished
!insertmacro MUI_PAGE_FINISH

# Wizard costom pages
!macro List Item Flag
  IntOp $0 $status & ${Flag}
  ${If} $0 != 0
    StrCpy $text "$text$\r$\n  - ${Item}"
  ${EndIf}
!macroend

!define List `!insertmacro List`

Function LoadSuccessPage
  StrCpy $title "${PG_COMPLETE_TITLE}"
  StrCpy $text "${PG_COMPLETE_TEXT}"
  StrCpy $link "${PG_HOMEPAGE}"
  StrCpy $url "${PRODUCT_WEBSITE}"
FunctionEnd

Function LoadSystemErrorPage
  StrCpy $title "${PG_FAIL_TITLE}"
  StrCpy $text "${PG_SYS_FAIL_TEXT}$\r$\n"

  ${List} "${PG_CPU_SUPPORT}"    ${ENV_FLAG_CPU_SUPPORTED}
  ${List} "${PG_VMX_SUPPORT}"    ${ENV_FLAG_VMX_SUPPORTED}
  ${List} "${PG_NX_SUPPORT}"     ${ENV_FLAG_NX_SUPPORTED}
  ${List} "${PG_EM64T_SUPPORT}"  ${ENV_FLAG_EM64T_SUPPORTED}
  ${List} "${PG_EPT_SUPPORT}"    ${ENV_FLAG_EPT_SUPPORTED}
  ${List} "${PG_OSVER_SUPPORT}"  ${ENV_FLAG_OSVER_SUPPORTED}
  ${List} "${PG_OSARCH_SUPPORT}" ${ENV_FLAG_OSARCH_SUPPORTED}

  StrCpy $link "${PG_HOMEPAGE}"
  StrCpy $url "${PRODUCT_WEBSITE}"
FunctionEnd

Function LoadHostErrorPage
  StrCpy $title "${PG_COMPLETE_TITLE}"
  StrCpy $text "${PG_HOST_FAIL_TEXT}$\r$\n"

  ${List} "${PG_ENABLE_VMX}"     ${ENV_FLAG_VMX_ENABLED}
  ${List} "${PG_ENABLE_NX}"      ${ENV_FLAG_NX_ENABLED}
  ${List} "${PG_DISABLE_HYPERV}" ${ENV_FLAG_HYPERV_DISABLED}

  StrCpy $link "${PG_WIKIPAGE}"
  StrCpy $url "${PRODUCT_WEBSITE}${PRODUCT_WIKIPAGE}"
FunctionEnd

Function LoadGuestErrorPage
  StrCpy $title "${PG_FAIL_TITLE}"
  StrCpy $text "${PG_GUEST_FAIL_TEXT}"
  StrCpy $link "${PG_HOMEPAGE}"
  StrCpy $url "${PRODUCT_WEBSITE}"
FunctionEnd
!else
!insertmacro MUI_UNPAGE_INSTFILES
!endif

!insertmacro MUI_LANGUAGE "English"

!endif  # UI_NSH_
