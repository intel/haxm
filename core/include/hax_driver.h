/*
 * Copyright (c) 2009 Intel Corporation
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

#ifndef HAX_CORE_HAX_DRIVER_H_
#define HAX_CORE_HAX_DRIVER_H_

#include "vm.h"
#include "pmu.h"
#define CONFIG_VM_NUM 16

struct hax_t {
    int vmx_enable_flag;
    int nx_enable_flag;
    int em64t_enable_flag;
    int ug_enable_flag;

    /*
     * Common architectural performance monitoring (APM) parameters (version ID,
     * etc.) supported by all logical processors of the host CPU
     */
    uint apm_version;
    uint apm_general_count;
    uint64_t apm_general_mask;
    uint apm_event_count;
    uint32_t apm_event_unavailability;
    uint apm_fixed_count;
    uint64_t apm_fixed_mask;
    // Unparsed CPUID leaf 0xa output for CPUID virtualization
    struct cpu_pmu_info apm_cpuid_0xa;

    hax_list_head hax_vmlist;
    hax_mutex hax_lock;
    uint64_t mem_limit;
    uint64_t mem_quota;
};

uint64_t hax_get_memory_threshold(void);
extern struct hax_t *hax;
#endif  // HAX_CORE_HAX_DRIVER_H_
