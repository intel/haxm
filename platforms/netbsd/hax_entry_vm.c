/*
 * Copyright (c) 2018 Kamil Rytarowski
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/lwp.h>
#include <sys/proc.h>

#include "../../core/include/hax_core_interface.h"

extern struct cfdriver hax_vm_cd;

dev_type_open(hax_vm_open);
dev_type_close(hax_vm_close);
dev_type_ioctl(hax_vm_ioctl);

struct cdevsw hax_vm_cdevsw = {
    .d_open = hax_vm_open,
    .d_close = hax_vm_close,
    .d_read = noread,
    .d_write = nowrite,
    .d_ioctl = hax_vm_ioctl,
    .d_stop = nostop,
    .d_tty = notty,
    .d_poll = nopoll,
    .d_mmap = nommap,
    .d_kqfilter = nokqfilter,
    .d_discard = nodiscard,
    .d_flag = D_OTHER | D_MPSAFE
};

int hax_vm_open(dev_t self, int flag __unused, int mode __unused,
                struct lwp *l __unused)
{
    struct hax_vm_softc *sc;
    struct vm_t *cvm;
    struct hax_vm_netbsd_t *vm;
    int unit;
    int ret;

    sc = device_lookup_private(&hax_vm_cd, minor(self));
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_lookup_private() for hax_vm failed\n");
        return -ENODEV;
    }

    vm = sc->vm;

    unit = device_unit(sc->sc_dev);

    if (!vm) {
        hax_log(HAX_LOGE, "HAX VM 'hax_vm/vm%02d' is not ready\n", unit);
        return -ENODEV;
    }

    hax_assert(unit == vm->id);

    cvm = hax_get_vm(vm->id, 1);
    if (!cvm)
        return -ENODEV;

    ret = hax_vm_core_open(cvm);
    hax_put_vm(cvm);
    hax_log(HAX_LOGI, "Open VM%02d\n", vm->id);
    return ret;
}

int hax_vm_close(dev_t self __unused, int flag __unused, int mode __unused,
                 struct lwp *l __unused)
{
    struct hax_vm_softc *sc;
    struct vm_t *cvm;
    struct hax_vm_netbsd_t *vm;

    sc = device_lookup_private(&hax_vm_cd, minor(self));
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_lookup_private() for hax_vm failed\n");
        return -ENODEV;
    }

    vm = sc->vm;
    cvm = hax_get_vm(vm->id, 1);

    hax_log(HAX_LOGI, "Close VM%02d\n", vm->id);
    if (cvm) {
        /* put the ref get just now */
        hax_put_vm(cvm);
        hax_put_vm(cvm);
    }
    return 0;
}

int hax_vm_ioctl(dev_t self __unused, u_long cmd, void *data, int flag,
                 struct lwp *l __unused)
{
    int ret = 0;
    struct vm_t *cvm;
    struct hax_vm_netbsd_t *vm;
    struct hax_vm_softc *sc;

    sc = device_lookup_private(&hax_vm_cd, minor(self));
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_lookup_private() for hax_vm failed\n");
        return -ENODEV;
    }
    vm = sc->vm;
    cvm = hax_get_vm(vm->id, 1);
    if (!cvm)
        return -ENODEV;

    switch (cmd) {
    case HAX_VM_IOCTL_VCPU_CREATE:
    case HAX_VM_IOCTL_VCPU_CREATE_ORIG: {
        uint32_t *vcpu_id, vm_id;
        vcpu_id = (uint32_t *)data;
        struct vcpu_t *cvcpu;

        vm_id = vm->id;
        cvcpu = vcpu_create(cvm, vm, *vcpu_id);
        if (!cvcpu) {
            hax_log(HAX_LOGE, "Failed to create vcpu %x on vm %x\n",
                    *vcpu_id, vm_id);
            ret = -EINVAL;
            break;
        }
        break;
    }
    case HAX_VM_IOCTL_ALLOC_RAM: {
        struct hax_alloc_ram_info *info;
        info = (struct hax_alloc_ram_info *)data;
        hax_log(HAX_LOGI, "IOCTL_ALLOC_RAM: vm_id=%d, va=0x%llx, size=0x%x, "
                "pad=0x%x\n", vm->id, info->va, info->size, info->pad);
        ret = hax_vm_add_ramblock(cvm, info->va, info->size);
        break;
    }
    case HAX_VM_IOCTL_ADD_RAMBLOCK: {
        struct hax_ramblock_info *info;
        info = (struct hax_ramblock_info *)data;
        if (info->reserved) {
            hax_log(HAX_LOGE, "IOCTL_ADD_RAMBLOCK: vm_id=%d, reserved=0x%llx\n",
                    vm->id, info->reserved);
            ret = -EINVAL;
            break;
        }
        hax_log(HAX_LOGI, "IOCTL_ADD_RAMBLOCK: vm_id=%d, start_va=0x%llx, "
                "size=0x%llx\n", vm->id, info->start_va, info->size);
        ret = hax_vm_add_ramblock(cvm, info->start_va, info->size);
        break;
    }
    case HAX_VM_IOCTL_SET_RAM: {
        struct hax_set_ram_info *info;
        info = (struct hax_set_ram_info *)data;
        ret = hax_vm_set_ram(cvm, info);
        break;
    }
    case HAX_VM_IOCTL_SET_RAM2: {
        struct hax_set_ram_info2 *info;
        info = (struct hax_set_ram_info2 *)data;
        if (info->reserved1 || info->reserved2) {
            hax_log(HAX_LOGE, "IOCTL_SET_RAM2: vm_id=%d, reserved1=0x%x "
                    "reserved2=0x%llx\n", vm->id, info->reserved1,
                    info->reserved2);
            ret = -EINVAL;
            break;
        }
        ret = hax_vm_set_ram2(cvm, info);
        break;
    }
    case HAX_VM_IOCTL_PROTECT_RAM: {
        struct hax_protect_ram_info *info;
        info = (struct hax_protect_ram_info *)data;
        if (info->reserved) {
            hax_log(HAX_LOGE, "IOCTL_PROTECT_RAM: vm_id=%d, reserved=0x%x\n",
                    vm->id, info->reserved);
            ret = -EINVAL;
            break;
        }
        ret = hax_vm_protect_ram(cvm, info);
        break;
    }
    case HAX_VM_IOCTL_NOTIFY_QEMU_VERSION: {
        struct hax_qemu_version *info;
        info = (struct hax_qemu_version *)data;
        // TODO: Print information about the process that sent the ioctl.
        ret = hax_vm_set_qemuversion(cvm, info);
        break;
    }
    default:
        // TODO: Print information about the process that sent the ioctl.
        hax_log(HAX_LOGE, "Unknown VM IOCTL %#lx, pid=%d ('%s')\n", cmd,
                l->l_proc->p_pid, l->l_proc->p_comm);
        break;
    }
    hax_put_vm(cvm);
    return ret;
}
