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

!include "x64.nsh"

!include 'RegOps.nsh'
!include 'Service.nsh'
!include 'UI.nsh'
!include 'Utils.nsh'

Function .onInit
  SetSilent silent
FunctionEnd

Section Main
  WriteUninstaller "$EXEDIR\uninstall.exe"
SectionEnd

Function un.onInit
  MessageBox MB_YESNO|MB_ICONEXCLAMATION "${DLG_UNINSTALL}" /SD IDYES IDYES \
      Uninstall
  ${Exit} 2 0
Uninstall:
FunctionEnd

Section Uninstall
  ${DisableX64FSRedirection}

  ${OpenLog}
  ${Log} "Version: ${PRODUCT_VERSION}"
  Call un.CheckEnv

  ; Sometimes checktool.exe is still locked when removing $INSTDIR. Below two
  ; parts ensure that checktool.exe is completely unlocked for removal.
  ; In silent uninstall mode, these parts of logic will be skipped but completed
  ; by silent_install.bat instead. This is because the uninstaller itself will
  ; always be occupied in $INSTDIR, which can only be removed by batch file.
  IfSilent Remove
  ${For} $0 0 2
    ExecWait 'cmd.exe /c ren "$INSTDIR" ${PRODUCT_NAME}' $1
    ${If} $1 == 0
      ${Break}
    ${EndIf}
    Sleep 5000
  ${Next}

  ${If} $1 == 1
    ${Log} "Error: Installed program files are being locked."
    ${Exit} 0 3
  ${EndIf}

Remove:
  Call un.DeleteRegItems
  Call un.StopService
  Call un.DeleteService

  Delete "${DRIVER_DIR}\${DRIVER_FILE}"
  ${Log} "Delete file: ${DRIVER_DIR}\${DRIVER_FILE}"

  RMDir /r "$INSTDIR"
  ${Log} "Delete folder: $INSTDIR"

  SetAutoClose true
  ${Exit} 0 0
SectionEnd
