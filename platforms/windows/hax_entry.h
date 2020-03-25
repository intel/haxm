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

#ifndef HAX_WINDOWS_HAX_ENTRY_H_
#define HAX_WINDOWS_HAX_ENTRY_H_

struct hax_dev_windows {
    hax_atomic_t count;  // how many opened session
};

#define HAX_DEVEXT_TYPE_UP 0x1
#define HAX_DEVEXT_TYPE_VM 0x2
#define HAX_DEVEXT_TYPE_VCPU 0x3
struct hax_dev_ext;

struct hax_vcpu_mem;

struct hax_vm_windows {
    int vm_id;
    struct vm_t *cvm;
    PDEVICE_OBJECT ext;
    PUNICODE_STRING ssdl;
};

struct hax_vcpu_windows {
    int vm_id;
    int vcpu_id;
    struct vcpu_t *cvcpu;
    PDEVICE_OBJECT ext;
};

struct hax_dev_ext {
    int type;
    union {
        struct hax_dev_windows haxdev_ext;
        struct hax_vm_windows vmdev_ext;
        struct hax_vcpu_windows vcpudev_ext;
    };
};

static struct vcpu_t *wvcpu2cvcpu(struct hax_vcpu_windows *vcpu) {
    if (!vcpu)
        return NULL;
    return vcpu->cvcpu;
}

//#define HAX_UNIFIED_BINARY 1  /* Only two binaries for final release! */

#ifndef HAX_UNIFIED_BINARY

#if (NTDDI_VERSION >= NTDDI_WIN7)
#define MDL_HAX_PAGE 1
#endif

#if (NTDDI_VERSION < NTDDI_WS03)
#define SMPC_DPCS 1
int smpc_dpc_init(void);
int smpc_dpc_exit(void);
#else
static inline smpc_dpc_init(void) { return 1; }
static inline smpc_dpc_exit(void) { return 1; }
#endif

/* According to DDK, the IoAllocateMdl can support at most
 * 64M - PAGE_SIZE * (sizeof(MDL))/sizeof(ULONG_PTR), so take 32M here
 */
#if (NTDDI_VERSION <= NTDDI_WS03)
#define HAX_RAM_ENTRY_SIZE 0x2000000
#else
#define HAX_RAM_ENTRY_SIZE 0x4000000
#endif

#else /* HAX_UNIFIED_BINARY */

#define SMPC_DPCS 1
int smpc_dpc_init(void);
int smpc_dpc_exit(void);

#define HAX_RAM_ENTRY_SIZE 0x2000000

#endif /* HAX_UNIFIED_BINARY */


extern PDRIVER_OBJECT HaxDriverObject;

#define HAX_DEVICE_TYPE 0x4000

#define HAX_IOCTL_VERSION \
        CTL_CODE(HAX_DEVICE_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_IOCTL_CREATE_VM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_IOCTL_CAPABILITY \
        CTL_CODE(HAX_DEVICE_TYPE, 0x910, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_IOCTL_SET_MEMLIMIT \
        CTL_CODE(HAX_DEVICE_TYPE, 0x911, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VM_IOCTL_VCPU_CREATE \
        CTL_CODE(HAX_DEVICE_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_ALLOC_RAM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_SET_RAM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_VCPU_DESTROY \
        CTL_CODE(HAX_DEVICE_TYPE, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_ADD_RAMBLOCK \
        CTL_CODE(HAX_DEVICE_TYPE, 0x913, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_SET_RAM2 \
        CTL_CODE(HAX_DEVICE_TYPE, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_PROTECT_RAM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x915, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_RUN \
        CTL_CODE(HAX_DEVICE_TYPE, 0x906, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_SET_MSRS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x907, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_GET_MSRS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_SET_FPU \
        CTL_CODE(HAX_DEVICE_TYPE, 0x909, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_GET_FPU \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90a, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_SETUP_TUNNEL \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_INTERRUPT \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_SET_REGS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_GET_REGS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90e, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_KICKOFF \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90f, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* API version 2.0 */
#define HAX_VM_IOCTL_NOTIFY_QEMU_VERSION \
        CTL_CODE(HAX_DEVICE_TYPE, 0x910, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_IOCTL_VCPU_DEBUG \
        CTL_CODE(HAX_DEVICE_TYPE, 0x916, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_SET_CPUID \
        CTL_CODE(HAX_DEVICE_TYPE, 0x917, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif // HAX_WINDOWS_HAX_ENTRY_H_
