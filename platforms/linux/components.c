/*
 * Copyright (c) 2018 Kryptos Logic
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

#include <linux/cred.h>
#include <linux/dm-ioctl.h>
#include <linux/miscdevice.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/fs.h>

#include "../../core/include/hax_core_interface.h"

#define HAX_VM_DEVFS_FMT    "hax_vm/vm%02d"
#define HAX_VCPU_DEVFS_FMT  "hax_vm%02d/vcpu%02d"

#define load_user_data(dest, src, body_len, body_max, arg_t, body_t)          \
        arg_t __user *from = (arg_t __user *)(*(arg_t **)(src));              \
        size_t size;                                                          \
        arg_t header;                                                         \
        (dest) = NULL;                                                        \
        if (copy_from_user(&header, from, sizeof(arg_t))) {                   \
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
        if (copy_from_user((dest), from, size)) {                             \
            hax_log(HAX_LOGE, "%s: argument read error.\n", __func__);        \
            unload_user_data(dest);                                           \
            ret = -EFAULT;                                                    \
            break;                                                            \
        }

#define unload_user_data(dest)         \
        if ((dest) != NULL)            \
            hax_vfree((dest), size);

typedef struct hax_vm_linux_t {
    struct vm_t *cvm;
    int id;
    struct miscdevice dev;
    char *devname;
} hax_vm_linux_t;

typedef struct hax_vcpu_linux_t {
    struct vcpu_t *cvcpu;
    struct hax_vm_linux_t *vm;
    int id;
    struct miscdevice dev;
    char *devname;
} hax_vcpu_linux_t;

static int hax_vm_open(struct inode *inodep, struct file *filep);
static int hax_vm_release(struct inode *inodep, struct file *filep);
static long hax_vm_ioctl(struct file *filp, unsigned int cmd,
                         unsigned long arg);

static int hax_vcpu_open(struct inode *inodep, struct file *filep);
static int hax_vcpu_release(struct inode *inodep, struct file *filep);
static long hax_vcpu_ioctl(struct file *filp, unsigned int cmd,
                           unsigned long arg);

static struct file_operations hax_vm_fops = {
    .owner          = THIS_MODULE,
    .open           = hax_vm_open,
    .release        = hax_vm_release,
    .unlocked_ioctl = hax_vm_ioctl,
    .compat_ioctl   = hax_vm_ioctl,
};

static struct file_operations hax_vcpu_fops = {
    .owner          = THIS_MODULE,
    .open           = hax_vcpu_open,
    .release        = hax_vcpu_release,
    .unlocked_ioctl = hax_vcpu_ioctl,
    .compat_ioctl   = hax_vcpu_ioctl,
};

/* Component management */

static void hax_component_perm(const char *devname, struct miscdevice *misc)
{
    int err;
    struct path path;
    struct inode *inode;
    const struct cred *cred;
    char devpath[DM_NAME_LEN];

    if (!misc || !misc->this_device)
        return;

    snprintf(devpath, sizeof(devpath), "/dev/%s", devname);
    err = kern_path(devpath, LOOKUP_FOLLOW, &path);
    if (err || !path.dentry) {
        hax_log(HAX_LOGE, "Could not obtain device inode\n");
        return;
    }
    cred = get_current_cred();
    inode = path.dentry->d_inode;
    inode->i_uid.val = cred->uid.val;
    inode->i_gid.val = cred->gid.val;
    inode->i_mode |= 0660;
}

static hax_vcpu_linux_t* hax_vcpu_create_linux(struct vcpu_t *cvcpu,
                                               hax_vm_linux_t *vm, int vcpu_id)
{
    hax_vcpu_linux_t *vcpu;

    if (!cvcpu || !vm)
        return NULL;

    vcpu = kmalloc(sizeof(hax_vcpu_linux_t), GFP_KERNEL);
    if (!vcpu)
        return NULL;

    memset(vcpu, 0, sizeof(hax_vcpu_linux_t));
    vcpu->cvcpu = cvcpu;
    vcpu->id = vcpu_id;
    vcpu->vm = vm;
    set_vcpu_host(cvcpu, vcpu);
    return vcpu;
}

static void hax_vcpu_destroy_linux(hax_vcpu_linux_t *vcpu)
{
    struct vcpu_t *cvcpu;

    if (!vcpu)
        return;

    cvcpu = vcpu->cvcpu;
    hax_vcpu_destroy_hax_tunnel(cvcpu);
    set_vcpu_host(cvcpu, NULL);
    vcpu->cvcpu = NULL;
    kfree(vcpu);
}

int hax_vcpu_create_host(struct vcpu_t *cvcpu, void *vm_host, int vm_id,
                         int vcpu_id)
{
    int err;
    hax_vcpu_linux_t *vcpu;
    hax_vm_linux_t *vm;

    vm = (hax_vm_linux_t *)vm_host;
    vcpu = hax_vcpu_create_linux(cvcpu, vm, vcpu_id);
    if (!vcpu)
        return -1;

    vcpu->devname = kmalloc(DM_NAME_LEN, GFP_KERNEL);
    snprintf(vcpu->devname, DM_NAME_LEN, HAX_VCPU_DEVFS_FMT, vm_id, vcpu_id);
    vcpu->dev.minor = MISC_DYNAMIC_MINOR;
    vcpu->dev.name = vcpu->devname;
    vcpu->dev.fops = &hax_vcpu_fops;

    err = misc_register(&vcpu->dev);
    if (err) {
        hax_log(HAX_LOGE, "Failed to register HAXM-VCPU device\n");
        hax_vcpu_destroy_linux(vcpu);
        return -1;
    }
    hax_component_perm(vcpu->devname, &vcpu->dev);
    hax_log(HAX_LOGI, "Created HAXM-VCPU device with minor=%d\n",
            vcpu->dev.minor);
    return 0;
}

int hax_vcpu_destroy_host(struct vcpu_t *cvcpu, void *vcpu_host)
{
    hax_vcpu_linux_t *vcpu;

    vcpu = (hax_vcpu_linux_t *)vcpu_host;
    misc_deregister(&vcpu->dev);
    kfree(vcpu->devname);

    hax_vcpu_destroy_linux(vcpu);
    return 0;
}

static hax_vm_linux_t *hax_vm_create_linux(struct vm_t *cvm, int vm_id)
{
    hax_vm_linux_t *vm;

    if (!cvm)
        return NULL;

    vm = kmalloc(sizeof(hax_vm_linux_t), GFP_KERNEL);
    if (!vm)
        return NULL;

    memset(vm, 0, sizeof(hax_vm_linux_t));
    vm->cvm = cvm;
    vm->id = vm_id;
    set_vm_host(cvm, vm);
    return vm;
}

static void hax_vm_destroy_linux(hax_vm_linux_t *vm)
{
    struct vm_t *cvm;

    if (!vm)
        return;

    cvm = vm->cvm;
    set_vm_host(cvm, NULL);
    vm->cvm = NULL;
    hax_vm_free_all_ram(cvm);
    kfree(vm);
}

int hax_vm_create_host(struct vm_t *cvm, int vm_id)
{
    int err;
    hax_vm_linux_t *vm;

    vm = hax_vm_create_linux(cvm, vm_id);
    if (!vm)
        return -1;

    vm->devname = kmalloc(DM_NAME_LEN, GFP_KERNEL);
    snprintf(vm->devname, DM_NAME_LEN, HAX_VM_DEVFS_FMT, vm_id);
    vm->dev.minor = MISC_DYNAMIC_MINOR;
    vm->dev.name = vm->devname;
    vm->dev.fops = &hax_vm_fops;

    err = misc_register(&vm->dev);
    if (err) {
        hax_log(HAX_LOGE, "Failed to register HAXM-VM device\n");
        hax_vm_destroy_linux(vm);
        return -1;
    }
    hax_component_perm(vm->devname, &vm->dev);
    hax_log(HAX_LOGI, "Created HAXM-VM device with minor=%d\n", vm->dev.minor);
    return 0;
}

/* When coming here, all vcpus should have been destroyed already. */
int hax_vm_destroy_host(struct vm_t *cvm, void *vm_host)
{
    hax_vm_linux_t *vm;

    vm = (hax_vm_linux_t *)vm_host;
    misc_deregister(&vm->dev);
    kfree(vm->devname);

    hax_vm_destroy_linux(vm);
    return 0;
}

/* No corresponding function in Linux side, it can be cleaned later. */
int hax_destroy_host_interface(void)
{
    return 0;
}

/* VCPU operations */

static int hax_vcpu_open(struct inode *inodep, struct file *filep)
{
    int ret;
    struct vcpu_t *cvcpu;
    struct hax_vcpu_linux_t *vcpu;
    struct miscdevice *miscdev;

    miscdev = filep->private_data;
    vcpu = container_of(miscdev, struct hax_vcpu_linux_t, dev);
    cvcpu = hax_get_vcpu(vcpu->vm->id, vcpu->id, 1);

    hax_log(HAX_LOGD, "HAX vcpu open called\n");
    if (!cvcpu)
        return -ENODEV;

    ret = hax_vcpu_core_open(cvcpu);
    if (ret)
        hax_log(HAX_LOGE, "Failed to open core vcpu\n");
    hax_put_vcpu(cvcpu);
    return ret;
}

static int hax_vcpu_release(struct inode *inodep, struct file *filep)
{
    int ret = 0;
    struct vcpu_t *cvcpu;
    struct hax_vcpu_linux_t *vcpu;
    struct miscdevice *miscdev;

    miscdev = filep->private_data;
    vcpu = container_of(miscdev, struct hax_vcpu_linux_t, dev);
    cvcpu = hax_get_vcpu(vcpu->vm->id, vcpu->id, 1);

    hax_log(HAX_LOGD, "HAX vcpu close called\n");
    if (!cvcpu) {
        hax_log(HAX_LOGE, "Failed to find the vcpu, is it closed already?\n");
        return 0;
    }

    /* put the one for vcpu create */
    hax_put_vcpu(cvcpu);
    /* put the one just held */
    hax_put_vcpu(cvcpu);
    return ret;
}

static long hax_vcpu_ioctl(struct file *filp, unsigned int cmd,
                           unsigned long arg)
{
    int ret = 0;
    void *argp = (void *)arg;
    struct vcpu_t *cvcpu;
    struct hax_vcpu_linux_t *vcpu;
    struct miscdevice *miscdev;

    miscdev = filp->private_data;
    vcpu = container_of(miscdev, struct hax_vcpu_linux_t, dev);
    cvcpu = hax_get_vcpu(vcpu->vm->id, vcpu->id, 1);
    if (!cvcpu)
        return -ENODEV;

    switch (cmd) {
    case HAX_VCPU_IOCTL_RUN:
        ret = vcpu_execute(cvcpu);
        break;
    case HAX_VCPU_IOCTL_SETUP_TUNNEL: {
        struct hax_tunnel_info info;
        ret = hax_vcpu_setup_hax_tunnel(cvcpu, &info);
        if (copy_to_user(argp, &info, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        break;
    }
    case HAX_VCPU_IOCTL_SET_MSRS: {
        struct hax_msr_data msrs;
        struct vmx_msr *msr;
        int i, fail;

        if (copy_from_user(&msrs, argp, sizeof(msrs))) {
            ret = -EFAULT;
            break;
        }
        msr = msrs.entries;
        /* nr_msr needs to be verified */
        if (msrs.nr_msr >= 0x20) {
            hax_log(HAX_LOGE, "MSRS invalid!\n");
            ret = -EFAULT;
            break;
        }
        for (i = 0; i < msrs.nr_msr; i++, msr++) {
            fail = vcpu_set_msr(cvcpu, msr->entry, msr->value);
            if (fail) {
                break;
            }
        }
        msrs.done = i;
        break;
    }
    case HAX_VCPU_IOCTL_GET_MSRS: {
        struct hax_msr_data msrs;
        struct vmx_msr *msr;
        int i, fail;

        if (copy_from_user(&msrs, argp, sizeof(msrs))) {
            ret = -EFAULT;
            break;
        }
        msr = msrs.entries;
        if(msrs.nr_msr >= 0x20) {
            hax_log(HAX_LOGE, "MSRS invalid!\n");
            ret = -EFAULT;
            break;
        }
        for (i = 0; i < msrs.nr_msr; i++, msr++) {
            fail = vcpu_get_msr(cvcpu, msr->entry, &msr->value);
            if (fail) {
                break;
            }
        }
        msrs.done = i;
        if (copy_to_user(argp, &msrs, sizeof(msrs))) {
            ret = -EFAULT;
            break;
        }
        break;
    }
    case HAX_VCPU_IOCTL_SET_FPU: {
        struct fx_layout fl;
        if (copy_from_user(&fl, argp, sizeof(fl))) {
            ret = -EFAULT;
            break;
        }
        ret = vcpu_put_fpu(cvcpu, &fl);
        break;
    }
    case HAX_VCPU_IOCTL_GET_FPU: {
        struct fx_layout fl;
        ret = vcpu_get_fpu(cvcpu, &fl);
        if (copy_to_user(argp, &fl, sizeof(fl))) {
            ret = -EFAULT;
            break;
        }
        break;
    }
    case HAX_VCPU_SET_REGS: {
        struct vcpu_state_t vc_state;
        if (copy_from_user(&vc_state, argp, sizeof(vc_state))) {
            ret = -EFAULT;
            break;
        }
        ret = vcpu_set_regs(cvcpu, &vc_state);
        break;
    }
    case HAX_VCPU_GET_REGS: {
        struct vcpu_state_t vc_state;
        ret = vcpu_get_regs(cvcpu, &vc_state);
        if (copy_to_user(argp, &vc_state, sizeof(vc_state))) {
            ret = -EFAULT;
            break;
        }
        break;
    }
    case HAX_VCPU_IOCTL_INTERRUPT: {
        uint8_t vector;
        if (copy_from_user(&vector, argp, sizeof(vector))) {
            ret = -EFAULT;
            break;
        }
        vcpu_interrupt(cvcpu, vector);
        break;
    }
    case HAX_IOCTL_VCPU_DEBUG: {
        struct hax_debug_t hax_debug;
        if (copy_from_user(&hax_debug, argp, sizeof(hax_debug))) {
            ret = -EFAULT;
            break;
        }
        vcpu_debug(cvcpu, &hax_debug);
        break;
    }
    case HAX_VCPU_IOCTL_SET_CPUID: {
        struct hax_cpuid *cpuid;
        load_user_data(cpuid, argp, total, HAX_MAX_CPUID_ENTRIES, hax_cpuid,
                       hax_cpuid_entry);
        ret = vcpu_set_cpuid(cvcpu, cpuid);
        unload_user_data(cpuid);
        break;
    }
    default:
        // TODO: Print information about the process that sent the ioctl.
        hax_log(HAX_LOGE, "Unknown VCPU IOCTL 0x%lx\n", cmd);
        ret = -ENOSYS;
        break;
    }
    hax_put_vcpu(cvcpu);
    return ret;
}

/* VM operations */

static int hax_vm_open(struct inode *inodep, struct file *filep)
{
    int ret;
    struct vm_t *cvm;
    struct hax_vm_linux_t *vm;
    struct miscdevice *miscdev;

    miscdev = filep->private_data;
    vm = container_of(miscdev, struct hax_vm_linux_t, dev);
    cvm = hax_get_vm(vm->id, 1);
    if (!cvm)
        return -ENODEV;

    ret = hax_vm_core_open(cvm);
    hax_put_vm(cvm);
    hax_log(HAX_LOGI, "Open VM\n");
    return ret;
}

static int hax_vm_release(struct inode *inodep, struct file *filep)
{
    struct vm_t *cvm;
    struct hax_vm_linux_t *vm;
    struct miscdevice *miscdev;

    miscdev = filep->private_data;
    vm = container_of(miscdev, struct hax_vm_linux_t, dev);
    cvm = hax_get_vm(vm->id, 1);

    hax_log(HAX_LOGI, "Close VM\n");
    if (cvm) {
        /* put the ref get just now */
        hax_put_vm(cvm);
        hax_put_vm(cvm);
    }
    return 0;
}

static long hax_vm_ioctl(struct file *filp, unsigned int cmd,
                         unsigned long arg)
{
    int ret = 0;
    void *argp = (void *)arg;
    struct vm_t *cvm;
    struct hax_vm_linux_t *vm;
    struct miscdevice *miscdev;

    miscdev = filp->private_data;
    vm = container_of(miscdev, struct hax_vm_linux_t, dev);
    cvm = hax_get_vm(vm->id, 1);
    if (!cvm)
        return -ENODEV;

    switch (cmd) {
    case HAX_VM_IOCTL_VCPU_CREATE:
    case HAX_VM_IOCTL_VCPU_CREATE_ORIG: {
        uint32_t vcpu_id, vm_id;
        struct vcpu_t *cvcpu;

        vm_id = vm->id;
        if (copy_from_user(&vcpu_id, argp, sizeof(vcpu_id))) {
            ret = -EFAULT;
            break;
        }
        cvcpu = vcpu_create(cvm, vm, vcpu_id);
        if (!cvcpu) {
            hax_log(HAX_LOGE, "Failed to create vcpu %x on vm %x\n",
                    vcpu_id, vm_id);
            ret = -EINVAL;
            break;
        }
        break;
    }
    case HAX_VM_IOCTL_ALLOC_RAM: {
        struct hax_alloc_ram_info info;
        if (copy_from_user(&info, argp, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        hax_log(HAX_LOGI, "IOCTL_ALLOC_RAM: vm_id=%d, va=0x%llx, size=0x%x, "
                "pad=0x%x\n", vm->id, info.va, info.size, info.pad);
        ret = hax_vm_add_ramblock(cvm, info.va, info.size);
        break;
    }
    case HAX_VM_IOCTL_ADD_RAMBLOCK: {
        struct hax_ramblock_info info;
        if (copy_from_user(&info, argp, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        if (info.reserved) {
            hax_log(HAX_LOGE, "IOCTL_ADD_RAMBLOCK: vm_id=%d, reserved=0x%llx\n",
                    vm->id, info.reserved);
            ret = -EINVAL;
            break;
        }
        hax_log(HAX_LOGI, "IOCTL_ADD_RAMBLOCK: vm_id=%d, start_va=0x%llx, "
                "size=0x%llx\n", vm->id, info.start_va, info.size);
        ret = hax_vm_add_ramblock(cvm, info.start_va, info.size);
        break;
    }
    case HAX_VM_IOCTL_SET_RAM: {
        struct hax_set_ram_info info;
        if (copy_from_user(&info, argp, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        ret = hax_vm_set_ram(cvm, &info);
        break;
    }
    case HAX_VM_IOCTL_SET_RAM2: {
        struct hax_set_ram_info2 info;
        if (copy_from_user(&info, argp, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        if (info.reserved1 || info.reserved2) {
            hax_log(HAX_LOGE, "IOCTL_SET_RAM2: vm_id=%d, reserved1=0x%x "
                    "reserved2=0x%llx\n", vm->id, info.reserved1,
                    info.reserved2);
            ret = -EINVAL;
            break;
        }
        ret = hax_vm_set_ram2(cvm, &info);
        break;
    }
    case HAX_VM_IOCTL_PROTECT_RAM: {
        struct hax_protect_ram_info info;
        if (copy_from_user(&info, argp, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        if (info.reserved) {
            hax_log(HAX_LOGE, "IOCTL_PROTECT_RAM: vm_id=%d, reserved=0x%x\n",
                    vm->id, info.reserved);
            ret = -EINVAL;
            break;
        }
        ret = hax_vm_protect_ram(cvm, &info);
        break;
    }
    case HAX_VM_IOCTL_NOTIFY_QEMU_VERSION: {
        struct hax_qemu_version info;
        if (copy_from_user(&info, argp, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        // TODO: Print information about the process that sent the ioctl.
        ret = hax_vm_set_qemuversion(cvm, &info);
        break;
    }
    default:
        // TODO: Print information about the process that sent the ioctl.
        hax_log(HAX_LOGE, "Unknown VM IOCTL 0x%lx\n", cmd);
        break;
    }
    hax_put_vm(cvm);
    return ret;
}
