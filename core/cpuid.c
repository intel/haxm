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

#define CPUID_CACHE_SIZE 6

typedef struct cpuid_cache_t {
    uint32_t     data[CPUID_CACHE_SIZE];  // Host cached features
    hax_cpuid_t  host_supported;          // Physical CPU supported features
    hax_cpuid_t  hax_supported;           // Hypervisor supported features
    bool         initialized;
} cpuid_cache_t;

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

static cpuid_cache_t cache = {0};

static hax_cpuid_entry * find_cpuid_entry(hax_cpuid *cpuid_info,
                                          uint32_t function, uint32_t index);
static void cpuid_set_0000_0001(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info);
static void cpuid_set_8000_0001(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info);
static void cpuid_set_fixed_features(hax_cpuid_t *cpuid);

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

void cpuid_host_init(void)
{
    cpuid_args_t res;
    uint32_t *data = cache.data;

    cpuid_query_leaf(&res, 0x00000001);
    data[0] = res.ecx;
    data[1] = res.edx;

    cpuid_query_subleaf(&res, 0x00000007, 0x00);
    data[2] = res.ecx;
    data[3] = res.ebx;

    cpuid_query_leaf(&res, 0x80000001);
    data[4] = res.ecx;
    data[5] = res.edx;

    cache.initialized = true;
}

bool cpuid_host_has_feature(uint32_t feature_key)
{
    cpuid_feature_t feature;
    uint32_t value;

    feature.value = feature_key;
    if (!cache.initialized || feature.index >= CPUID_CACHE_SIZE) {
        return cpuid_host_has_feature_uncached(feature_key);
    }
    value = cache.data[feature.index];
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

void cpuid_init_supported_features(void)
{
    uint32_t bit, flag, function, x86_feature;

    // Initialize host supported features
    for (bit = 0; bit < sizeof(uint32_t) * 8; ++bit) {
        flag = 1 << bit;

        function = 0x01;
        x86_feature = FEATURE_KEY_LEAF(0, function, CPUID_REG_ECX, bit);
        if (cpuid_host_has_feature(x86_feature)) {
            cache.host_supported.feature_1_ecx |= flag;
        }

        x86_feature = FEATURE_KEY_LEAF(1, function, CPUID_REG_EDX, bit);
        if (cpuid_host_has_feature(x86_feature)) {
            cache.host_supported.feature_1_edx |= flag;
        }

        function = 0x80000001;
        x86_feature = FEATURE_KEY_LEAF(5, function, CPUID_REG_EDX, bit);
        if (cpuid_host_has_feature(x86_feature)) {
            cache.host_supported.feature_8000_0001_edx |= flag;
        }
    }

    hax_log(HAX_LOGI, "%s: host supported features:\n", __func__);
    hax_log(HAX_LOGI, "feature_1_ecx: %08lx, feature_1_edx: %08lx\n",
            cache.host_supported.feature_1_ecx,
            cache.host_supported.feature_1_edx);
    hax_log(HAX_LOGI, "feature_8000_0001_ecx: %08lx, "
            "feature_8000_0001_edx: %08lx\n",
            cache.host_supported.feature_8000_0001_ecx,
            cache.host_supported.feature_8000_0001_edx);

    // Initialize HAXM supported features
    cache.hax_supported = (hax_cpuid_t){
        .feature_1_ecx =
            FEATURE(SSE3)       |
            FEATURE(SSSE3)      |
            FEATURE(SSE41)      |
            FEATURE(SSE42)      |
            FEATURE(CMPXCHG16B) |
            FEATURE(MOVBE)      |
            FEATURE(AESNI)      |
            FEATURE(PCLMULQDQ)  |
            FEATURE(POPCNT),
        .feature_1_edx =
            FEATURE(PAT)        |
            FEATURE(FPU)        |
            FEATURE(VME)        |
            FEATURE(DE)         |
            FEATURE(TSC)        |
            FEATURE(MSR)        |
            FEATURE(PAE)        |
            FEATURE(MCE)        |
            FEATURE(CX8)        |
            FEATURE(APIC)       |
            FEATURE(SEP)        |
            FEATURE(MTRR)       |
            FEATURE(PGE)        |
            FEATURE(MCA)        |
            FEATURE(CMOV)       |
            FEATURE(CLFSH)      |
            FEATURE(MMX)        |
            FEATURE(FXSR)       |
            FEATURE(SSE)        |
            FEATURE(SSE2)       |
            FEATURE(SS)         |
            FEATURE(PSE)        |
            FEATURE(HTT),
        .feature_8000_0001_ecx = 0,
        .feature_8000_0001_edx =
            FEATURE(NX)         |
            FEATURE(SYSCALL)    |
            FEATURE(RDTSCP)     |
            FEATURE(EM64T)
    };

    hax_log(HAX_LOGI, "%s: HAXM supported features:\n", __func__);
    hax_log(HAX_LOGI, "feature_1_ecx: %08lx, feature_1_edx: %08lx\n",
            cache.hax_supported.feature_1_ecx,
            cache.hax_supported.feature_1_edx);
    hax_log(HAX_LOGI, "feature_8000_0001_ecx: %08lx, "
            "feature_8000_0001_edx: %08lx\n",
            cache.hax_supported.feature_8000_0001_ecx,
            cache.hax_supported.feature_8000_0001_edx);
}

void cpuid_guest_init(hax_cpuid_t *cpuid)
{
    *cpuid = cache.hax_supported;
    cpuid->features_mask = ~0ULL;
}

void cpuid_get_features_mask(hax_cpuid_t *cpuid, uint64_t *features_mask)
{
    *features_mask = cpuid->features_mask;
}

void cpuid_set_features_mask(hax_cpuid_t *cpuid, uint64_t features_mask)
{
    cpuid->features_mask = features_mask;
}

void cpuid_get_guest_features(hax_cpuid_t *cpuid,
                              uint32_t *cpuid_1_features_ecx,
                              uint32_t *cpuid_1_features_edx,
                              uint32_t *cpuid_8000_0001_features_ecx,
                              uint32_t *cpuid_8000_0001_features_edx)
{
    *cpuid_1_features_ecx         = cpuid->feature_1_ecx;
    *cpuid_1_features_edx         = cpuid->feature_1_edx;
    *cpuid_8000_0001_features_ecx = cpuid->feature_8000_0001_ecx;
    *cpuid_8000_0001_features_edx = cpuid->feature_8000_0001_edx;
}

void cpuid_set_guest_features(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info)
{
    static void (*cpuid_set_guest_feature[])(hax_cpuid_t *, hax_cpuid *) = {
        cpuid_set_0000_0001,
        cpuid_set_8000_0001
    };
    static size_t count = sizeof(cpuid_set_guest_feature) /
                          sizeof(cpuid_set_guest_feature[0]);
    int i;

    hax_log(HAX_LOGI, "%s: before:\n", __func__);
    hax_log(HAX_LOGI, "feature_1_ecx: %08lx, feature_1_edx: %08lx\n",
            cpuid->feature_1_ecx, cpuid->feature_1_edx);
    hax_log(HAX_LOGI, "feature_8000_0001_ecx: %08lx, feature_8000_0001_edx: %08lx"
            "\n", cpuid->feature_8000_0001_ecx, cpuid->feature_8000_0001_edx);

    for (i = 0; i < count; ++i) {
        cpuid_set_guest_feature[i](cpuid, cpuid_info);
    }

    hax_log(HAX_LOGI, "%s: after:\n", __func__);
    hax_log(HAX_LOGI, "feature_1_ecx: %08lx, feature_1_edx: %08lx\n",
            cpuid->feature_1_ecx, cpuid->feature_1_edx);
    hax_log(HAX_LOGI, "feature_8000_0001_ecx: %08lx, feature_8000_0001_edx: %08lx"
            "\n", cpuid->feature_8000_0001_ecx, cpuid->feature_8000_0001_edx);
}

static hax_cpuid_entry * find_cpuid_entry(hax_cpuid *cpuid_info,
                                          uint32_t function, uint32_t index)
{
    int i;
    hax_cpuid_entry *entry, *found = NULL;

    for (i = 0; i < cpuid_info->total; ++i) {
        entry = &cpuid_info->entries[i];
        if (entry->function == function && entry->index == index) {
            found = entry;
            break;
        }
    }

    return found;
}

static void cpuid_set_0000_0001(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info)
{
    const uint32_t kFunction = 0x01;
    hax_cpuid_entry *entry;

    entry = find_cpuid_entry(cpuid_info, kFunction, 0);
    if (entry == NULL)
        return;

    hax_log(HAX_LOGI, "%s: function: %08lx, index: %lu, flags: %08lx\n",
            __func__, entry->function, entry->index, entry->flags);
    hax_log(HAX_LOGI, "%s: eax: %08lx, ebx: %08lx, ecx: %08lx, edx: %08lx\n",
            __func__, entry->eax, entry->ebx, entry->ecx, entry->edx);

    cpuid->feature_1_ecx = entry->ecx;
    cpuid->feature_1_edx = entry->edx;

    // Filter the unsupported features
    cpuid->feature_1_ecx &= cache.host_supported.feature_1_ecx &
                            cache.hax_supported.feature_1_ecx;
    cpuid->feature_1_edx &= cache.host_supported.feature_1_edx &
                            cache.hax_supported.feature_1_edx;

    // Set fixed supported features
    cpuid_set_fixed_features(cpuid);

    if (entry->ecx != cpuid->feature_1_ecx ||
        entry->edx != cpuid->feature_1_edx) {
        hax_log(HAX_LOGW, "%s: filtered or unchanged flags: ecx: %08lx, "
                "edx: %08lx\n", __func__, entry->ecx ^ cpuid->feature_1_ecx,
                entry->edx ^ cpuid->feature_1_edx);
    }
}

static void cpuid_set_8000_0001(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info)
{
    const uint32_t kFunction = 0x80000001;
    hax_cpuid_entry *entry;

    entry = find_cpuid_entry(cpuid_info, kFunction, 0);
    if (entry == NULL)
        return;

    hax_log(HAX_LOGI, "%s: function: %08lx, index: %lu, flags: %08lx\n",
            __func__, entry->function, entry->index, entry->flags);
    hax_log(HAX_LOGI, "%s: eax: %08lx, ebx: %08lx, ecx: %08lx, edx: %08lx\n",
            __func__, entry->eax, entry->ebx, entry->ecx, entry->edx);

    cpuid->feature_8000_0001_edx = entry->edx;

    // Filter the unsupported features
    cpuid->feature_8000_0001_edx &=
        cache.host_supported.feature_8000_0001_edx &
        cache.hax_supported.feature_8000_0001_edx;

    if (entry->edx != cpuid->feature_8000_0001_edx) {
        hax_log(HAX_LOGW, "%s: filtered or unchanged flags: edx: %08lx\n",
                __func__, entry->edx ^ cpuid->feature_8000_0001_edx);
    }
}

static void cpuid_set_fixed_features(hax_cpuid_t *cpuid)
{
    const uint32_t kFixedFeatures =
        FEATURE(MCE)  |
        FEATURE(APIC) |
        FEATURE(MTRR) |
        FEATURE(PAT);

    cpuid->feature_1_edx |= kFixedFeatures;
}
