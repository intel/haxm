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

// VM and VCPU host components handling
#include "hax_win.h"

#include <Ntstrsafe.h>
#include <wdmsec.h>
#include <ntifs.h>

#define HAX_VCPU_DEV_FMT     L"\\Device\\hax_vm%02d_vcpu%02d"
#define HAX_VCPU_DOS_DEV_FMT L"\\DosDevices\\hax_vm%02d_vcpu%02d"
#define HAX_VM_DEV_FMT       L"\\Device\\hax_vm%02d"
#define HAX_VM_DOS_DEV_FMT   L"\\DosDevices\\hax_vm%02d"

GUID HAX_VCPU_GUID =
{0x24e0d1e0L, 0x8189, 0x34e0, {0x99, 0x16, 0x04, 0x55, 0x22, 0x67, 0x30, 0x08}};

GUID  HAX_VM_GUID =
{0x49d5e8e1L, 0x0906, 0x15c0, {0x88, 0x44, 0x09, 0x00, 0x56, 0x79, 0x24, 0x05}};

int hax_vcpu_create_host(struct vcpu_t *cvcpu, void *vm_host, int vm_id,
                         int vcpu_id)
{
    NTSTATUS ntStatus;
    struct hax_dev_ext *DevExt = NULL;
    struct hax_vcpu_windows *vcpu;
    struct hax_vm_windows *vm = vm_host;
    PDEVICE_OBJECT pDevObj;
    DECLARE_UNICODE_STRING_SIZE(ntUnicodeString, 64);
    DECLARE_UNICODE_STRING_SIZE(ntWin32NameString, 64);

    if (!vm)
        return -EINVAL;

    RtlUnicodeStringPrintf(&ntUnicodeString, HAX_VCPU_DEV_FMT, vm_id, vcpu_id);
    RtlUnicodeStringPrintf(&ntWin32NameString, HAX_VCPU_DOS_DEV_FMT, vm_id,
                           vcpu_id);

    ntStatus = IoCreateDeviceSecure(
            HaxDriverObject, sizeof(struct hax_dev_ext), &ntUnicodeString,
            FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
            // An exclusive device, only corresponding QEMU can open it.
            TRUE, vm->ssdl, (LPGUID)&HAX_VCPU_GUID, &pDevObj);

    if (!NT_SUCCESS(ntStatus)) {
        hax_log(HAX_LOGE, "Failed to create VCPU device\n");
        return -1;
    }

    DevExt = (struct hax_dev_ext *)pDevObj->DeviceExtension;
    ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);
    if (!NT_SUCCESS(ntStatus)) {
        hax_log(HAX_LOGE, "Failed to creaet symbolic link \n");
        IoDeleteDevice(pDevObj);
        return -1;
    }
    pDevObj->Flags &= ~DO_DEVICE_INITIALIZING;
    DevExt->type = HAX_DEVEXT_TYPE_VCPU;
    vcpu = &DevExt->vcpudev_ext;
    vcpu->cvcpu = cvcpu;
    vcpu->vm_id = vm_id;
    vcpu->vcpu_id = vcpu_id;
    vcpu->ext = pDevObj;
    set_vcpu_host(cvcpu, vcpu);
    return 0;
}

int hax_vcpu_destroy_host(struct vcpu_t *cvcpu, struct hax_vcpu_windows *vcpu)
{
    PDEVICE_OBJECT pDevObj;
    DECLARE_UNICODE_STRING_SIZE(ntWin32NameString, 64);

    RtlUnicodeStringPrintf(&ntWin32NameString, HAX_VCPU_DOS_DEV_FMT,
                           vcpu->vm_id, vcpu->vcpu_id);

    pDevObj = (PDEVICE_OBJECT)vcpu->ext;
    IoDeleteSymbolicLink(&ntWin32NameString);

    hax_vcpu_destroy_hax_tunnel(cvcpu);

    set_vcpu_host(cvcpu, NULL);
    vcpu->cvcpu = NULL;
    IoDeleteDevice(pDevObj);

    return 0;
}

static int hax_win_destruct_ssdl(PUNICODE_STRING ssdl)
{
    if (!ssdl)
        return -EINVAL;

    hax_free_unicodestring(ssdl);
    hax_vfree(ssdl, sizeof(UNICODE_STRING));

    return 0;
}

static PUNICODE_STRING hax_win_construct_ssdl(void)
{
    NTSTATUS ntStatus;
    PUNICODE_STRING ssdl = NULL;
    int ssdlValid = 0;
    uint64_t length = 256;

    ssdl = hax_vmalloc(sizeof(UNICODE_STRING), 0);
    if (!ssdl) {
        hax_log(HAX_LOGE, "Failed to alloc ssdl\n");
        goto error;
    }
    if (hax_init_unicodestring(ssdl, length)) {
        hax_log(HAX_LOGE, "Failed to get the ssid unicode string\n");
        goto error;
    }
    ssdlValid = 1;
    ntStatus = RtlUnicodeStringPrintf(
            ssdl, L"%ws", L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)");

    if (!NT_SUCCESS(ntStatus)) {
        hax_log(HAX_LOGE, "Failed to get the SSDL string\n");
        goto error;
    }

    return ssdl;
error:
    if (ssdlValid)
        hax_free_unicodestring(ssdl);
    if (ssdl)
        hax_vfree(ssdl, sizeof(UNICODE_STRING));
    return NULL;
}

int hax_vm_create_host(struct vm_t *cvm, int vm_id)
{
    NTSTATUS ntStatus;
    struct hax_dev_ext *DevExt = NULL;
    struct hax_vm_windows *vm;
    PDEVICE_OBJECT pDevObj;
    PUNICODE_STRING ssdl;

    DECLARE_UNICODE_STRING_SIZE(ntUnicodeString, 64);
    DECLARE_UNICODE_STRING_SIZE(ntWin32NameString, 64);

    RtlUnicodeStringPrintf(&ntUnicodeString, HAX_VM_DEV_FMT, vm_id);
    RtlUnicodeStringPrintf(&ntWin32NameString, HAX_VM_DOS_DEV_FMT, vm_id);

    ssdl = hax_win_construct_ssdl();
    if (!ssdl) {
        hax_log(HAX_LOGE, "Failed to construct ssdl for current thread\n");
        return -1;
    }

    ntStatus = IoCreateDeviceSecure(
            HaxDriverObject, sizeof(struct hax_dev_ext), &ntUnicodeString,
            FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
            // An exclusive device, only corresponding QEMU can open it.
            TRUE, ssdl,
            // According to DDK, we have to use the GUID, but no idea why.
            (LPGUID)&HAX_VM_GUID, &pDevObj);

    if (!NT_SUCCESS(ntStatus)) {
        hax_log(HAX_LOGE, "Failed to create VM device\n");
        hax_win_destruct_ssdl(ssdl);
        return -1;
    }

    DevExt = (struct hax_dev_ext *)pDevObj->DeviceExtension;

    ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);
    if (!NT_SUCCESS(ntStatus)) {
        hax_log(HAX_LOGE, "Failed to creaet symbolic link \n");
        hax_win_destruct_ssdl(ssdl);
        IoDeleteDevice(pDevObj);
        return -1;
    }

    pDevObj->Flags &= ~DO_DEVICE_INITIALIZING;
    DevExt->type = HAX_DEVEXT_TYPE_VM;
    vm = &DevExt->vmdev_ext;
    vm->cvm = cvm;
    vm->vm_id = vm_id;
    vm->ext = pDevObj;
    vm->ssdl = ssdl;
    set_vm_host(cvm, vm);
    return 0;
}

/* No corresponding function in Windows side, it can be cleaned later. */
int hax_destroy_host_interface(void)
{
    return 0;
}

/* When coming here, all vcpus should have been destroyed already. */
int hax_vm_destroy_host(struct vm_t *cvm, void *host_pointer)
{
    PDEVICE_OBJECT pDevObj;
    DECLARE_UNICODE_STRING_SIZE(ntWin32NameString, 64);
    struct hax_vm_windows *vm = (struct hax_vm_windows *)host_pointer;

    RtlUnicodeStringPrintf(&ntWin32NameString, HAX_VM_DOS_DEV_FMT, vm->vm_id);
    pDevObj = (PDEVICE_OBJECT)vm->ext;

    IoDeleteSymbolicLink(&ntWin32NameString);

    set_vm_host(cvm, NULL);
    vm->cvm = NULL;
    hax_win_destruct_ssdl(vm->ssdl);
    hax_vm_free_all_ram(cvm);

    /* The hax_vm_windows should be freed here */
    IoDeleteDevice(pDevObj);

    return 0;
}
