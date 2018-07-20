/*
 * Copyright (c) 2018 Alexandro Sanchez Bach <alexandro@phi.nz>
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

#include "include/cpuid.h"

#include "include/ia32.h"

typedef union cpuid_feature_t {
    struct {
        uint32_t index        : 5;
        uint32_t leaf_lo      : 5;
        uint32_t leaf_hi      : 2;
        uint32_t subleaf_key  : 5;
        uint32_t subleaf_used : 1;
        uint32_t reg          : 2;
        uint32_t bit          : 5;
        uint32_t /*reserved*/ : 7;
    };
    uint32_t value;
} cpuid_feature_t;

void cpuid_query_leaf(cpuid_args_t *args, uint32_t leaf)
{
    args->eax = leaf;
    asm_cpuid(args);
}

void cpuid_query_subleaf(cpuid_args_t *args, uint32_t leaf, uint32_t subleaf)
{
    args->eax = leaf;
    args->ecx = subleaf;
    asm_cpuid(args);
}

void cpuid_host_init(cpuid_cache_t *cache)
{
    cpuid_args_t res;
    uint32_t *data = cache->data;

    cpuid_query_leaf(&res, 0x00000001);
    data[0] = res.ecx;
    data[1] = res.edx;

    cpuid_query_subleaf(&res, 0x00000007, 0x00);
    data[2] = res.ecx;
    data[3] = res.ebx;

    cpuid_query_leaf(&res, 0x80000001);
    data[4] = res.ecx;
    data[5] = res.edx;

    cache->initialized = 1;
}

bool cpuid_host_has_feature(cpuid_cache_t *cache, uint32_t feature_key)
{
    cpuid_feature_t feature;
    uint32_t value;

    feature.value = feature_key;
    if (!cache->initialized || feature.index >= CPUID_CACHE_SIZE) {
        return cpuid_host_has_feature_uncached(feature_key);
    }
    value = cache->data[feature.index];
    if (value & (1 << feature.bit)) {
        return true;
    }
    return false;
}

bool cpuid_host_has_feature_uncached(uint32_t feature_key)
{
    cpuid_args_t res;
    cpuid_feature_t feature;
    uint32_t value, leaf;

    feature.value = feature_key;
    leaf = (feature.leaf_hi << 30) | feature.leaf_lo;
    if (feature.subleaf_used) {
        cpuid_query_subleaf(&res, leaf, feature.subleaf_key);
    } else {
        cpuid_query_leaf(&res, leaf);
    }
    value = res.regs[feature.reg];
    if (value & (1 << feature.bit)) {
        return true;
    }
    return false;
}
