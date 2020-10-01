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

!ifndef REG_OPS_NSH_
!define REG_OPS_NSH_

!ifdef INSTALL
!include "Explode.nsh"
!endif

!include 'Log.nsh'
!include 'Strings.nsh'

!ifdef INSTALL

!macro WriteRegValues Subkey
  WriteRegStr ${REG_ROOT_KEY} ${Subkey} "DisplayName" "$(^Name)"
  WriteRegStr ${REG_ROOT_KEY} ${Subkey} "UninstallString" \
      "$INSTDIR\uninstall.exe"
  WriteRegStr ${REG_ROOT_KEY} ${Subkey} "DisplayIcon" \
      "$INSTDIR\uninstall.exe"
  WriteRegStr ${REG_ROOT_KEY} ${Subkey} "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${REG_ROOT_KEY} ${Subkey} "URLInfoAbout" "${PRODUCT_WEBSITE}"
  WriteRegStr ${REG_ROOT_KEY} ${Subkey} "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegDWORD ${REG_ROOT_KEY} ${Subkey} "EstimatedSize" "$0"
  WriteRegDWORD ${REG_ROOT_KEY} ${Subkey} "Version" "$1"
  WriteRegDWORD ${REG_ROOT_KEY} ${Subkey} "VersionMajor" "$2"
  WriteRegDWORD ${REG_ROOT_KEY} ${Subkey} "VersionMinor" "$3"
!macroend

!define WriteRegValues `!insertmacro WriteRegValues`

Function CreateRegItems
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0

  ${Explode} $1 "." ${PRODUCT_VERSION}
  Pop $2  ; VersionMajor
  Pop $3  ; VersionMinor
  Pop $4  ; VersionRevision
  IntOp $R2 $2 << 24
  IntOp $R3 $3 << 16
  IntOp $1 $R2 | $R3
  IntOp $1 $1 | $4  ; Version

  ${WriteRegValues} ${REG_KEY_INSTALL}
  WriteRegStr ${REG_ROOT_KEY} "${REG_KEY_USER_DATA}\Features" "" ""
  WriteRegStr ${REG_ROOT_KEY} "${REG_KEY_USER_DATA}\Patches" "" ""
  WriteRegStr ${REG_ROOT_KEY} "${REG_KEY_USER_DATA}\Usage" "" ""
  ${Log} "Create registry key: ${REG_ROOT_KEY}\${REG_KEY_USER_DATA}"

  ${WriteRegValues} ${REG_KEY_PRODUCT}
  ${Log} "Create registry key: ${REG_ROOT_KEY}\${REG_KEY_PRODUCT}"
FunctionEnd

!else

Function un.DeleteRegItems
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}

  DeleteRegKey ${REG_ROOT_KEY} ${REG_KEY_USER_DATA}
  ${Log} "Delete registry key: ${REG_ROOT_KEY}\${REG_KEY_USER_DATA}"
  DeleteRegKey ${REG_ROOT_KEY} ${REG_KEY_PRODUCT}
  ${Log} "Delete registry key: ${REG_ROOT_KEY}\${REG_KEY_PRODUCT}"
FunctionEnd

!endif  # INSTALL

!endif  # REG_OPS_NSH_
