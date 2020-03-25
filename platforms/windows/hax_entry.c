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

// The entry code for HAX kernel driver

//#include <ntddk.h>
#include <ntifs.h>
#include <string.h>

#include "hax_win.h"

// vcpu.h
int vcpu_takeoff(struct vcpu_t *vcpu);
void vcpu_debug(struct vcpu_t *vcpu, struct hax_debug_t *debug);

#define NT_DEVICE_NAME L"\\Device\\HAX"
#define DOS_DEVICE_NAME L"\\DosDevices\\HAX"

DRIVER_INITIALIZE DriverEntry;
__drv_dispatchType(IRP_MJ_CREATE)
DRIVER_DISPATCH HaxCreate;
__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH HaxClose;
__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH HaxIoControl;
DRIVER_UNLOAD HaxUnloadDriver;

static int hax_host_init(void)
{
    int ret;

    ret = cpu_info_init();
    if (ret < 0) {
        hax_log(HAX_LOGE, "CPU info init failed\n");
        return ret;
    }

    ret = smpc_dpc_init();
    if (ret < 0) {
        hax_log(HAX_LOGE, "SMPC DPC init failed\n");
        cpu_info_exit();
        return ret;
    }

    if (hax_module_init() < 0) {
            hax_log(HAX_LOGE, "Hax module init failed\n");
            smpc_dpc_exit();
            cpu_info_exit();
            return -1;
    }

    return 0;
}

static int hax_host_exit(void)
{
    hax_module_exit();
    smpc_dpc_exit();
    return 0;
}

PDEVICE_OBJECT  HaxDeviceObject = NULL;    // ptr to device object

PDRIVER_OBJECT HaxDriverObject;
NTSTATUS DriverEntry(__in PDRIVER_OBJECT DriverObject,
                     __in PUNICODE_STRING RegistryPath)
{
    NTSTATUS        ntStatus;
    int ret;
    UNICODE_STRING  ntUnicodeString;    // NT Device Name "\Device\HAXDEV"
    UNICODE_STRING  ntWin32NameString;
    PDEVICE_OBJECT pDevObj;
    struct hax_dev_ext *DevExt = NULL;
    UNREFERENCED_PARAMETER(RegistryPath);
    RtlInitUnicodeString( &ntUnicodeString, NT_DEVICE_NAME );
    RtlInitUnicodeString( &ntWin32NameString, DOS_DEVICE_NAME );
    ntStatus = IoCreateDevice(DriverObject,   // Our Driver Object
                              sizeof(struct hax_dev_ext),
                              // Device name "\Device\SIOCTL"
                              &ntUnicodeString,
                              FILE_DEVICE_UNKNOWN,  // Device type
                              // Device characteristics
                              FILE_DEVICE_SECURE_OPEN,
                              FALSE,  // Not an exclusive device
                              &pDevObj);  // Returned ptr to Device Object
    if (!NT_SUCCESS(ntStatus)) {
        hax_log(HAX_LOGE, "Couldn't create the device object\n");
        write_event(HaxDriverCreateUpDevFailure, DriverObject, NULL, 0);
        return ntStatus;
    }
    DevExt = (struct hax_dev_ext *)pDevObj->DeviceExtension;
    DevExt->type = HAX_DEVEXT_TYPE_UP;

    //
    // Create a symbolic link between our device name  and the Win32 name
    //

    ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);
    if (!NT_SUCCESS(ntStatus)) {
        //
        // Delete everything that this routine has allocated.
        //
        hax_log(HAX_LOGE, "Couldn't create symbolic link\n");
        write_event(HaxDriverCreateUpSymFailure, DriverObject, NULL, 0);
        goto error_0;
    }
    /* hax_host_init may trigger eventlog which need HaxDriverObject */
    HaxDriverObject = DriverObject;
    ret = hax_host_init();
    if (ret < 0) {
        ntStatus = STATUS_UNSUCCESSFUL;
        hax_log(HAX_LOGE, "Hax host init failed\n");
        write_event(HaxDriverHostInitFailure, DriverObject, NULL, 0);
        goto error_1;
    }
    DriverObject->MajorFunction[IRP_MJ_CREATE] = HaxCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = HaxClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HaxIoControl;
    DriverObject->DriverUnload = HaxUnloadDriver;

    HaxDeviceObject = pDevObj;
    write_event(HaxDriverLoaded, DriverObject, NULL, 0);
    return ntStatus;
error_1:
    IoDeleteSymbolicLink(&ntWin32NameString);
error_0:
    IoDeleteDevice(pDevObj);
    HaxDriverObject = NULL;
    HaxDeviceObject = NULL;
    return ntStatus;
}

NTSTATUS HaxCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    struct hax_dev_windows *dev_win;
    struct hax_dev_ext *devext;

    devext = (struct hax_dev_ext *)DeviceObject->DeviceExtension;

    switch (devext->type) {
        case HAX_DEVEXT_TYPE_UP:
            dev_win = &devext->haxdev_ext;
            hax_atomic_add(&dev_win->count, 1);
            break;
        default:
            break;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS HaxClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    struct hax_dev_windows *dev_win;
    struct hax_dev_ext *devext;
    struct hax_vm_windows *vm;
    struct hax_vcpu_windows *vcpu;
    struct vcpu_t *cvcpu;
    struct vm_t *cvm;
    NTSTATUS ret = STATUS_SUCCESS;
    devext = (struct hax_dev_ext *)DeviceObject->DeviceExtension;

    hax_log(HAX_LOGI, "HaxClose device %x at process %p\n", devext->type,
            (ULONG_PTR)PsGetCurrentThread());
    switch (devext->type) {
        case HAX_DEVEXT_TYPE_UP:
            dev_win = &devext->haxdev_ext;
            hax_atomic_dec(&dev_win->count);
            break;
        case HAX_DEVEXT_TYPE_VM:
            vm = &devext->vmdev_ext;
            cvm = vm->cvm;
            hax_log(HAX_LOGI, "Close VM %x\n", vm->vm_id);
            if (cvm)
                hax_put_vm(cvm);
            break;
        case HAX_DEVEXT_TYPE_VCPU:
            vcpu = &devext->vcpudev_ext;
            cvcpu = hax_get_vcpu(vcpu->vm_id, vcpu->vcpu_id, 1);
            if (!cvcpu) {
                hax_log(HAX_LOGE, "Failed to get cvcpu for vm %x vcpu %x\n",
                        vcpu->vm_id, vcpu->vcpu_id);
                ret = STATUS_UNSUCCESSFUL;
                goto done;
            }
            hax_put_vcpu(vcpu->cvcpu);
            hax_put_vcpu(vcpu->cvcpu);
            break;
        default:
            hax_log(HAX_LOGE, "Invalid device type %x\n", devext->type);
            ret = STATUS_UNSUCCESSFUL;
            break;
    }

done:
    Irp->IoStatus.Status = ret;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

/* return >0 if success, <0 if wrong */
NTSTATUS hax_get_versioninfo(void *buf, int buflength, int *ret_bl)
{
    struct hax_module_version *version;

    if (buflength < sizeof(struct hax_module_version))
        return STATUS_BUFFER_TOO_SMALL;
    version = (struct hax_module_version *)buf;
    version->cur_version = HAX_CUR_VERSION;
    version->compat_version = HAX_COMPAT_VERSION;
    if (ret_bl)
        *ret_bl = sizeof(struct hax_module_version);

    return STATUS_SUCCESS;
}

NTSTATUS HaxVcpuControl(PDEVICE_OBJECT DeviceObject,
                        struct hax_vcpu_windows *ext, PIRP Irp)
{
    NTSTATUS ret = STATUS_SUCCESS;
    ULONG               inBufLength; // Input buffer length
    ULONG               outBufLength; // Output buffer length
    PCHAR               inBuf, outBuf; // pointer to Input and output buffer
    uint32_t vcpu_id, vm_id;
    struct vcpu_t *cvcpu;
    int infret = 0;
    struct hax_vcpu_windows *vcpu = ext;
    PIO_STACK_LOCATION  irpSp;// Pointer to current stack location

    irpSp = IoGetCurrentIrpStackLocation( Irp );
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    inBuf = Irp->AssociatedIrp.SystemBuffer;
    outBuf = Irp->AssociatedIrp.SystemBuffer;

    vm_id = vcpu->vm_id;
    vcpu_id = vcpu->vcpu_id;

    cvcpu = hax_get_vcpu(vm_id, vcpu_id, 1);
    if (!cvcpu) {
        ret = STATUS_INVALID_HANDLE;
        goto done;
    }

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
        case HAX_VCPU_IOCTL_RUN:
            if (vcpu_execute(cvcpu))
                ret = STATUS_UNSUCCESSFUL;
            break;
        case HAX_VCPU_IOCTL_SETUP_TUNNEL: {
            struct hax_tunnel_info info, *uinfo;
            if (outBufLength < sizeof(struct hax_tunnel_info )) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            uinfo = (struct hax_tunnel_info *)outBuf;
            ret = hax_vcpu_setup_hax_tunnel(cvcpu, &info);
            uinfo->va = info.va;
            uinfo->io_va = info.io_va;
            uinfo->size = info.size;
            infret = sizeof(struct hax_tunnel_info);
            break;
        }
        case HAX_VCPU_IOCTL_SET_MSRS: {
            struct hax_msr_data *msrs, *outmsrs;
            struct vmx_msr *msr;
            int i, fail, nr_msr_length;

            if (inBufLength < sizeof(struct hax_msr_data)||
                outBufLength < sizeof(struct hax_msr_data)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            msrs = (struct hax_msr_data *)inBuf;
            outmsrs = (struct hax_msr_data *)outBuf;
            msr = msrs->entries;

            /*nr_msr need to verified*/
            nr_msr_length = inBufLength/sizeof(struct vmx_msr);
            if((msrs->nr_msr >= nr_msr_length) || (msrs->nr_msr >=0x20)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }

            for (i = 0; i < msrs->nr_msr; i++, msr++) {
                fail = vcpu_set_msr(cvcpu, msr->entry, msr->value);
                if (fail) {
                    //  hax_log(HAX_LOGE,
                    //          "Failed to set msr  %x index %x\n",
                    //          msr->entry, i);
                    break;
                }
            }
            outmsrs->done = i;
            infret = sizeof(struct hax_msr_data);
            break;
        }
        case HAX_VCPU_IOCTL_GET_MSRS: {
            struct hax_msr_data *msrs;
            struct vmx_msr *msr;
            int i, fail, nr_msr_length;

            if (outBufLength < sizeof(struct hax_msr_data)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            msrs = (struct hax_msr_data *)outBuf;
            msr = msrs->entries;

            // nr_msr need to verified
            nr_msr_length = outBufLength/sizeof(struct vmx_msr);
            if((msrs->nr_msr >= nr_msr_length) || (msrs->nr_msr >=0x20)) {
            ret = STATUS_INVALID_PARAMETER;
                    goto done;
            }

            for (i = 0; i < msrs->nr_msr; i++, msr++) {
                fail = vcpu_get_msr(cvcpu, msr->entry, &msr->value);
                if (fail) {
                    //  printf("Failed to get msr %x index %x\n",
                    //         msr->entry, i);
                    break;
                }
            }
            msrs->done = i;
            infret = sizeof(struct hax_msr_data);
            break;
        }
        case HAX_VCPU_IOCTL_SET_FPU: {
            struct fx_layout *fl;
            if (inBufLength < sizeof(struct fx_layout)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            fl = (struct fx_layout *)inBuf;
            // vcpu_put_fpu() cannot fail
            vcpu_put_fpu(cvcpu, fl);
            break;
        }
        case HAX_VCPU_IOCTL_GET_FPU: {
            struct fx_layout *fl;
            if (outBufLength < sizeof(struct fx_layout)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            fl = (struct fx_layout *)outBuf;
            // vcpu_get_fpu() cannot fail
            vcpu_get_fpu(cvcpu, fl);
            infret = sizeof(struct fx_layout);
            break;
        }
        case HAX_VCPU_SET_REGS: {
            struct vcpu_state_t *vc_state;
            if(inBufLength < sizeof(struct vcpu_state_t)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            vc_state = (struct vcpu_state_t *)inBuf;
            if (vcpu_set_regs(cvcpu, vc_state))
                ret = STATUS_UNSUCCESSFUL;
            break;
        }
        case HAX_VCPU_GET_REGS: {
            struct vcpu_state_t *vc_state;
            if(outBufLength < sizeof(struct vcpu_state_t)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;

            }
            vc_state = (struct vcpu_state_t *)outBuf;
            // vcpu_get_regs() cannot fail
            vcpu_get_regs(cvcpu, vc_state);
            infret = sizeof(struct vcpu_state_t);
            break;
        }
        case HAX_VCPU_IOCTL_INTERRUPT: {
            uint8_t vector;
            if (inBufLength < sizeof(uint32_t)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            vector = (uint8_t) (*(uint32_t *) inBuf);
            vcpu_interrupt(cvcpu, vector);
            break;
        }
        case HAX_VCPU_IOCTL_KICKOFF: {
            vcpu_takeoff(cvcpu);
            break;
        }
        case HAX_IOCTL_VCPU_DEBUG: {
            if (inBufLength < sizeof(struct hax_debug_t)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            vcpu_debug(cvcpu, (struct hax_debug_t*)inBuf);
            break;
        }
        case HAX_VCPU_IOCTL_SET_CPUID: {
            hax_cpuid *cpuid = (hax_cpuid *)inBuf;
            if (inBufLength < sizeof(hax_cpuid) || inBufLength <
                    sizeof(hax_cpuid) + cpuid->total *
                    sizeof(hax_cpuid_entry)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            if (vcpu_set_cpuid(cvcpu, cpuid)) {
                ret = STATUS_UNSUCCESSFUL;
            }
            break;
        }
        default:
            hax_log(HAX_LOGE, "Unknow vcpu ioctl %lx\n",
                    irpSp->Parameters.DeviceIoControl.IoControlCode);
            hax_log(HAX_LOGI, "set regs ioctl %lx get regs %lx",
                    HAX_VCPU_SET_REGS, HAX_VCPU_GET_REGS );
            ret = STATUS_INVALID_PARAMETER;
            break;
    }

done:
    Irp->IoStatus.Status = ret;
    Irp->IoStatus.Information = infret;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    if (cvcpu)
        hax_put_vcpu(cvcpu);
    return ret;
}

NTSTATUS HaxVmControl(PDEVICE_OBJECT DeviceObject, struct hax_vm_windows *ext,
                      PIRP Irp)
{
    NTSTATUS ret = STATUS_SUCCESS;
    ULONG               inBufLength; // Input buffer length
    ULONG               outBufLength; // Output buffer length
    PCHAR               inBuf, outBuf; // pointer to Input and output buffer
    uint32_t vcpu_id, vm_id;
    struct vcpu_t *cvcpu;
    struct vm_t *cvm;
    struct hax_vm_windows *vm = ext;
    int infret = 0;
    PIO_STACK_LOCATION  irpSp;  // Pointer to current stack location

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    inBuf = Irp->AssociatedIrp.SystemBuffer;
    outBuf = Irp->AssociatedIrp.SystemBuffer;

    /* CVM should be always valid */
    cvm = hax_get_vm(ext->vm_id, 1);
    if (!cvm) {
            ret = STATUS_INVALID_HANDLE;
            goto done;
    }

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
        case HAX_VM_IOCTL_VCPU_CREATE: {
            if (inBufLength < sizeof(uint32_t)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            vcpu_id = *((uint32_t *)inBuf);
            vm_id = vm->vm_id;
            cvcpu = vcpu_create(cvm, vm, vcpu_id);
            if (!cvcpu) {
                hax_log(HAX_LOGI, "Failed to create vcpu %x on vm %x\n",
                        vcpu_id, vm_id);
                ret = STATUS_UNSUCCESSFUL;
                goto done;
            }
            ret = STATUS_SUCCESS;
            break;
        }
        case HAX_VM_IOCTL_ALLOC_RAM: {
            struct hax_alloc_ram_info *info;
            if (inBufLength < sizeof(struct hax_alloc_ram_info)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            info = (struct hax_alloc_ram_info *)inBuf;
            hax_log(HAX_LOGI, "IOCTL_ALLOC_RAM: vm_id=%d, va=0x%llx, size=0x%x,"
                    " pad=0x%x\n", vm->vm_id, info->va, info->size, info->pad);
            if (hax_vm_add_ramblock(cvm, info->va, info->size)) {
                ret = STATUS_UNSUCCESSFUL;
            }
            break;
        }
        case HAX_VM_IOCTL_ADD_RAMBLOCK: {
            struct hax_ramblock_info *info;
            if (inBufLength < sizeof(struct hax_ramblock_info)) {
                hax_log(HAX_LOGE, "IOCTL_ADD_RAMBLOCK: inBufLength=%u < %u\n",
                        inBufLength, sizeof(struct hax_ramblock_info));
                ret = STATUS_INVALID_PARAMETER;
                break;
            }
            info = (struct hax_ramblock_info *)inBuf;
            if (info->reserved) {
                hax_log(HAX_LOGE, "IOCTL_ADD_RAMBLOCK: vm_id=%d, "
                        "reserved=0x%llx\n", vm->vm_id, info->reserved);
                ret = STATUS_INVALID_PARAMETER;
                break;
            }
            hax_log(HAX_LOGI, "IOCTL_ADD_RAMBLOCK: vm_id=%d, start_va=0x%llx,"
                    " size=0x%llx\n", vm->vm_id, info->start_va, info->size);
            if (hax_vm_add_ramblock(cvm, info->start_va, info->size)) {
                ret = STATUS_UNSUCCESSFUL;
            }
            break;
        }
        case HAX_VM_IOCTL_SET_RAM: {
            struct hax_set_ram_info *info;
            int res;
            if (inBufLength < sizeof(struct hax_set_ram_info)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            info = (struct hax_set_ram_info *)inBuf;
            if (!(info->flags & HAX_RAM_INFO_INVALID)
                && !hax_valid_uva(info->va, info->size)) {
                ret = STATUS_INVALID_PARAMETER;
                break;
            }
            res = hax_vm_set_ram(cvm, info);
            if (res) {
                ret = res == -EINVAL ? STATUS_INVALID_PARAMETER
                      : STATUS_UNSUCCESSFUL;
            }
            break;
        }
        case HAX_VM_IOCTL_SET_RAM2: {
            struct hax_set_ram_info2 *info;
            int res;
            if (inBufLength < sizeof(struct hax_set_ram_info2)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            info = (struct hax_set_ram_info2 *)inBuf;
            if (info->reserved1 || info->reserved2) {
                hax_log(HAX_LOGE, "IOCTL_SET_RAM2: vm_id=%d, reserved1=0x%x"
                        " reserved2=0xllx\n",
                        vm->vm_id, info->reserved1, info->reserved2);
                ret = STATUS_INVALID_PARAMETER;
                break;
            }
            if (!(info->flags & HAX_RAM_INFO_INVALID)
                && !hax_valid_uva(info->va, info->size)) {
                ret = STATUS_INVALID_PARAMETER;
                break;
            }
            res = hax_vm_set_ram2(cvm, info);
            if (res) {
                ret = res == -EINVAL ? STATUS_INVALID_PARAMETER
                      : STATUS_UNSUCCESSFUL;
            }
            break;
        }
        case HAX_VM_IOCTL_PROTECT_RAM: {
            struct hax_protect_ram_info *info;
            int res;
            if (inBufLength < sizeof(struct hax_protect_ram_info)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            info = (struct hax_protect_ram_info *)inBuf;
            if (info->reserved) {
                hax_log(HAX_LOGE, "IOCTL_PROTECT_RAM: vm_id=%d, "
                        "reserved=0x%x\n", vm->vm_id, info->reserved);
                ret = STATUS_INVALID_PARAMETER;
                break;
            }
            res = hax_vm_protect_ram(cvm, info);
            if (res) {
                ret = res == -EINVAL ? STATUS_INVALID_PARAMETER
                      : STATUS_UNSUCCESSFUL;
            }
            break;
        }
        case HAX_VM_IOCTL_NOTIFY_QEMU_VERSION: {
            struct hax_qemu_version *info;

            if (inBufLength < sizeof(struct hax_qemu_version)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            info = (struct hax_qemu_version *)inBuf;

            // hax_vm_set_qemuversion() cannot fail
            hax_vm_set_qemuversion(cvm, info);
            break;
        }
        default:
            ret = STATUS_INVALID_PARAMETER;
            break;
    }

done:
    Irp->IoStatus.Status = ret;
    Irp->IoStatus.Information = infret;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    if (cvm) hax_put_vm(cvm);
    return ret;
}

NTSTATUS HaxDeviceControl(PDEVICE_OBJECT DeviceObject,
                          struct hax_dev_windows *ext, PIRP Irp)
{
    NTSTATUS ret = STATUS_UNSUCCESSFUL;
    ULONG               inBufLength; // Input buffer length
    ULONG               outBufLength; // Output buffer length
    PCHAR               inBuf = NULL, outBuf = NULL;
                        // pointer to Input and output buffer
    struct vm_t *cvm;
    int vm_id;
    int infret = 0;
    PIO_STACK_LOCATION  irpSp;// Pointer to current stack location

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    inBuf = Irp->AssociatedIrp.SystemBuffer;
    outBuf = Irp->AssociatedIrp.SystemBuffer;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
        case HAX_IOCTL_VERSION:
            if (outBufLength < sizeof(struct hax_module_version)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            ret = hax_get_versioninfo(outBuf, outBufLength, NULL);
            /* I assume the outbuf and inbuf is same buffer, right? */
            infret = sizeof(struct hax_module_version);
            break;
        case HAX_IOCTL_CAPABILITY:
            if (outBufLength < sizeof(struct hax_capabilityinfo)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }

            hax_get_capability(outBuf, outBufLength, NULL);
            /* For bug 221: (uint32_t)win_refcount will be used to store
             * HaxDeviceObject->ReferenceCount, when the value > 1,
             * then means at least one avd instance is existed
             */
            ((struct hax_capabilityinfo *)outBuf)->win_refcount =
                                                HaxDeviceObject->ReferenceCount;
            infret = sizeof(struct hax_capabilityinfo);
            ret = STATUS_SUCCESS;
            break;

        case HAX_IOCTL_SET_MEMLIMIT:
            if (inBufLength < sizeof(struct hax_set_memlimit)) {
                ret = STATUS_INVALID_PARAMETER;
                goto done;
            }
            if (hax_set_memlimit(inBuf, inBufLength, NULL)) {
                ret = STATUS_UNSUCCESSFUL;
                goto done;
            }
            ret = STATUS_SUCCESS;
            break;
        case HAX_IOCTL_CREATE_VM:
            if (outBufLength < sizeof(uint32_t)) {
                Irp->IoStatus.Information = 0;
                ret = STATUS_BUFFER_TOO_SMALL;
                goto done;
            }
            cvm = hax_create_vm(&vm_id);
            if (!cvm) {
                hax_log(HAX_LOGE, "Failed to create the HAX VM\n");
                ret =  STATUS_UNSUCCESSFUL;
                break;
            }
            *((uint32_t *)outBuf) = vm_id;
            infret = sizeof(uint32_t);
            ret = STATUS_SUCCESS;
            break;
        default:
            ret = STATUS_INVALID_DEVICE_REQUEST;
            hax_log(HAX_LOGE, "Invalid hax ioctl %x\n",
                    irpSp->Parameters.DeviceIoControl.IoControlCode);
            break;
    }
done:
    Irp->IoStatus.Status = ret;
    Irp->IoStatus.Information = infret;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );
    return ret;
}

NTSTATUS HaxIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS ret = STATUS_SUCCESS;
    struct hax_dev_ext *devext;

    devext = (struct hax_dev_ext *)DeviceObject->DeviceExtension;

    switch (devext->type) {
        case HAX_DEVEXT_TYPE_UP:
            ret = HaxDeviceControl(DeviceObject, &devext->haxdev_ext, Irp);
            break;
        case HAX_DEVEXT_TYPE_VM:
            ret = HaxVmControl(DeviceObject, &devext->vmdev_ext, Irp);
            break;
        case HAX_DEVEXT_TYPE_VCPU:
            ret = HaxVcpuControl(DeviceObject, &devext->vcpudev_ext, Irp);
            break;
    }

    return ret;
}

VOID HaxUnloadDriver(__in PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING ntWin32NameString;

    RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&ntWin32NameString);
    IoDeleteDevice(HaxDeviceObject);
    hax_log(HAX_LOGI, "Unload the driver\n");
    hax_host_exit();
    write_event(HaxDriverUnloaded, DriverObject, NULL, 0);
    HaxDeviceObject = NULL;
    HaxDriverObject = NULL;
}
