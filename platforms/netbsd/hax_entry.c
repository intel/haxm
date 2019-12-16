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
#include <sys/module.h>
#include <sys/cpu.h>

#include "../../core/include/config.h"
#include "../../core/include/hax_core_interface.h"
#include "../../include/hax.h"
#include "../../include/hax_interface.h"

#define HAX_DEVICE_NAME "HAX"
#define HAX_VM_DEVICE_NAME "hax_vm"
#define HAX_VCPU_DEVICE_NAME "hax_vcpu"

static int hax_cmajor = 348, hax_bmajor = -1;
static int hax_vm_cmajor = 349, hax_vm_bmajor = -1;
static int hax_vcpu_cmajor = 350, hax_vcpu_bmajor = -1;

extern struct cdevsw hax_cdevsw;
extern struct cdevsw hax_vm_cdevsw;
extern struct cdevsw hax_vcpu_cdevsw;

static int hax_vm_match(device_t, cfdata_t, void *);
static void hax_vm_attach(device_t, device_t, void *);
static int hax_vm_detach(device_t, int);

CFATTACH_DECL_NEW(hax_vm, sizeof(struct hax_vm_softc),
                  hax_vm_match, hax_vm_attach, hax_vm_detach, NULL);

static int hax_vcpu_match(device_t, cfdata_t, void *);
static void hax_vcpu_attach(device_t, device_t, void *);
static int hax_vcpu_detach(device_t, int);

CFATTACH_DECL_NEW(hax_vcpu, sizeof(struct hax_vcpu_softc),
                  hax_vcpu_match, hax_vcpu_attach, hax_vcpu_detach, NULL);

static int
hax_vm_match(device_t parent, cfdata_t match, void *aux)
{
    return 1;
}

static void
hax_vm_attach(device_t parent, device_t self, void *aux)
{
    struct hax_vm_softc *sc;
    int unit;

    sc = device_private(self);
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_private() for hax_vm failed\n");
        return;
    }

    unit = device_unit(self);

    sc->sc_dev = self;
    sc->vm = NULL;

    snprintf(self->dv_xname, sizeof self->dv_xname, "hax_vm/vm%02d", unit);

    if (!pmf_device_register(self, NULL, NULL))
        aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
hax_vm_detach(device_t self, int flags)
{
    struct hax_vm_softc *sc;

    sc = device_private(self);
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_private() for hax_vm failed\n");
        return -ENODEV;
    }
    pmf_device_deregister(self);

    return 0;
}

static int
hax_vcpu_match(device_t parent, cfdata_t match, void *aux)
{
    return 1;
}

static void
hax_vcpu_attach(device_t parent, device_t self, void *aux)
{
    struct hax_vcpu_softc *sc;
    int unit, vm_id;
    uint32_t vcpu_id;

    sc = device_private(self);
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_private() for hax_vcpu failed\n");
        return;
    }

    unit = device_unit(self);
    vm_id = unit2vmmid(unit);
    vcpu_id = unit2vcpuid(unit);

    sc->sc_dev = self;
    sc->vcpu = NULL;

    snprintf(self->dv_xname, sizeof self->dv_xname, "hax_vm%02d/vcpu%02d",
             vm_id, vcpu_id);

    if (!pmf_device_register(self, NULL, NULL))
        aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
hax_vcpu_detach(device_t self, int flags)
{
    struct hax_vcpu_softc *sc;

    sc = device_private(self);
    if (sc == NULL) {
        hax_log(HAX_LOGE, "device_private() for hax_vcpu failed\n");
        return -ENODEV;
    }
    pmf_device_deregister(self);

    return 0;
}

static const struct cfiattrdata haxbus_iattrdata = {
    "haxbus", 0, { { NULL, NULL, 0 },}
};

static const struct cfiattrdata *const hax_vm_attrs[] = {
    &haxbus_iattrdata, NULL
};

CFDRIVER_DECL(hax_vm, DV_DULL, hax_vm_attrs);
extern struct cfattach hax_vm_ca;
static int hax_vmloc[] = {
    -1,
    -1,
    -1
};

static struct cfdata hax_vm_cfdata[] = {
    {
        .cf_name = "hax_vm",
        .cf_atname = "hax_vm",
        .cf_unit = 0,
        .cf_fstate = FSTATE_STAR,
        .cf_loc = hax_vmloc,
        .cf_flags = 0,
        .cf_pspec = NULL,
    },
    { NULL, NULL, 0, FSTATE_NOTFOUND, NULL, 0, NULL }
};

static const struct cfiattrdata *const hax_vcpu_attrs[] = {
    &haxbus_iattrdata, NULL
};

CFDRIVER_DECL(hax_vcpu, DV_DULL, hax_vcpu_attrs);
extern struct cfattach hax_vcpu_ca;
static int hax_vcpuloc[] = {
    -1,
    -1,
    -1
};

static struct cfdata hax_vcpu_cfdata[] = {
    {
        .cf_name = "hax_vcpu",
        .cf_atname = "hax_vcpu",
        .cf_unit = 0,
        .cf_fstate = FSTATE_STAR,
        .cf_loc = hax_vcpuloc,
        .cf_flags = 0,
        .cf_pspec = NULL,
    },
    { NULL, NULL, 0, FSTATE_NOTFOUND, NULL, 0, NULL }
};

MODULE(MODULE_CLASS_MISC, haxm, NULL);

static int
haxm_modcmd(modcmd_t cmd, void *arg __unused)
{
    int err;
    size_t i;

    switch (cmd) {
    case MODULE_CMD_INIT: {
        // Initialization
        err = cpu_info_init();
        if (err) {
            hax_log(HAX_LOGE, "Unable to init cpu info\n");
            goto init_err0;
        }

        // Register hax_vm
        err = config_cfdriver_attach(&hax_vm_cd);
        if (err) {
            hax_log(HAX_LOGE, "Unable to register cfdriver hax_vm\n");
            goto init_err1;
        }

        err = config_cfattach_attach(hax_vm_cd.cd_name, &hax_vm_ca);
        if (err) {
            hax_log(HAX_LOGE, "Unable to register cfattch hax_vm\n");
            goto init_err2;
        }

        err = config_cfdata_attach(hax_vm_cfdata, 1);
        if (err) {
            hax_log(HAX_LOGE, "Unable to register cfdata hax_vm\n");
            goto init_err3;
        }

        // Register hax_vcpu
        err = config_cfdriver_attach(&hax_vcpu_cd);
        if (err) {
            hax_log(HAX_LOGE, "Unable to register cfdriver hax_vcpu\n");
            goto init_err4;
        }

        err = config_cfattach_attach(hax_vcpu_cd.cd_name, &hax_vcpu_ca);
        if (err) {
            hax_log(HAX_LOGE, "Unable to register cfattch hax_vcpu\n");
            goto init_err5;
        }

        err = config_cfdata_attach(hax_vcpu_cfdata, 1);
        if (err) {
            hax_log(HAX_LOGE, "Unable to register cfdata hax_vcpu\n");
            goto init_err6;
        }

        // Register device entries
        err = devsw_attach(HAX_DEVICE_NAME, NULL, &hax_bmajor, &hax_cdevsw,
                       &hax_cmajor);
        if (err) {
            hax_log(HAX_LOGE, "Failed to register HAXM device\n");
            goto init_err7;
        }
        err = devsw_attach(HAX_VM_DEVICE_NAME, NULL, &hax_vm_bmajor, &hax_vm_cdevsw,
                       &hax_vm_cmajor);
        if (err) {
            hax_log(HAX_LOGE, "Failed to register HAXM VM device\n");
            goto init_err8;
        }
        err = devsw_attach(HAX_VCPU_DEVICE_NAME, NULL, &hax_vcpu_bmajor, &hax_vcpu_cdevsw,
                       &hax_vcpu_cmajor);
        if (err) {
            hax_log(HAX_LOGE, "Failed to register HAXM VCPU device\n");
            goto init_err9;
        }

        for (i = 0; i < HAX_MAX_VMS; i++)
            config_attach_pseudo(hax_vm_cfdata);

        for (i = 0; i < (HAX_MAX_VMS * HAX_MAX_VCPUS); i++)
            config_attach_pseudo(hax_vcpu_cfdata);

        // Initialize HAXM
        if (hax_module_init() < 0) {
            hax_log(HAX_LOGE, "Failed to initialize HAXM module\n");
            goto init_err10;
        }

        hax_log(HAX_LOGI, "Created HAXM device\n");
        return 0;

init_err10:
        devsw_detach(NULL, &hax_vcpu_cdevsw);
init_err9:
        devsw_detach(NULL, &hax_vm_cdevsw);
init_err8:
        devsw_detach(NULL, &hax_cdevsw);
init_err7:
        config_cfdata_detach(hax_vcpu_cfdata);
init_err6:
        config_cfattach_detach(hax_vcpu_cd.cd_name, &hax_vcpu_ca);
init_err5:
        config_cfdriver_detach(&hax_vcpu_cd);
init_err4:
        config_cfdata_detach(hax_vm_cfdata);
init_err3:
        config_cfattach_detach(hax_vm_cd.cd_name, &hax_vm_ca);
init_err2:
        config_cfdriver_detach(&hax_vm_cd);
init_err1:
        cpu_info_exit();
init_err0:
        return ENXIO;
    }
    case MODULE_CMD_FINI: {
        if (hax_module_exit() < 0) {
            hax_log(HAX_LOGE, "Failed to finalize HAXM module\n");
            return EBUSY;
        }

        devsw_detach(NULL, &hax_vcpu_cdevsw);
        devsw_detach(NULL, &hax_vm_cdevsw);
        devsw_detach(NULL, &hax_cdevsw);

        config_cfdata_detach(hax_vcpu_cfdata);
        config_cfattach_detach(hax_vcpu_cd.cd_name, &hax_vcpu_ca);
        config_cfdriver_detach(&hax_vcpu_cd);

        config_cfdata_detach(hax_vm_cfdata);
        config_cfattach_detach(hax_vm_cd.cd_name, &hax_vm_ca);
        config_cfdriver_detach(&hax_vm_cd);

        hax_log(HAX_LOGI, "Removed HAXM device\n");
        return 0;
    }
    default:
        return ENOTTY;
    }
}
