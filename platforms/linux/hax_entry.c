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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include "../../include/hax.h"
#include "../../include/hax_interface.h"
#include "../../include/hax_release_ver.h"
#include "../../core/include/hax_core_interface.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Kryptos Logic");
MODULE_DESCRIPTION("Hypervisor that provides x86 virtualization on Intel VT-x compatible CPUs.");
MODULE_VERSION(HAXM_RELEASE_VERSION_STR);

#define HAX_DEVICE_NAME "HAX"

static long hax_dev_ioctl(struct file *filp, unsigned int cmd,
                          unsigned long arg);

static struct file_operations hax_dev_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = hax_dev_ioctl,
    .compat_ioctl   = hax_dev_ioctl,
};

static struct miscdevice hax_dev = {
    MISC_DYNAMIC_MINOR,
    HAX_DEVICE_NAME,
    &hax_dev_fops,
};

static long hax_dev_ioctl(struct file *filp, unsigned int cmd,
                          unsigned long arg)
{
    int ret = 0;
    void *argp = (void *)arg;

    switch (cmd) {
    case HAX_IOCTL_VERSION: {
        struct hax_module_version version = {};
        version.cur_version = HAX_CUR_VERSION;
        version.compat_version = HAX_COMPAT_VERSION;
        if (copy_to_user(argp, &version, sizeof(version)))
            return -EFAULT;
        break;
    }
    case HAX_IOCTL_CAPABILITY: {
        struct hax_capabilityinfo capab = {};
        hax_get_capability(&capab, sizeof(capab), NULL);
        if (copy_to_user(argp, &capab, sizeof(capab)))
            return -EFAULT;
        break;
    }
    case HAX_IOCTL_SET_MEMLIMIT: {
        struct hax_set_memlimit memlimit = {};
        if (copy_from_user(&memlimit, argp, sizeof(memlimit)))
            return -EFAULT;
        ret = hax_set_memlimit(&memlimit, sizeof(memlimit), NULL);
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

        if (copy_to_user(argp, &vm_id, sizeof(vm_id)))
            return -EFAULT;
        break;
    }
    default:
        break;
    }
    return ret;
}

static int __init hax_driver_init(void)
{
    int err;

    err = cpu_info_init();
    if (err) {
        hax_log(HAX_LOGE, "Failed to initialize CPU info\n");
        return err;
    }

    if (hax_module_init() < 0) {
        hax_log(HAX_LOGE, "Failed to initialize HAXM module\n");
        cpu_info_exit();
        return -EAGAIN;
    }

    err = misc_register(&hax_dev);
    if (err) {
        hax_log(HAX_LOGE, "Failed to register HAXM device\n");
        cpu_info_exit();
        hax_module_exit();
        return err;
    }

    hax_log(HAX_LOGI, "Created HAXM device with minor=%d\n", hax_dev.minor);
    return 0;
}

static void __exit hax_driver_exit(void)
{
    if (hax_module_exit() < 0) {
        hax_log(HAX_LOGE, "Failed to finalize HAXM module\n");
    }

    misc_deregister(&hax_dev);
    hax_log(HAX_LOGI, "Removed HAXM device\n");
}

module_init(hax_driver_init);
module_exit(hax_driver_exit);
