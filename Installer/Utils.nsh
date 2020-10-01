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

!ifndef UTILS_NSH_
!define UTILS_NSH_

!include "LogicLib.nsh"

!include 'Log.nsh'
!include 'Strings.nsh'

# ExecCommand --------------------------

!macro Output un
  Function ${un}Output
    Pop $0
    ${If} $R2 == true
      DetailPrint $0
    ${EndIf}
    ${Log} $0
  FunctionEnd
!macroend

!ifdef INSTALL
!insertmacro Output ""
!else
!insertmacro Output "un."
!endif

!macro Execute un
  GetFunctionAddress $R1 ${un}Output
  DetailPrint 'Execute: ${Command}'
  ${Log} 'Execute: ${Command}'
  StrCpy $R2 ${Detailed}
  ExecDos::exec /TOFUNC '${Command}' "" $R1
!macroend

!ifdef INSTALL

!macro ExecCommandCall Command Detailed
  !insertmacro Execute ""
!macroend
!define ExecCommand `!insertmacro ExecCommandCall`

!else

!macro un.ExecCommandCall Command Detailed
  !insertmacro Execute "un."
!macroend
!define un.ExecCommand `!insertmacro un.ExecCommandCall`

!endif  # INSTALL

# Exit ---------------------------------

!macro Exit Mode Code
  ${CloseLog}
  SetErrorLevel ${Code}
  ${Switch} ${Mode}
    ${Case} 1
      Quit
      ${Break}
    ${Case} 2
      Abort
      ${Break}
    ${Default}
      ${Break}
  ${EndSwitch}
!macroend

!define Exit `!insertmacro Exit`

# CheckEnv -----------------------------

!macro CheckEnv un
  Function ${un}CheckEnv
    ${${un}ExecCommand} '$INSTDIR\checktool.exe --verbose' true
    Pop $0

    ${If} $0 != "0"
      MessageBox MB_OK|MB_ICONSTOP "${DLG_CHECK_ENV}" /SD IDOK
      ${Log} "${DLG_CHECK_ENV}"
      ${Exit} 2 3
    ${EndIf}
  FunctionEnd
!macroend

!ifdef INSTALL
!insertmacro CheckEnv ""
!else
!insertmacro CheckEnv "un."
!endif

!endif  # UTILS_NSH_
