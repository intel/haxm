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

extern struct cfdriver hax_vcpu_cd;

dev_type_open(hax_vcpu_open);
dev_type_close(hax_vcpu_close);
dev_type_ioctl(hax_vcpu_ioctl);

struct cdevsw hax_vcpu_cdevsw = {
    .d_open = hax_vcpu_open,
    .d_close = hax_vcpu_close,
    .d_read = noread,
    .d_write = nowrite,
    .d_ioctl = hax_vcpu_ioctl,
    .d_stop = nostop,
    .d_tty = notty,
    .d_poll = nopoll,
    .d_mmap = nommap,
    .d_kqfilter = nokqfilter,
    .d_discard = nodiscard,
    .d_flag = D_OTHER | D_MPSAFE
};

#define load_user_data(dest, src, body_len, body_max, arg_t, body_t)          \
        void *uaddr = (void *)(*(arg_t **)(src));                             \
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
        size = sizeof(arg_t) + header.body_len * sizeof(body_t);               \
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

/* VCPU operations */

int hax_vcpu_open(dev_t self, int flag __unused, int mode __unused,
                  struct lwp *l __unused)
{
    struct hax_vcpu_softc *sc;
    struct vcpu_t *cvcpu;
    struct hax_vcpu_netbsd_t *vcpu;
    int ret;
    int unit, vm_id;
    uint32_t vcpu_id;

    sc = device_lookup_private(&hax_vcpu_cd, minor(self));
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_lookup_private() for hax_vcpu failed\n");
        return -ENODEV;
    }

    vcpu = sc->vcpu;

    unit = device_unit(sc->sc_dev);
    vm_id = unit2vmmid(unit);
    vcpu_id = unit2vcpuid(unit);

    if (!vcpu) {
        hax_log(HAX_LOGE, "HAX VCPU 'hax_vm%02d/vcpu%02d' is not ready\n",
                vm_id, vcpu_id);
        return -ENODEV;
    }

    hax_assert(vcpu->vm->id == vm_id);
    hax_assert(vcpu->id == vcpu_id);

    cvcpu = hax_get_vcpu(vcpu->vm->id, vcpu->id, 1);

    hax_log(HAX_LOGD, "HAX VM%02d vcpu%02d open called\n", vcpu->vm->id,
            vcpu->id);
    if (!cvcpu)
        return -ENODEV;

    ret = hax_vcpu_core_open(cvcpu);
    if (ret)
        hax_log(HAX_LOGE, "Failed to open core vcpu\n");
    hax_put_vcpu(cvcpu);
    return ret;
}

int hax_vcpu_close(dev_t self, int flag __unused, int mode __unused,
                   struct lwp *l __unused)
{
    struct hax_vcpu_softc *sc;
    struct vcpu_t *cvcpu;
    struct hax_vcpu_netbsd_t *vcpu;

    sc = device_lookup_private(&hax_vcpu_cd, minor(self));
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_lookup_private() for hax_vcpu failed\n");
        return -ENODEV;
    }
    vcpu = sc->vcpu;
    cvcpu = hax_get_vcpu(vcpu->vm->id, vcpu->id, 1);

    hax_log(HAX_LOGD, "HAX VM%02d vcpu%02d close called\n", vcpu->vm->id,
            vcpu->id);
    if (!cvcpu) {
        hax_log(HAX_LOGE, "Failed to find the vcpu, is it closed already?\n");
        return 0;
    }

    /* put the one for vcpu create */
    hax_put_vcpu(cvcpu);
    /* put the one just held */
    hax_put_vcpu(cvcpu);

    return 0;
}

int hax_vcpu_ioctl(dev_t self, u_long cmd, void *data, int flag,
                   struct lwp *l __unused)
{
    int ret = 0;
    struct hax_vcpu_softc *sc;
    struct vcpu_t *cvcpu;
    struct hax_vcpu_netbsd_t *vcpu;

    sc = device_lookup_private(&hax_vcpu_cd, minor(self));
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_lookup_private() for hax_vcpu failed\n");
        return -ENODEV;
    }
    vcpu = sc->vcpu;
    cvcpu = hax_get_vcpu(vcpu->vm->id, vcpu->id, 1);

    if (!cvcpu)
        return -ENODEV;

    switch (cmd) {
    case HAX_VCPU_IOCTL_RUN:
        ret = vcpu_execute(cvcpu);
        break;
    case HAX_VCPU_IOCTL_SETUP_TUNNEL: {
        struct hax_tunnel_info *info;
        info = (struct hax_tunnel_info *)data;
        ret = hax_vcpu_setup_hax_tunnel(cvcpu, info);
        break;
    }
    case HAX_VCPU_IOCTL_SET_MSRS: {
        struct hax_msr_data *msrs;
        msrs = (struct hax_msr_data *)data;
        struct vmx_msr *msr;
        int i, fail;

        msr = msrs->entries;
        /* nr_msr needs to be verified */
        if (msrs->nr_msr >= 0x20) {
            hax_log(HAX_LOGE, "MSRS invalid!\n");
            ret = -EFAULT;
            break;
        }
        for (i = 0; i < msrs->nr_msr; i++, msr++) {
            fail = vcpu_set_msr(cvcpu, msr->entry, msr->value);
            if (fail) {
                break;
            }
        }
        msrs->done = i;
        break;
    }
    case HAX_VCPU_IOCTL_GET_MSRS: {
        struct hax_msr_data *msrs;
        msrs = (struct hax_msr_data *)data;
        struct vmx_msr *msr;
        int i, fail;

        msr = msrs->entries;
        if(msrs->nr_msr >= 0x20) {
            hax_log(HAX_LOGE, "MSRS invalid!\n");
            ret = -EFAULT;
            break;
        }
        for (i = 0; i < msrs->nr_msr; i++, msr++) {
            fail = vcpu_get_msr(cvcpu, msr->entry, &msr->value);
            if (fail) {
                break;
            }
        }
        msrs->done = i;
        break;
    }
    case HAX_VCPU_IOCTL_SET_FPU: {
        struct fx_layout *fl;
        fl = (struct fx_layout *)data;
        ret = vcpu_put_fpu(cvcpu, fl);
        break;
    }
    case HAX_VCPU_IOCTL_GET_FPU: {
        struct fx_layout *fl;
        fl = (struct fx_layout *)data;
        ret = vcpu_get_fpu(cvcpu, fl);
        break;
    }
    case HAX_VCPU_SET_REGS: {
        struct vcpu_state_t *vc_state;
        vc_state = (struct vcpu_state_t *)data;
        ret = vcpu_set_regs(cvcpu, vc_state);
        break;
    }
    case HAX_VCPU_GET_REGS: {
        struct vcpu_state_t *vc_state;
        vc_state = (struct vcpu_state_t *)data;
        ret = vcpu_get_regs(cvcpu, vc_state);
        break;
    }
    case HAX_VCPU_IOCTL_INTERRUPT: {
        uint8_t *vector;
        vector = (uint8_t *)data;
        vcpu_interrupt(cvcpu, *vector);
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
    default:
        // TODO: Print information about the process that sent the ioctl.
        hax_log(HAX_LOGE, "Unknown VCPU IOCTL %#lx, pid=%d ('%s')\n", cmd,
                l->l_proc->p_pid, l->l_proc->p_comm);
        ret = -ENOSYS;
        break;
    }
    hax_put_vcpu(cvcpu);
    return ret;
}
