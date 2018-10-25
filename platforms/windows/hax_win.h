/*
 * Copyright (c) 2011 Intel Corporation
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

#ifndef HAX_WINDOWS_HAX_WIN_H_
#define HAX_WINDOWS_HAX_WIN_H_

//#include <ntddk.h>
#include <Ntifs.h>
#include <stdarg.h>
#include "hax_core_interface.h"
#include "../../include/hax_interface.h"
#include "hax_types_windows.h"
#include "hax_entry.h"
#include "hax_event_win.h"

struct windows_vcpu_mem {
    uint32_t flags;
    PMDL pmdl;
};

int hax_init_unicodestring(PUNICODE_STRING str, int length);

int hax_free_unicodestring(PUNICODE_STRING str);

int write_event(NTSTATUS err_code, PVOID obj, void *dump_data, int dsize);

int hax_valid_uva(uint64_t uva, uint64_t size);

NTSTATUS
PptRegGetDword(
  IN     ULONG RelativeTo,
  IN     __nullterminated PWSTR Path,
  IN     __nullterminated PWSTR ParameterName,
  IN     OUT PULONG ParameterValue
);

#define MAX_HOST_MEM_SIZE ((uint64_t)1 << 41)

#endif  // HAX_WINDOWS_HAX_WIN_H_
