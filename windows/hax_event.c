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
#include <wdm.h>
#include "hax_win.h"

/* Really have no idea of the unicode string, so no insertion string input */
int write_event(NTSTATUS err_code, PVOID obj, void *dump_data, int dsize)
{
    uint8_t dump_size = 0;
    PIO_ERROR_LOG_PACKET packet;

    if (dump_data)
        dump_size = (dsize + sizeof(ULONG) - 1) & ~(sizeof(ULONG) - 1);
    if ((dump_size + sizeof(IO_ERROR_LOG_PACKET)) > ERROR_LOG_MAXIMUM_SIZE)
        return -1;
    packet = IoAllocateErrorLogEntry(obj,
                                     sizeof(IO_ERROR_LOG_PACKET) + dump_size);
    if (packet == NULL)
        return -1;
    packet->ErrorCode = err_code;
    packet->DumpDataSize = dump_size;
    if (dump_data) {
        memcpy_s(&packet->DumpData[0], sizeof(IO_ERROR_LOG_PACKET) + dump_size,
                 dump_data, dump_size);
    }
    /* DumpData should be multiple of sizeof(ulong) */
    IoWriteErrorLogEntry(packet);
    return 0;
}

NTSTATUS ce2he[]= {
    HaxDriverNoVT,  /* NoVtEvent */
    HaxDriverNoNX,
    HaxDriverNoEMT64,
    HaxDriverVTDisable,
    HaxDriverNXDisable,
    HaxDriverVTEnableFailure,   /* VT enable failed */
};
uint8_t ce2he_size = sizeof(ce2he)/sizeof(NTSTATUS);

int hax_notify_host_event(enum hax_notify_event event, uint32_t *param,
                          uint32_t size)
{
    if (event >= ce2he_size)
        return -EINVAL;
    if (!HaxDriverObject)
        return -EINVAL;

    write_event(ce2he[event], HaxDriverObject, param, size);

    return 0;
}
