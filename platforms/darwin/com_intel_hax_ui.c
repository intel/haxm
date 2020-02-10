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

#include "com_intel_hax.h"

#include <libkern/version.h>
#include <sys/proc.h>
#include <sys/ttycom.h>

/* Major version number of Darwin/XNU kernel */
extern const int version_major;
static int hax_vcpu_major = 0;

/* MAXCOMLEN + 1 == 17 (see bsd/sys/param.h) */
#define TASK_NAME_LEN 17

/*
 * A tricky point of the vcpu/vm reference count:
 * There is no explicitly vcpu/vm destroy from QEMU, except when failure at
 * creating the vcpu/vm. So the vcpu is destroyed when the vcpu's devfs is
 * closed, which will be done automatically when QEMU quits.
 * The reference count is managed in this way:
 * 1) When vcpu/vm is created, a reference count 1. For each vcpu, a VM
 *    reference count is added
 * 2) When vcpu/vm's devfs is opened, there is no reference count added
 * 3) Whenever access the vcpu, a reference count is needed, and reference count
 *    is released when access done
 * 4) When the devfs is closed, the reference count is decreased. For vcpu, the
 *    vm reference count is decreased
 */

/*
 * MAC driver's QEMU interface is based on device id, VM device's minor id is
 * created by Darwin part, which is nothing about hax core's vm_id (maybe it can
 * be same?).
 * Current mechanism is, minor_id = (((vm's mid + 1) << 12) | vcpu's vcpu id),
 * where vcpu id is same as hax core's vcpu_id. This limits the vm number but
 * should be OK for our purpose.
 * Notice: It's bad to allocate major number for each VM, although that will
 * make vcpu minor_id management easier, since the cdevsw is a fixed size array
 * in mac, and is very limited
 */

/* Translate the vcpu device's device id to vm id */
#define minor2vcpuvmmid(dev)  (((minor(dev) >> 12) & 0xfff) - 1)
/* translate the vcpu device's device id to vcpu id */
#define minor2vcpuid(dev) (minor(dev) & 0xfff)

#define HAX_VCPU_DEVFS_FMT_COMPAT "hax_vm%02d*/vcpu%02d"
#define HAX_VCPU_DEVFS_FMT        "hax_vm%02d/vcpu%02d"

#define HAX_VM_DEVFS_FMT_COMPAT   "hax_vm*/vm%02d"
#define HAX_VM_DEVFS_FMT          "hax_vm/vm%02d"

#define load_user_data(dest, src, body_len, body_max, arg_t, body_t)          \
        user_addr_t uaddr = (user_addr_t)(*(arg_t **)(src));                  \
        size_t size;                                                          \
        arg_t header;                                                         \
        (dest) = NULL;                                                        \
        if (copyin(uaddr, &header, sizeof(arg_t))) {                          \
            hax_log(HAX_LOGE, "%s: argument header read error.\n", __func__); \
            ret = -EFAULT;                                                    \
            break;                                                            \
        }                                                                     \
        if (header.body_len > (body_max)) {                                   \
            hax_log(HAX_LOGW, "%s: %d exceeds argument body maximum %d.\n",   \
                    __func__, header.body_len, (body_max));                   \
            ret = -E2BIG;                                                     \
            break;                                                            \
        }                                                                     \
        size = sizeof(arg_t) + header.body_len * sizeof(body_t);              \
        (dest) = hax_vmalloc(size, HAX_MEM_NONPAGE);                          \
        if ((dest) == NULL) {                                                 \
            hax_log(HAX_LOGE, "%s: failed to allocate memory.\n", __func__);  \
            ret = -ENOMEM;                                                    \
            break;                                                            \
        }                                                                     \
        if (copyin(uaddr, (dest), size)) {                                    \
            hax_log(HAX_LOGE, "%s: argument read error.\n", __func__);        \
            unload_user_data(dest);                                           \
            ret = -EFAULT;                                                    \
            break;                                                            \
        }

#define unload_user_data(dest)       \
        if ((dest) != NULL)          \
            hax_vfree((dest), size);

static void handle_unknown_ioctl(dev_t dev, ulong cmd, struct proc *p);

static struct vcpu_t * get_vcpu_by_dev(dev_t dev) {
    int vm_id = minor2vcpuvmmid(dev);
    int vcpu_id = minor2vcpuid(dev);

    return hax_get_vcpu(vm_id, vcpu_id, 1);
}

static int hax_vcpu_open(dev_t dev, int flags, __unused int devtype,
                         __unused struct proc *p)
{
    struct vcpu_t *cvcpu;
    int ret;

    hax_log(HAX_LOGD, "HAX vcpu open called\n");
    cvcpu = get_vcpu_by_dev(dev);
    if (!cvcpu)
        return -ENODEV;

    ret = hax_vcpu_core_open(cvcpu);
    if (ret)
        hax_log(HAX_LOGE, "Failed to open core vcpu\n");
    hax_put_vcpu(cvcpu);
    return ret;
}

static int hax_vcpu_close(dev_t dev, int flags, __unused int devtype,
                          __unused struct proc *p)
{
    int ret = 0;
    struct vcpu_t *cvcpu;
    hax_log(HAX_LOGD, "HAX vcpu close called\n");

    cvcpu = get_vcpu_by_dev(dev);

    if (!cvcpu) {
        hax_log(HAX_LOGE, "Failed to find the vcpu, is it closed already? \n");
        return 0;
    }

    /* put the one for vcpu create */
    hax_put_vcpu(cvcpu);
    /* put the one just held */
    hax_put_vcpu(cvcpu);

    return ret;
}

static int hax_vcpu_ioctl(dev_t dev, ulong cmd, caddr_t data, int flag,
                          struct proc *p)
{
    struct hax_vcpu_mac *vcpu;
    struct vcpu_t *cvcpu;
    int ret = 0;

    cvcpu = get_vcpu_by_dev(dev);
    if (!cvcpu)
        return -ENODEV;

    vcpu = (struct hax_vcpu_mac *)get_vcpu_host(cvcpu);
    if (vcpu == NULL) {
        hax_put_vcpu(cvcpu);
        return -ENODEV;
    }

    switch (cmd) {
        case HAX_VCPU_IOCTL_RUN: {
            ret = vcpu_execute(cvcpu);
            break;
        }
        case HAX_VCPU_IOCTL_SETUP_TUNNEL: {
            struct hax_tunnel_info info, *uinfo;
            uinfo = (struct hax_tunnel_info *)data;
            ret = hax_vcpu_setup_hax_tunnel(cvcpu, &info);
            uinfo->va = info.va;
            uinfo->io_va = info.io_va;
            uinfo->size = info.size;
            break;
        }
        case HAX_VCPU_IOCTL_SET_MSRS: {
            struct hax_msr_data *msrs;
            struct vmx_msr *msr;
            int i, fail;

            msrs = (struct hax_msr_data *)data;
            msr = msrs->entries;
            /* nr_msr needs to be verified */
            if (msrs->nr_msr >= 0x20) {
                hax_log(HAX_LOGE, "MSRS invalid!\n");
                ret = -EFAULT;
                break;
            }
            for (i = 0; i < msrs->nr_msr; i++, msr++) {
                fail = vcpu_set_msr(mvcpu2cvcpu(vcpu), msr->entry, msr->value);
                if (fail) {
                    // hax_log(HAX_LOGE,
                    //         "Failed to set msr %x index %x\n",
                    //         msr->entry, i);
                    break;
                }
            }
            msrs->done = i;
            break;
        }
        case HAX_VCPU_IOCTL_GET_MSRS: {
            struct hax_msr_data *msrs;
            struct vmx_msr *msr;
            int i, fail;

            msrs = (struct hax_msr_data *)data;
            msr = msrs->entries;
            if(msrs->nr_msr >= 0x20) {
                hax_log(HAX_LOGE, "MSRS invalid!\n");
                ret = -EFAULT;
                break;
            }

            for (i = 0; i < msrs->nr_msr; i++, msr++) {
                fail = vcpu_get_msr(mvcpu2cvcpu(vcpu), msr->entry, &msr->value);
                if (fail) {
                    // printf("Failed to get msr %x index %x\n", msr->entry, i);
                    break;
                }
            }
            msrs->done = i;
            break;
        }
        case HAX_VCPU_IOCTL_SET_FPU: {
            struct fx_layout *fl;
            fl = (struct fx_layout *)data;
            ret = vcpu_put_fpu(mvcpu2cvcpu(vcpu), fl);
            break;
        }
        case HAX_VCPU_IOCTL_GET_FPU: {
            struct fx_layout *fl;
            fl = (struct fx_layout *)data;
            ret = vcpu_get_fpu(mvcpu2cvcpu(vcpu), fl);
            break;
        }
        case HAX_VCPU_SET_REGS: {
            struct vcpu_state_t *vc_state;
            vc_state = (struct vcpu_state_t *)data;
            ret = vcpu_set_regs(mvcpu2cvcpu(vcpu), vc_state);
            break;
        }
        case HAX_VCPU_GET_REGS: {
            struct vcpu_state_t *vc_state;
            vc_state = (struct vcpu_state_t *)data;
            ret = vcpu_get_regs(mvcpu2cvcpu(vcpu), vc_state);
            break;
        }
        case HAX_VCPU_IOCTL_INTERRUPT: {
            uint8_t vector;
            vector = (uint8_t)(*(uint32_t *)data);
            vcpu_interrupt(mvcpu2cvcpu(vcpu), vector);
            break;
        }
        case HAX_IOCTL_VCPU_DEBUG: {
            struct hax_debug_t *hax_debug;
            hax_debug = (struct hax_debug_t *)data;
            vcpu_debug(cvcpu, hax_debug);
            break;
        }
        case HAX_VCPU_IOCTL_SET_CPUID: {
            struct hax_cpuid *cpuid;
            load_user_data(cpuid, data, total, HAX_MAX_CPUID_ENTRIES, hax_cpuid,
                           hax_cpuid_entry);
            ret = vcpu_set_cpuid(cvcpu, cpuid);
            unload_user_data(cpuid);
            break;
        }
        default: {
            handle_unknown_ioctl(dev, cmd, p);
            ret = -ENOSYS;
            break;
        }
    }
    hax_put_vcpu(cvcpu);
    return ret;
}

static struct cdevsw hax_vcpu_devsw = {
    hax_vcpu_open, hax_vcpu_close, eno_rdwrt, eno_rdwrt, hax_vcpu_ioctl,
    eno_stop, eno_reset, NULL, eno_select, eno_mmap, eno_strat, NULL, NULL,
    D_TTY
};

static int hax_get_vcpu_mid(struct hax_vcpu_mac *vcpu)
{
    hax_assert(vcpu->vcpu_id < 0xfff);
    return (((vcpu->vm_id + 1) << 12) | vcpu->vcpu_id);
}

/* VCPU's minor id is same as vcpu id */
static void hax_put_vcpu_mid(struct hax_vcpu_mac *vcpu)
{
    return;
}

int hax_vcpu_destroy_ui(struct hax_vcpu_mac *vcpu)
{
    devfs_remove(vcpu->pnode);
    hax_put_vcpu_mid(vcpu);
    return 0;
}

int hax_vcpu_create_ui(struct hax_vcpu_mac *vcpu)
{
    /* DEVMAXPATHSIZE == 128 (see bsd/miscfs/devfs/devfsdefs.h) */
    char devfs_pathname[128];
    void *pnode;
    int minor_id;
    /* XXX add the synchronization here */

    minor_id = hax_get_vcpu_mid(vcpu);
    if (minor_id < 0) {
        hax_log(HAX_LOGE, "No vcpu minor id left\n");
        return 0;
    }

    /* See comments in hax_vm_create_ui() below */
    if (version_major <= 16) {
        snprintf(devfs_pathname, sizeof(devfs_pathname),
                 HAX_VCPU_DEVFS_FMT_COMPAT, vcpu->vm_id, vcpu->vcpu_id);
    } else {
        snprintf(devfs_pathname, sizeof(devfs_pathname), HAX_VCPU_DEVFS_FMT,
                 vcpu->vm_id, vcpu->vcpu_id);
    }
    /* Should the vcpu node in the corresponding vm directory */
    pnode = devfs_make_node(makedev(hax_vcpu_major, minor_id), DEVFS_CHAR,
                            vcpu->owner, vcpu->gowner, 0600, devfs_pathname);
    if (NULL == pnode) {
        hax_log(HAX_LOGE, "Failed to init the device, %s\n", devfs_pathname);
        hax_put_vcpu_mid(vcpu);
        return -1;
    }
    hax_log(HAX_LOGI, "%s: Created devfs node /dev/%s for vCPU #%d\n",
            __func__, devfs_pathname, vcpu->vcpu_id);
    vcpu->pnode = pnode;

    return 0;
}

static int hax_vm_major = 0;
static int hax_vm_open(dev_t dev, int flags, __unused int devtype,
                       __unused struct proc *p)
{
    struct vm_t *cvm;
    int ret;

    cvm = hax_get_vm(minor(dev), 1);
    if (!cvm)
        return -ENODEV;
    ret = hax_vm_core_open(cvm);
    hax_put_vm(cvm);
    hax_log(HAX_LOGI, "Open VM\n");
    return ret;
}

static int hax_vm_close(dev_t dev, int flags, __unused int devtype,
                        __unused struct proc *p)
{
    struct vm_t *cvm;

    cvm = hax_get_vm(minor(dev), 1);
    hax_log(HAX_LOGI, "Close VM\n");
    if (cvm) {
        /* put the ref get just now */
        hax_put_vm(cvm);
        hax_put_vm(cvm);
    }
    return 0;
}

static int hax_vm_ioctl(dev_t dev, ulong cmd, caddr_t data, int flag,
                        struct proc *p)
{
    int ret = 0;
    struct vm_t *cvm;
    struct hax_vm_mac *vm_mac;

    //printf("vm ioctl %lx\n", cmd);
    cvm = hax_get_vm(minor(dev), 1);
    if (!cvm)
        return -ENODEV;
    vm_mac = (struct hax_vm_mac *)get_vm_host(cvm);
    if (!vm_mac) {
        hax_put_vm(cvm);
        return -ENODEV;
    }

    switch (cmd) {
        case HAX_VM_IOCTL_VCPU_CREATE:
        case HAX_VM_IOCTL_VCPU_CREATE_ORIG: {
            uint32_t vcpu_id, vm_id;
            struct vcpu_t *cvcpu;

            vcpu_id = *((uint32_t *)data);
            vm_id = vm_mac->vm_id;
            cvcpu = vcpu_create(cvm, vm_mac, vcpu_id);
            if (!cvcpu) {
                hax_log(HAX_LOGE, "Failed to create vcpu %x on vm %x\n",
                        vcpu_id, vm_id);
                ret = -EINVAL;
                hax_put_vm(cvm);
                return ret;
            }
            break;
        }
        case HAX_VM_IOCTL_ALLOC_RAM: {
            struct hax_alloc_ram_info *info;
            info = (struct hax_alloc_ram_info *)data;
            hax_log(HAX_LOGI, "IOCTL_ALLOC_RAM: vm_id=%d, va=0x%llx, size=0x%x,"
                    " pad=0x%x\n", vm_mac->vm_id, info->va, info->size,
                    info->pad);
            ret = hax_vm_add_ramblock(cvm, info->va, info->size);
            break;
        }
        case HAX_VM_IOCTL_ADD_RAMBLOCK: {
            struct hax_ramblock_info *info;
            info = (struct hax_ramblock_info *)data;
            if (info->reserved) {
                hax_log(HAX_LOGE, "IOCTL_ADD_RAMBLOCK: vm_id=%d, "
                        "reserved=0x%llx\n", vm_mac->vm_id, info->reserved);
                ret = -EINVAL;
                break;
            }
            hax_log(HAX_LOGI, "IOCTL_ADD_RAMBLOCK: vm_id=%d, start_va=0x%llx,"
                    " size=0x%llx\n", vm_mac->vm_id, info->start_va,
                    info->size);
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
                hax_log(HAX_LOGE, "IOCTL_SET_RAM2: vm_id=%d, reserved1=0x%x"
                        " reserved2=0x%llx\n",
                        vm_mac->vm_id, info->reserved1, info->reserved2);
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
                hax_log(HAX_LOGE, "IOCTL_PROTECT_RAM: vm_id=%d, "
                        "reserved=0x%x\n", vm_mac->vm_id, info->reserved);
                ret = -EINVAL;
                break;
            }
            ret = hax_vm_protect_ram(cvm, info);
            break;
        }
        case HAX_VM_IOCTL_NOTIFY_QEMU_VERSION: {
            int pid;
            char task_name[TASK_NAME_LEN];
            struct hax_qemu_version *info;

            pid = proc_pid(p);
            proc_name(pid, task_name, sizeof(task_name));
            /*
             * This message is informational, but HAX_LOGW makes sure it is
             * printed by default, which helps us identify QEMU PIDs, in case
             * we ever receive unknown ioctl()s from other processes.
             */
            hax_log(HAX_LOGW, "%s: Got HAX_VM_IOCTL_NOTIFY_QEMU_VERSION, "
                    "pid=%d ('%s')\n", __func__, pid, task_name);
            info = (struct hax_qemu_version *)data;

            ret = hax_vm_set_qemuversion(cvm, info);
            break;
        }
        default: {
            handle_unknown_ioctl(dev, cmd, p);
            ret = -ENOSYS;
            break;
        }
    }

    hax_put_vm(cvm);
    return ret;
}

static struct cdevsw hax_vm_devsw = {
    hax_vm_open, hax_vm_close, eno_rdwrt, eno_rdwrt, hax_vm_ioctl, eno_stop,
    eno_reset, NULL, eno_select, eno_mmap, eno_strat, NULL, NULL, D_TTY
};

int hax_vm_create_ui(struct hax_vm_mac *vm)
{
    /* DEVMAXPATHSIZE == 128 (see bsd/miscfs/devfs/devfsdefs.h) */
    char devfs_pathname[128];
    void *pnode;

    if (version_major <= 16) {
        /* We are running on macOS 10.12 or older, whose implementation of
         * devfs_make_node() eats the last character of every subdirectory along
         * the given path, e.g. "hax_vm/vm01" would become "hax_v/vm01", hence
         * the extra '*'. See also:
         * https://lists.apple.com/archives/darwin-kernel/2007/Dec/msg00064.html
         */
        snprintf(devfs_pathname, sizeof(devfs_pathname),
                 HAX_VM_DEVFS_FMT_COMPAT, vm->vm_id);
    } else {
        /* We are running on macOS 10.13 or newer, where the above-mentioned
         * bug no longer exists.
         * TODO: This may break in the future. A better solution is to avoid
         * creating any subdirectories, but that requires userspace changes.
         */
        snprintf(devfs_pathname, sizeof(devfs_pathname), HAX_VM_DEVFS_FMT,
                 vm->vm_id);
    }
    pnode = devfs_make_node(makedev(hax_vm_major, vm->vm_id), DEVFS_CHAR,
                            vm->owner, vm->gowner, 0600, devfs_pathname);
    if (NULL == pnode) {
        hax_log(HAX_LOGE, "Failed to init the device %s\n", devfs_pathname);
        cdevsw_remove(hax_vm_major, &hax_vm_devsw);
        return -1;
    }
    hax_log(HAX_LOGI, "%s: Created devfs node /dev/%s for VM #%d\n", __func__,
            devfs_pathname, vm->vm_id);
    vm->pnode = pnode;
    return 0;
}

int hax_vm_destroy_ui(struct hax_vm_mac *vm)
{
    devfs_remove(vm->pnode);
    return 0;
}

static int hax_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag,
                     struct proc *p)
{
    int ret = 0;

    switch (cmd) {
        case HAX_IOCTL_VERSION: {
            struct hax_module_version *version;
            version = (struct hax_module_version *)data;
            version->cur_version = HAX_CUR_VERSION;
            version->compat_version = HAX_COMPAT_VERSION;
            break;
        }
        case HAX_IOCTL_CAPABILITY: {
            struct hax_capabilityinfo *capab;
            capab = (struct hax_capabilityinfo *)data;
            hax_get_capability(capab, sizeof(struct hax_capabilityinfo), NULL);
            break;
        }
        case HAX_IOCTL_SET_MEMLIMIT: {
            struct hax_set_memlimit *memlimit;
            memlimit = (struct hax_set_memlimit*)data;
            ret = hax_set_memlimit(memlimit, sizeof(struct hax_set_memlimit),
                                   NULL);
            break;
        }
        case HAX_IOCTL_CREATE_VM: {
            int vm_id;
            struct vm_t *cvm;

            cvm = hax_create_vm(&vm_id);
            if (!cvm) {
                hax_log(HAX_LOGE, "Failed to create the HAX VM\n");
                ret = -ENOMEM;
                break;
            }

            *((uint32_t *)data) = vm_id;
            break;
        }

        default: {
            handle_unknown_ioctl(dev, cmd, p);
            ret = -ENOSYS;
            break;
        }
    }
    return ret;
}

static int hax_open(dev_t dev, int flags, __unused int devtype,
                    __unused struct proc *p)
{
    hax_log(HAX_LOGI, "HAX module opened\n");
    return 0;
}

static int hax_close(__unused dev_t dev, __unused int flags,
                     __unused int devtype, __unused struct proc *p)
{
    hax_log(HAX_LOGI, "hax_close\n");
    return (0);
}

static struct cdevsw hax_devsw = {
    hax_open, hax_close, eno_rdwrt, eno_rdwrt, hax_ioctl, eno_stop, eno_reset,
    NULL, eno_select, eno_mmap, eno_strat, NULL, NULL, D_TTY,
};

static int hax_major = 0;
static void *pnode = NULL;

int com_intel_hax_init_ui(void)
{
    hax_log(HAX_LOGI, "%s: XNU version_major=%d\n", __func__, version_major);

    hax_major = cdevsw_add(-1, &hax_devsw);
    if (hax_major < 0) {
        hax_log(HAX_LOGE, "Failed to alloc major number\n");
        return -1;
    }

    pnode = devfs_make_node(makedev(hax_major, 0), DEVFS_CHAR, 0, 0, 0666,
                            "HAX", 0);

    if (NULL == pnode) {
        hax_log(HAX_LOGE, "Failed to init the device\n");
        goto error;
    }

    if (hax_vm_major <= 0) {
        hax_vm_major = cdevsw_add(-1, &hax_vm_devsw);
        if (hax_vm_major < 0) {
            hax_log(HAX_LOGE, "Failed to allocate VM major number\n");
            goto error;
        }
    }

    if (hax_vcpu_major <= 0) {
        hax_vcpu_major = cdevsw_add(-1, &hax_vcpu_devsw);
        if (hax_vcpu_major < 0) {
            hax_log(HAX_LOGE, "Failed to allocate VCPU major number\n");
            goto error;
        }
    }
    return 0;

error:
    if (hax_vcpu_major) {
        cdevsw_remove(hax_vcpu_major, &hax_vcpu_devsw);
        hax_vcpu_major = 0;
    }
    if (hax_vm_major) {
        cdevsw_remove(hax_vm_major, &hax_vm_devsw);
        hax_vm_major = 0;
    }
    if (pnode) {
        devfs_remove(pnode);
        pnode = NULL;
    }
    if (hax_major) {
        cdevsw_remove(hax_major, &hax_devsw);
        hax_major = 0;
    }
    return -1;
}

int com_intel_hax_exit_ui(void)
{
    hax_log(HAX_LOGI, "Exit hax module\n");

    if (hax_vcpu_major) {
        cdevsw_remove(hax_vcpu_major, &hax_vcpu_devsw);
        hax_vcpu_major = 0;
    }

    if (hax_vm_major) {
        cdevsw_remove(hax_vm_major, &hax_vm_devsw);
        hax_vm_major = 0;
    }

    if (pnode) {
        devfs_remove(pnode);
        pnode = NULL;
    }

    if (hax_major) {
        cdevsw_remove(hax_major, &hax_devsw);
        hax_major = 0;
    }

    return 0;
}

static void handle_unknown_ioctl(dev_t dev, ulong cmd, struct proc *p)
{
    int dev_major = major(dev);
    const char *dev_name = NULL;
    int pid;
    char task_name[TASK_NAME_LEN];

    if (cmd == TIOCSCTTY) {
        /* Because HAXM cdevsw's are created with d_type == D_TTY, the Darwin
         * kernel may send us an TIOCSCTTY ioctl while servicing an open()
         * syscall on these devices (see open1() in bsd/vfs/vfs_syscall.c).
         * Suppress the bogus warning for this ioctl to avoid confusion.
         */
        return;
    }

    if (dev_major == hax_major) {
        dev_name = "HAX";
    } else if (dev_major == hax_vm_major) {
        dev_name = "VM";
    } else if (dev_major == hax_vcpu_major) {
        dev_name = "VCPU";
    } else {
        dev_name = "??";
        hax_log(HAX_LOGE, "%s: Unknown device major %d\n", __func__, dev_major);
    }

    pid = proc_pid(p);
    proc_name(pid, task_name, sizeof(task_name));
    hax_log(HAX_LOGW, "Unknown %s ioctl 0x%lx from pid=%d ('%s')\n", dev_name,
            cmd, pid, task_name);
}
