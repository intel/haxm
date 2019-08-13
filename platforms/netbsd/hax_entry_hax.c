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
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <machine/specialreg.h>
#include <machine/cpufunc.h>

#include "../../include/hax.h"
#include "../../include/hax_interface.h"
#include "../../include/hax_release_ver.h"
#include "../../core/include/hax_core_interface.h"
#include "../../core/include/config.h"

dev_type_open(hax_open);
dev_type_close(hax_close);
dev_type_ioctl(hax_ioctl);

struct cdevsw hax_cdevsw = {
    .d_open = hax_open,
    .d_close = hax_close,
    .d_read = noread,
    .d_write = nowrite,
    .d_ioctl = hax_ioctl,
    .d_stop = nostop,
    .d_tty = notty,
    .d_poll = nopoll,
    .d_mmap = nommap,
    .d_kqfilter = nokqfilter,
    .d_discard = nodiscard,
    .d_flag = D_OTHER | D_MPSAFE
};

int hax_open(dev_t dev __unused, int flags __unused, int mode __unused,
             struct lwp *l __unused)
{
    hax_log(HAX_LOGI, "HAX module opened\n");
    return 0;
}

int hax_close(dev_t self __unused, int flag __unused, int mode __unused,
              struct lwp *l __unused)
{
    hax_log(HAX_LOGI, "hax_close\n");
    return 0;
}

int hax_ioctl(dev_t self __unused, u_long cmd, void *data, int flag,
              struct lwp *l)
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
        hax_get_capability(capab, sizeof(*capab), NULL);
        break;
    }
    case HAX_IOCTL_SET_MEMLIMIT: {
        struct hax_set_memlimit *memlimit;
        memlimit = (struct hax_set_memlimit *)data;
        ret = hax_set_memlimit(memlimit, sizeof(*memlimit), NULL);
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
    default:
        hax_log(HAX_LOGE, "Unknown ioctl %#lx, pid=%d ('%s')\n", cmd,
                l->l_proc->p_pid, l->l_proc->p_comm);
        ret = -ENOSYS;
        break;
    }
    return ret;
}
