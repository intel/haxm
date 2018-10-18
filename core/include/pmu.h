/*
 * Copyright (c) 2017 Intel Corporation
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

#ifndef HAX_CORE_PMU_H_
#define HAX_CORE_PMU_H_

#include "types.h"

/*
 * Information about a logical processor's performance monitoring units (PMU),
 * particularly its support for architectural performance monitoring (APM). See
 * IA SDM Vol. 2A Table 3-8 ("Information Returned by CPUID Instruction"),
 * Initial EAX Value = 0AH ("Architectural Performance Monitoring Leaf").
 */
struct cpu_pmu_info {
    union {
        uint32_t cpuid_eax;
        struct {
            // Version ID of architectural performance monitoring (APM)
            uint32_t apm_version        : 8;
            // Number of general-purpose performance monitoring counters
            uint32_t apm_general_count  : 8;
            // Bit width of general-purpose performance monitoring counters
            uint32_t apm_general_bitlen : 8;
            // Length of EBX bit vector
            uint32_t apm_event_count    : 8;
        };
    };
    union {
        uint32_t cpuid_ebx;
        // Bit vector to enumerate APM events
        uint32_t apm_event_unavailability;
    };
    union {
        uint32_t cpuid_edx;
        struct {
            // Number of fixed-function performance monitoring counters
            uint32_t apm_fixed_count  : 5;
            // Bit width of fixed-function performance monitoring counters
            uint32_t apm_fixed_bitlen : 8;
            // Reserved
            uint32_t                  : 19;
        };
    };
} PACKED;

// Maximum number of general-purpose performance monitoring counters per
// processor: IA SDM Vol. 3C 35.1 defines IA32_PERFEVTSELx, x = 0..3
#define APM_MAX_GENERAL_COUNT 4
// Maximum number of APM events: IA SDM Vol. 2 Table 3-8 lists 7 APM events
#define APM_MAX_EVENT_COUNT   7
// Maximum number of fixed-function performance monitoring counters per
// processor: IA SDM Vol. 3C 35.1 defines IA32_FIXED_CTRx, x = 0..2
#define APM_MAX_FIXED_COUNT   3

#endif  // HAX_CORE_PMU_H_
