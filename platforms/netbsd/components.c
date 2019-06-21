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
#include <sys/device.h>
#include <sys/kmem.h>

#include "../../core/include/hax_core_interface.h"
#include "../../core/include/config.h"

extern struct cfdriver hax_vm_cd;
extern struct cfdriver hax_vcpu_cd;

/* Component management */

static hax_vcpu_netbsd_t *hax_vcpu_create_netbsd(struct vcpu_t *cvcpu,
                                                 hax_vm_netbsd_t *vm,
                                                 int vcpu_id)
{
    hax_vcpu_netbsd_t *vcpu;

    if (!cvcpu || !vm)
        return NULL;

    vcpu = kmem_zalloc(sizeof(hax_vcpu_netbsd_t), KM_SLEEP);
    vcpu->cvcpu = cvcpu;
    vcpu->id = vcpu_id;
    vcpu->vm = vm;
    set_vcpu_host(cvcpu, vcpu);
    return vcpu;
}

static void hax_vcpu_destroy_netbsd(hax_vcpu_netbsd_t *vcpu)
{
    struct vcpu_t *cvcpu;

    if (!vcpu)
        return;

    cvcpu = vcpu->cvcpu;
    hax_vcpu_destroy_hax_tunnel(cvcpu);
    set_vcpu_host(cvcpu, NULL);
    vcpu->cvcpu = NULL;
    kmem_free(vcpu, sizeof(hax_vcpu_netbsd_t));
}

int hax_vcpu_create_host(struct vcpu_t *cvcpu, void *vm_host, int vm_id,
                         int vcpu_id)
{
    int err;
    hax_vcpu_netbsd_t *vcpu;
    hax_vm_netbsd_t *vm;
    struct hax_vcpu_softc *sc;
    devminor_t minor;

    minor = vmvcpu2unit(vm_id, vcpu_id);
    sc = device_lookup_private(&hax_vcpu_cd, minor);
    if (!sc) {
        hax_log(HAX_LOGE, "device lookup for hax_vcpu failed (minor %u)\n",
                minor);
        return -1;
    }

    vm = (hax_vm_netbsd_t *)vm_host;
    vcpu = hax_vcpu_create_netbsd(cvcpu, vm, vcpu_id);
    if (!vcpu)
        return -1;

    vcpu->id = vcpu_id;
    sc->vcpu = vcpu;

    hax_log(HAX_LOGI, "Created HAXM-VCPU device 'hax_vm%02d/vcpu%02d'\n",
            vm_id, vcpu_id);
    return 0;
}

int hax_vcpu_destroy_host(struct vcpu_t *cvcpu, void *vcpu_host)
{
    hax_vcpu_netbsd_t *vcpu;
    struct hax_vcpu_softc *sc;
    devminor_t minor;

    vcpu = (hax_vcpu_netbsd_t *)vcpu_host;

    minor = vmvcpu2unit(vcpu->vm->id, vcpu->id);
    sc = device_lookup_private(&hax_vcpu_cd, minor);
    if (!sc) {
        hax_log(HAX_LOGE, "device lookup for hax_vcpu failed (minor %u)\n",
                minor);
        return -1;
    }

    hax_vcpu_destroy_netbsd(vcpu);

    sc->vcpu = NULL;

    return 0;
}

static hax_vm_netbsd_t *hax_vm_create_netbsd(struct vm_t *cvm, int vm_id)
{
    hax_vm_netbsd_t *vm;

    if (!cvm)
        return NULL;

    vm = kmem_zalloc(sizeof(hax_vm_netbsd_t), KM_SLEEP);
    vm->cvm = cvm;
    vm->id = vm_id;
    set_vm_host(cvm, vm);
    return vm;
}

static void hax_vm_destroy_netbsd(hax_vm_netbsd_t *vm)
{
    struct vm_t *cvm;
    struct hax_vm_softc *sc;
    devminor_t minor;

    minor = vm->id;
    sc = device_lookup_private(&hax_vm_cd, minor);
    if (!sc) {
        hax_log(HAX_LOGE, "device lookup for hax_vm failed (minor %u)\n",
                minor);
        return;
    }

    if (!vm)
        return;

    cvm = vm->cvm;
    set_vm_host(cvm, NULL);
    vm->cvm = NULL;
    hax_vm_free_all_ram(cvm);
    kmem_free(vm, sizeof(hax_vm_netbsd_t));

    sc->vm = NULL;
}

int hax_vm_create_host(struct vm_t *cvm, int vm_id)
{
    int err;
    hax_vm_netbsd_t *vm;
    struct hax_vm_softc *sc;
    devminor_t minor;

    minor = vm_id;
    sc = device_lookup_private(&hax_vm_cd, minor);
    if (!sc) {
        hax_log(HAX_LOGE, "device lookup for hax_vm failed (minor %u)\n",
                minor);
        return -1;
    }

    vm = hax_vm_create_netbsd(cvm, vm_id);
    if (!vm)
        return -1;

    sc->vm = vm;

    hax_log(HAX_LOGI, "Created HAXM-VM device 'hax_vm/vm%02d'\n", vm_id);
    return 0;
}

/* When coming here, all vcpus should have been destroyed already. */
int hax_vm_destroy_host(struct vm_t *cvm, void *vm_host)
{
    hax_vm_netbsd_t *vm;

    vm = (hax_vm_netbsd_t *)vm_host;
    hax_vm_destroy_netbsd(vm);

    return 0;
}

/* No corresponding function in netbsd side, it can be cleaned later. */
int hax_destroy_host_interface(void)
{
    return 0;
}
