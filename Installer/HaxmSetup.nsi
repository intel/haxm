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

Unicode true
!define INSTALL

!include "StrStr.nsh"
!include "WinVer.nsh"
!include "WordFunc.nsh"
!include "x64.nsh"

!include 'RegOps.nsh'
!include 'Service.nsh'
!include 'UI.nsh'
!include 'Utils.nsh'

Var code

Section Main
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}
  ${DisableX64FSRedirection}

  SetOutPath "$INSTDIR"
  ${Log} "Output folder: $INSTDIR"

  SetOverwrite try
  File "assets\checktool.exe"
  ${Log} "Extract: checktool.exe... 100%"

  Call CheckEnv
  Call UninstallMsiVersion
  Call InstallDriver

  ; WriteUninstaller is not used directly because a signed binary is needed
  File "assets\uninstall.exe"
  ${Log} "Create uninstaller: $INSTDIR\uninstall.exe"
SectionEnd

Section -Post
  Call onInstalled
SectionEnd

Function .onInit
  ${OpenLog}
  Call CheckVersion

  ${If} ${RunningX64}
    StrCpy $0 $PROGRAMFILES64
  ${Else}
    StrCpy $0 $PROGRAMFILES
  ${EndIf}

  StrCpy $INSTDIR "$0${PROGRAM_DIR}"
FunctionEnd

Function CheckVersion
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}

  ClearErrors
  EnumRegKey $0 ${REG_ROOT_KEY} ${REG_KEY_PRODUCT} 0
  IfErrors not_installed installed
  not_installed:
    StrCpy $code 0
    ${Log} "Version: ${PRODUCT_VERSION}"
    Return
  installed:
    ReadRegStr $0 ${REG_ROOT_KEY} ${REG_KEY_PRODUCT} "DisplayVersion"
    ${VersionCompare} $0 ${PRODUCT_VERSION} $R0
    ${Switch} $R0
      ${Case} 0
        MessageBox MB_YESNO|MB_ICONQUESTION "${DLG_REINSTALL}" /SD IDYES IDYES \
            reinstall
        ${Exit} 1 0
  reinstall:
        StrCpy $code 1
        ${Log} "${LOG_REINSTALL}: $0"
        ${Break}
      ${Case} 1
        MessageBox MB_OK|MB_ICONEXCLAMATION "${DLG_DOWNGRADE}" /SD IDOK
        ${Log} "${LOG_UNINSTALL}: $0"
        ${Exit} 1 3
        ${Break}
      ${Default}
        StrCpy $code 2
        ${Log} "${LOG_UPGRADE}: $0 => ${PRODUCT_VERSION}"
        ${Break}
    ${EndSwitch}
FunctionEnd

Function UninstallMsiVersion
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}

  StrCpy $R0 0
  EnumRegKey $0 ${REG_ROOT_KEY} ${REG_KEY_UNINSTALL} $R0

  ${While} $0 != ""
    ReadRegStr $1 ${REG_ROOT_KEY} ${REG_KEY_UNINSTALL}\$0 "DisplayName"

    ${StrStr} $2 $1 "${PRODUCT_FULL_NAME}"

    ; MSI version's subkey is the product code GUID, rather than the product name.
    ${If} $2 != ""
    ${AndIf} $0 != ${PRODUCT_NAME}
      ReadRegStr $3 ${REG_ROOT_KEY} ${REG_KEY_UNINSTALL}\$0 "UninstallString"
      ${ExecCommand} "$3 /qn" false
      ${Break}
    ${EndIf}

    IntOp $R0 $R0 + 1
    EnumRegKey $0 ${REG_ROOT_KEY} ${REG_KEY_UNINSTALL} $R0
  ${EndWhile}
FunctionEnd

Function InstallDriver
  ${If} ${AtLeastWin10}
    ${If} ${RunningX64}
      File ${DRIVER_WIN10_64}
    ${Else}
      File ${DRIVER_WIN10_32}
    ${EndIf}
  ${Else}
    ${If} ${RunningX64}
      File ${DRIVER_WIN7_64}
    ${Else}
      File ${DRIVER_WIN7_32}
    ${EndIf}
  ${EndIf}
  ${Log} "Extract: ${DRIVER_FILE}... 100%"

  ${If} $code > 0
    Call StopService
  ${EndIf}

  CopyFiles "$INSTDIR\${DRIVER_FILE}" "${DRIVER_DIR}"
  ${Log} "Copy to ${DRIVER_DIR}\${DRIVER_FILE}"
FunctionEnd

Function onInstalled
  ${If} $code == 0
    Call CreateService
  ${EndIf}

  Call StartService
  Call CreateRegItems

  ${Exit} 0 $code
FunctionEnd

Function onAbort
  ${Exit} 0 0
FunctionEnd
