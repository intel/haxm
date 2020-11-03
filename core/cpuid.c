/*
 * Copyright (c) 2018 Alexandro Sanchez Bach <alexandro@phi.nz>
 * Copyright (c) 2020 Intel Corporation
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

typedef void (*set_leaf_t)(hax_cpuid_entry *, hax_cpuid_entry *);

typedef struct cpuid_controller_t {
    uint32_t    leaf;
    set_leaf_t  set_leaf;
} cpuid_controller_t;

static cpuid_cache_t cache = {0};

static hax_cpuid_entry * find_cpuid_entry(hax_cpuid_entry *features,
                                          uint32_t size, uint32_t function,
                                          uint32_t index);
static void dump_features(hax_cpuid_entry *features, uint32_t size);
static void filter_features(hax_cpuid_entry *entry);
static uint32_t get_feature_key_leaf(uint32_t function, uint32_t reg,
                                     uint32_t bit);

static void set_feature(hax_cpuid_entry *features, hax_cpuid *cpuid_info,
                        const cpuid_controller_t *cpuid_controller);
static void set_leaf_0000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src);
static void set_leaf_8000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src);

static const cpuid_controller_t kCpuidController[] = {
    {0x00000001, set_leaf_0000_0001},
    {0x80000001, set_leaf_8000_0001}
};

#define CPUID_TOTAL_CONTROLS \
        sizeof(kCpuidController)/sizeof(kCpuidController[0])

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
    uint32_t i, bit, flag, function, x86_feature;
    hax_cpuid_entry *host_supported, *hax_supported;

    // Initialize host supported features
    host_supported = cache.host_supported.features;

    for (i = 0; i < CPUID_FEATURE_SET_SIZE; ++i) {
        function = kCpuidController[i].leaf;
        host_supported[i].function = function;

        for (bit = 0; bit < sizeof(uint32_t) * 8; ++bit) {
            flag = 1 << bit;

#define SET_FEATURE_FLAG(r, n)                                       \
            x86_feature = get_feature_key_leaf(function, (n), bit);  \
            if (cpuid_host_has_feature(x86_feature)) {               \
                host_supported[i].r |= flag;                         \
            }

            SET_FEATURE_FLAG(eax, CPUID_REG_EAX);
            SET_FEATURE_FLAG(ebx, CPUID_REG_EBX);
            SET_FEATURE_FLAG(ecx, CPUID_REG_ECX);
            SET_FEATURE_FLAG(edx, CPUID_REG_EDX);
#undef SET_FEATURE_FLAG
        }
    }

    hax_log(HAX_LOGI, "%s: host supported features:\n", __func__);
    dump_features(host_supported, CPUID_FEATURE_SET_SIZE);

    // Initialize HAXM supported features
    hax_supported = cache.hax_supported.features;
    hax_supported[0] = (hax_cpuid_entry){
        .function = 0x01,
        .ecx =
            FEATURE(SSE3)       |
            FEATURE(SSSE3)      |
            FEATURE(SSE41)      |
            FEATURE(SSE42)      |
            FEATURE(CMPXCHG16B) |
            FEATURE(MOVBE)      |
            FEATURE(AESNI)      |
            FEATURE(PCLMULQDQ)  |
            FEATURE(POPCNT),
        .edx =
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
            FEATURE(HTT)
    };
    hax_supported[1] = (hax_cpuid_entry){
        .function = 0x80000001,
        .edx =
            FEATURE(NX)         |
            FEATURE(SYSCALL)    |
            FEATURE(RDTSCP)     |
            FEATURE(EM64T)
    };

    hax_log(HAX_LOGI, "%s: HAXM supported features:\n", __func__);
    dump_features(hax_supported, CPUID_FEATURE_SET_SIZE);
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

void cpuid_get_guest_features(hax_cpuid_t *cpuid, hax_cpuid_entry *features)
{
    hax_cpuid_entry *entry;

    if (cpuid == NULL || features == NULL)
        return;

    entry = find_cpuid_entry(cpuid->features, CPUID_FEATURE_SET_SIZE,
                             features->function, 0);
    if (entry == NULL)
        return;

    *features = *entry;
}

void cpuid_set_guest_features(hax_cpuid_t *cpuid, hax_cpuid *cpuid_info)
{
    int i;

    if (cpuid == NULL || cpuid_info == NULL)
        return;

    hax_log(HAX_LOGI, "%s: user setting:\n", __func__);
    dump_features(cpuid_info->entries, cpuid_info->total);

    hax_log(HAX_LOGI, "%s: before:\n", __func__);
    dump_features(cpuid->features, CPUID_FEATURE_SET_SIZE);

    for (i = 0; i < CPUID_TOTAL_CONTROLS; ++i) {
        set_feature(cpuid->features, cpuid_info, &kCpuidController[i]);
    }

    hax_log(HAX_LOGI, "%s: after:\n", __func__);
    dump_features(cpuid->features, CPUID_FEATURE_SET_SIZE);
}

static hax_cpuid_entry * find_cpuid_entry(hax_cpuid_entry *features,
                                          uint32_t size, uint32_t function,
                                          uint32_t index)
{
    int i;
    hax_cpuid_entry *entry;

    if (features == NULL)
        return NULL;

    for (i = 0; i < size; ++i) {
        entry = &features[i];
        if (entry->function == function && entry->index == index)
            return entry;
    }

    return NULL;
}

static void dump_features(hax_cpuid_entry *features, uint32_t size)
{
    int i;
    hax_cpuid_entry *entry;

    if (features == NULL)
        return;

    for (i = 0; i < size; ++i) {
        entry = &features[i];
        hax_log(HAX_LOGI, "function: %08lx, index: %lu, flags: %08lx\n",
                entry->function, entry->index, entry->flags);
        hax_log(HAX_LOGI, "eax: %08lx, ebx: %08lx, ecx: %08lx, edx: %08lx\n",
                entry->eax, entry->ebx, entry->ecx, entry->edx);
    }
}

static void filter_features(hax_cpuid_entry *entry)
{
    hax_cpuid_entry *host_supported, *hax_supported;

    host_supported = find_cpuid_entry(cache.host_supported.features,
            CPUID_FEATURE_SET_SIZE, entry->function, 0);
    hax_supported = find_cpuid_entry(cache.hax_supported.features,
            CPUID_FEATURE_SET_SIZE, entry->function, 0);

    if (host_supported == NULL || hax_supported == NULL)
        return;

    entry->eax &= host_supported->eax & hax_supported->eax;
    entry->ebx &= host_supported->ebx & hax_supported->ebx;
    entry->ecx &= host_supported->ecx & hax_supported->ecx;
    entry->edx &= host_supported->edx & hax_supported->edx;
}

static uint32_t get_feature_key_leaf(uint32_t function, uint32_t reg,
                                     uint32_t bit)
{
    if (function == 0x01) {
        if (reg == CPUID_REG_ECX)
            return FEATURE_KEY_LEAF(0, function, reg, bit);

        if (reg == CPUID_REG_EDX)
            return FEATURE_KEY_LEAF(1, function, reg, bit);

        return -1;
    }

    if (function == 0x80000001) {
        if (reg == CPUID_REG_EDX)
            return FEATURE_KEY_LEAF(5, function, reg, bit);

        return -1;
    }

    return -1;
}

static void set_feature(hax_cpuid_entry *features, hax_cpuid *cpuid_info,
                        const cpuid_controller_t *cpuid_controller)
{
    hax_cpuid_entry *dest, *src;
    uint32_t leaf;

    if (features == NULL || cpuid_info == NULL)
        return;

    leaf = cpuid_controller->leaf;

    dest = find_cpuid_entry(features, CPUID_FEATURE_SET_SIZE, leaf, 0);
    if (dest == NULL)
        return;

    src = find_cpuid_entry(cpuid_info->entries, cpuid_info->total, leaf, 0);
    if (src == NULL)
        return;

    if (cpuid_controller->set_leaf != NULL) {
        cpuid_controller->set_leaf(dest, src);
    } else {
        *dest = *src;
    }

    if (src->eax == dest->eax && src->ebx == dest->ebx &&
        src->ecx == dest->ecx && src->edx == dest->edx)
        return;

    hax_log(HAX_LOGW, "%s: filtered or unchanged flags:\n", __func__);
    hax_log(HAX_LOGW, "leaf: %08lx, eax: %08lx, ebx: %08lx, ecx: %08lx, "
            "edx: %08lx\n", leaf, src->eax ^ dest->eax, src->ebx ^ dest->ebx,
            src->ecx ^ dest->ecx, src->edx ^ dest->edx);
}

static void set_leaf_0000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src)
{
    const uint32_t kFixedFeatures =
        FEATURE(MCE)  |
        FEATURE(APIC) |
        FEATURE(MTRR) |
        FEATURE(PAT);

    if (dest == NULL || src == NULL)
        return;

    *dest = *src;
    filter_features(dest);
    // Set fixed supported features
    dest->edx |= kFixedFeatures;
}

static void set_leaf_8000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src)
{
    if (dest == NULL || src == NULL)
        return;

    *dest = *src;
    filter_features(dest);
}
