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

lck_grp_t *hax_lck_grp = NULL, *hax_mtx_grp = NULL;
lck_attr_t *hax_lck_attr = NULL, *hax_mtx_attr = NULL;
static lck_grp_attr_t *hax_lck_grp_attr = NULL, *hax_mtx_grp_attr = NULL;

static void lock_prim_exit(void)
{
    if (hax_lck_attr) {
        lck_attr_free(hax_lck_attr);
        hax_lck_attr = NULL;
    }
    if (hax_lck_grp) {
        lck_grp_free(hax_lck_grp);
        hax_lck_grp = NULL;
    }
    if (hax_lck_grp_attr) {
        lck_grp_attr_free(hax_lck_grp_attr);
        hax_lck_grp_attr = NULL;
    }
    if (hax_mtx_attr) {
        lck_attr_free(hax_mtx_attr);
        hax_mtx_attr = NULL;
    }
    if (hax_mtx_grp) {
        lck_grp_free(hax_mtx_grp);
        hax_mtx_grp = NULL;
    }
    if (hax_mtx_grp_attr) {
        lck_grp_attr_free(hax_mtx_grp_attr);
        hax_mtx_grp_attr = NULL;
    }
}

static int lock_prim_init(void)
{
    hax_lck_grp_attr = lck_grp_attr_alloc_init();
    if (!hax_lck_grp_attr)
        goto error;
    lck_grp_attr_setstat(hax_lck_grp_attr);

    hax_lck_grp = lck_grp_alloc_init("haxlock", hax_lck_grp_attr);
    if (!hax_lck_grp)
        goto error;

    hax_lck_attr = lck_attr_alloc_init();
    if (!hax_lck_attr)
        goto error;

    /* no idea if the spinlock and mutex can share the same grp and grp attr,
     * so provide two now
     */
    hax_mtx_grp_attr = lck_grp_attr_alloc_init();
    if (!hax_mtx_grp_attr)
        goto error;
    lck_grp_attr_setstat(hax_mtx_grp_attr);

    hax_mtx_grp = lck_grp_alloc_init("haxmtx", hax_mtx_grp_attr);
    if (!hax_mtx_grp)
        goto error;
    hax_mtx_attr = lck_attr_alloc_init();
    if (!hax_mtx_attr)
        goto error;

    return 0;
error:
    hax_log(HAX_LOGE, "Failed to init lock primitive\n");
    lock_prim_exit();
    return -1;
}

static int com_intel_hax_init(void)
{
    int ret;

    ret = lock_prim_init();
    if (ret < 0)
        return ret;

    ret = cpu_info_init();
    if (ret < 0)
        goto fail0;

    ret = hax_malloc_init();
    if (ret < 0)
        goto fail0;

    return 0;
fail0:
    cpu_info_exit();
    lock_prim_exit();
    return ret;
}

static int com_intel_hax_exit(void)
{

    hax_malloc_exit();
    lock_prim_exit();
    return 0;
}

kern_return_t com_intel_hax_start(kmod_info_t * ki, void * d) {
    hax_log(HAX_LOGD, "Start HAX module\n");

    if (com_intel_hax_init() < 0) {
        hax_log(HAX_LOGE, "Failed to init hax context\n");
        return KERN_FAILURE;
    }

    if (hax_module_init() < 0) {
        hax_log(HAX_LOGE, "Failed to init host hax\n");
        goto fail1;
    }

    if (!hax_em64t_enabled()) {
        hax_log(HAX_LOGE, "Cpu has no EMT64 support!\n");
        goto fail2;
    }

    if (com_intel_hax_init_ui() < 0) {
        hax_log(HAX_LOGE, "Failed to init hax UI\n");
        goto fail2;
    }

    return KERN_SUCCESS;

fail2:
    hax_module_exit();
fail1:
    com_intel_hax_exit();

    return KERN_FAILURE;
}

kern_return_t com_intel_hax_stop(kmod_info_t * ki, void * d)
{
    int ret;

    hax_log(HAX_LOGD, "Stop HAX module\n");
    ret = hax_module_exit();
    if (ret < 0) {
        hax_log(HAX_LOGE, "The module can't be removed now, \n"
                " close all VM interface and try again\n");
        return KERN_FAILURE;
    }
    com_intel_hax_exit();
    return KERN_SUCCESS;
}
