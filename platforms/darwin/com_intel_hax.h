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

#ifndef HAX_DARWIN_COM_INTEL_HAX_H_
#define HAX_DARWIN_COM_INTEL_HAX_H_

#include <mach/mach_types.h>
#include <IOKit/IOLib.h>
#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
#include <sys/ioccom.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <libkern/OSBase.h>

#include "../../include/hax.h"
#include "../../core/include/hax_core_interface.h"

#include "com_intel_hax_component.h"
#include "com_intel_hax_ui.h"
#include "com_intel_hax_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Definition for hax_mem_alloc.cpp */
int hax_malloc_init(void);
void hax_malloc_exit(void);

#ifdef __cplusplus
}
#endif
#endif  // HAX_DARWIN_COM_INTEL_HAX_H_
