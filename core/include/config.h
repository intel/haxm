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

#ifndef HAX_CORE_CONFIG_H_
#define HAX_CORE_CONFIG_H_

#include "../../include/hax_types.h"

struct config_t {
    int memory_pass_through;
    int disable_ept;
    int ept_small_pages;
    int disable_vpid;
    int disable_unrestricted_guest;

    /*
     * The default behavior for CPUID is to take the physical values and modify
     * them by removing features we don't want the guest to see.
     * no_cpuid_pass_through forces all values from the CPUID instruction to be
     * virtualized.
     * cpuid_pass_through forces all values from the CPUID instruction to be
     * passed through. Note: cpuid_pass_through may not always work, because it
     * may give the guest access to features we don't support.
     * cpuid_no_mwait controls whether the guest can see the monitor/mwait
     * feature. This option is meaningful only with default CPUID behavior -- it
     * is ignored in either the cpuid_pass_through or no_cpuid_pass_though
     * cases.
     */
    int no_cpuid_pass_through;
    int cpuid_pass_through;
    int cpuid_no_mwait;

    /*
     * The default behavior for MSRs is to pass through access to all MSRs
     * other than those that Hypersim reserves and those that are passed by
     * other means (such as the SYSENTER MSRs).
     * no_msr_pass_through forces all MSRs to be virtualized.
     */
    int no_msr_pass_through;
};

#define HAX_MAX_VCPUS 16

#ifdef HAX_PLATFORM_NETBSD
// TODO: Handle 64 VMs
#define HAX_MAX_VMS 8
#else
// Matches the number of bits in vm_mid_bits (see vm.c)
#define HAX_MAX_VMS 64
#endif

#endif  // HAX_CORE_CONFIG_H_
