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

//#include <ntddk.h>
#include <ntifs.h>
#include <string.h>
#include <ntdef.h>
#include <ntstrsafe.h>
#include "hax_win.h"

int hax_init_unicodestring(PUNICODE_STRING str, int length)
{
    void *buf;

    buf = hax_vmalloc(length, 0);
    if (!buf)
        return -ENOMEM;
   str->Buffer = buf;
   str->Length = length;

    RtlInitEmptyUnicodeString(str, buf, length);

    return 0;
}

/*
 * Can we simply use RtlFreeUnicodeString? Not sure since we can't find
 * the implementation and the DDK stated it's for RtlAnsiStringToUnicodeString
 * Only used for unicode string setup with hax_init_unicodestring
 */
int hax_free_unicodestring(PUNICODE_STRING str)
{
    hax_vfree(str->Buffer, str->MaximumLength);
    str->MaximumLength = 0;
    str->Length  = 0;
    str->Buffer = NULL;
    return 0;
}
