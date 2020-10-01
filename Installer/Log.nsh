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

!ifndef LOG_NSH_
!define LOG_NSH_

!include "FileFunc.nsh"

!insertmacro GetTime

Var log
Var path

!macro OpenLog
  GetFullPathName $path "."
  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  IntCmp $4 9 0 0 +2
  StrCpy $4 "0$4"
!ifdef INSTALL
  StrCpy $R0 "install"
!else
  StrCpy $R0 "uninstall"
!endif
  StrCpy $log "haxm_$R0-$2$1$0_$4$5.log"
  ; The log file is created in the current folder rather than $TEMP directly
  ; because:
  ;   1. Can be easily found by silent_install.bat
  ;   2. Convert Unicode to ANSI format to save disk space while copying it
  LogEx::Init true "$log"
!macroend

!define OpenLog `!insertmacro OpenLog`

!macro Log string
  LogEx::Write '${string}'
!macroend

!define Log `!insertmacro Log`

!macro CloseLog
  LogEx::Close
  ExecWait 'cmd.exe /c type "$path\$log" > "$TEMP\$log"' $0
  IfSilent +2 0
  Delete "$path\$log"
!macroend

!define CloseLog `!insertmacro CloseLog`

!endif  # LOG_NSH_
