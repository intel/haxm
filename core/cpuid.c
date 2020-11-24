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
#include "include/hax_driver.h"

#define CPUID_CACHE_SIZE        6
#define CPUID_FEATURE_SET_SIZE  2
#define MAX_BASIC_CPUID         0x16
#define MAX_EXTENDED_CPUID      0x80000008

extern uint32_t pw_reserved_bits_high_mask;

typedef struct cpuid_cache_t {
    // Host cached features
    uint32_t         data[CPUID_CACHE_SIZE];
    // Physical CPU supported features
    hax_cpuid_entry  host_supported[CPUID_FEATURE_SET_SIZE];
    // Hypervisor supported features
    hax_cpuid_entry  hax_supported[CPUID_FEATURE_SET_SIZE];
    bool             initialized;
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

typedef void (*execute_t)(cpuid_args_t *);
typedef void (*set_leaf_t)(hax_cpuid_entry *, hax_cpuid_entry *);

typedef struct cpuid_manager_t {
    uint32_t   leaf;
    execute_t  execute;
} cpuid_manager_t;

typedef struct cpuid_controller_t {
    uint32_t    leaf;
    set_leaf_t  set_leaf;
} cpuid_controller_t;

static cpuid_cache_t cache = {0};

static hax_cpuid_entry * find_cpuid_entry(hax_cpuid_entry *features,
                                          uint32_t size, uint32_t function,
                                          uint32_t index);
static void dump_features(hax_cpuid_entry *features, uint32_t size);

static void get_guest_cache(cpuid_args_t *args, hax_cpuid_entry *entry);
static void adjust_0000_0001(cpuid_args_t *args);
static void adjust_8000_0001(cpuid_args_t *args);
static void execute_0000_0000(cpuid_args_t *args);
static void execute_0000_0001(cpuid_args_t *args);
static void execute_0000_0002(cpuid_args_t *args);
static void execute_0000_000a(cpuid_args_t *args);
static void execute_4000_0000(cpuid_args_t *args);
static void execute_8000_0000(cpuid_args_t *args);
static void execute_8000_0001(cpuid_args_t *args);
static void execute_8000_0002(cpuid_args_t *args);
static void execute_8000_0003(cpuid_args_t *args);
static void execute_8000_0006(cpuid_args_t *args);
static void execute_8000_0008(cpuid_args_t *args);

static void set_feature(hax_cpuid_entry *features, hax_cpuid *cpuid_info,
                        const cpuid_controller_t *cpuid_controller);
static void set_leaf_0000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src);
static void set_leaf_0000_0015(hax_cpuid_entry *dest, hax_cpuid_entry *src);
static void set_leaf_0000_0016(hax_cpuid_entry *dest, hax_cpuid_entry *src);
static void set_leaf_8000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src);

// To fully support CPUID instructions (opcode = 0F A2) by software, it is
// recommended to add opcode_table_0FA2[] in core/emulate.c to emulate
// (Refer to Intel SDM Vol. 2A 3.2 CPUID).

static const cpuid_manager_t kCpuidManager[] = {
    // Basic CPUID Information
    {0x00000000, execute_0000_0000},  // Maximum Basic Information
    {0x00000001, execute_0000_0001},  // Version Information and Features
    {0x00000002, execute_0000_0002},  // Cache and TLB Information
    {0x0000000a, execute_0000_000a},  // Architectural Performance Monitoring
    {0x00000015, NULL},               // Time Stamp Counter and Nominal Core
                                      // Crystal Clock Information
    {0x00000016, NULL},               // Processor Frequency Information

    // Unimplemented CPUID Leaf Functions
    {0x40000000, execute_4000_0000},  // Unimplemented by real Intel CPUs

    // Extended Function CPUID Information
    {0x80000000, execute_8000_0000},  // Maximum Extended Information
    {0x80000001, execute_8000_0001},  // Extended Signature and Features
    {0x80000002, execute_8000_0002},  // Processor Brand String - part 1
    {0x80000003, execute_8000_0003},  // Processor Brand String - part 2
    {0x80000004, execute_8000_0003},  // Processor Brand String - part 3
    {0x80000006, execute_8000_0006},
    {0x80000008, execute_8000_0008}   // Virtual/Physical Address Size
};
// ________
// 03H        Reserved
// 04H        Deterministic Cache Parameters
//            [31:26] cores per package - 1
// 05H        MONITOR/MWAIT
//            Unsupported because feat_monitor is not set
// 06H        Thermal and Power Management
// 07H        Structured Extended Feature Flags
//            Cannot use host values, i.e., 'execute' cannot be NULL.
// 08H        Undefined
// 09H        Direct Cache Access Information
// 0bH        Extended Topology Enumeration Leaf
// 0dH        Processor Extended State Enumeration
// 14H        Intel Processor Trace Enumeration
// 15H        'NULL' means to use host values by default
// 80000005H  Reserved

#define CPUID_TOTAL_LEAVES sizeof(kCpuidManager)/sizeof(kCpuidManager[0])

static const cpuid_controller_t kCpuidController[] = {
    {0x00000001, set_leaf_0000_0001},
    {0x00000015, set_leaf_0000_0015},
    {0x00000016, set_leaf_0000_0016},
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
    hax_cpuid_entry *host_supported, *hax_supported;

    // Initialize host supported features
    host_supported = cache.host_supported;
    host_supported[0] = (hax_cpuid_entry){
        .function = 0x01,
        .ecx = cache.data[0],
        .edx = cache.data[1]
    };
    host_supported[1] = (hax_cpuid_entry){
        .function = 0x80000001,
        .ecx = cache.data[4],
        .edx = cache.data[5]
    };

    hax_log(HAX_LOGI, "%s: host supported features:\n", __func__);
    dump_features(host_supported, CPUID_FEATURE_SET_SIZE);

    // Initialize HAXM supported features
    hax_supported = cache.hax_supported;
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

uint32_t cpuid_guest_get_size(void)
{
    return sizeof(uint32_t) + CPUID_TOTAL_LEAVES * sizeof(hax_cpuid_entry);
}

void cpuid_guest_init(hax_cpuid_t *cpuid)
{
    int i;
    const cpuid_manager_t *cpuid_manager;
    cpuid_args_t args;
    hax_cpuid_entry *entry;

    cpuid->features_mask = ~0ULL;

    // Cache the supported CPUIDs in the 'vcpu->guest_cpuid' buffer. These
    // cached values will be returned directly when CPUID instructions are
    // issued from the guest OS.
    for (i = 0; i < CPUID_TOTAL_LEAVES; ++i) {
        cpuid_manager = &kCpuidManager[i];

        args.eax = cpuid_manager->leaf;
        args.ecx = 0;

        if (cpuid_manager->execute != NULL) {
            // Guest values or processed host values
            cpuid_manager->execute(&args);
        } else {
            // Host values
            asm_cpuid(&args);
        }

        entry = &cpuid->features[i];

        entry->function = cpuid_manager->leaf;
        entry->index    = 0;
        entry->flags    = 0;
        entry->eax      = args.eax;
        entry->ebx      = args.ebx;
        entry->ecx      = args.ecx;
        entry->edx      = args.edx;
    }

    dump_features(cpuid->features, CPUID_TOTAL_LEAVES);
}

void cpuid_execute(hax_cpuid_t *cpuid, cpuid_args_t *args)
{
    int i;
    uint32_t leaf, subleaf;
    hax_cpuid_entry *entry, *supported = NULL;

    if (cpuid == NULL || args == NULL)
        return;

    leaf = args->eax;
    subleaf = args->ecx;

    for (i = 0; i < CPUID_TOTAL_LEAVES; ++i) {
        entry = &cpuid->features[i];
        if (entry->function == leaf) {
            supported = entry;
            break;
        }
    }

    // Return guest values cached during the initialization phase. If the CPUID
    // leaf cannot be found, i.e., out of the kCpuidManager list, the processing
    // is undecided:
    // * Call get_guest_cache() with NULL to return all zeroes;
    // * Call asm_cpuid() to return host values.
    get_guest_cache(args, supported);

    hax_log(HAX_LOGD, "CPUID %08x %08x: %08x %08x %08x %08x\n", leaf, subleaf,
            args->eax, args->ebx, args->ecx, args->edx);
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

    entry = find_cpuid_entry(cpuid->features, CPUID_TOTAL_LEAVES,
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
    dump_features(cpuid->features, CPUID_TOTAL_LEAVES);

    for (i = 0; i < CPUID_TOTAL_CONTROLS; ++i) {
        set_feature(cpuid->features, cpuid_info, &kCpuidController[i]);
    }

    hax_log(HAX_LOGI, "%s: after:\n", __func__);
    dump_features(cpuid->features, CPUID_TOTAL_LEAVES);
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

static void get_guest_cache(cpuid_args_t *args, hax_cpuid_entry *entry)
{
    if (args == NULL)
        return;

    if (entry == NULL) {
        args->eax = args->ebx = args->ecx = args->edx = 0;
        return;
    }

    args->eax = entry->eax;
    args->ebx = entry->ebx;
    args->ecx = entry->ecx;
    args->edx = entry->edx;
}

static void adjust_0000_0001(cpuid_args_t *args)
{
#define VIRT_FAMILY    0x06
#define VIRT_MODEL     0x1f
#define VIRT_STEPPING  0x01
    hax_cpuid_entry *hax_supported;
    uint32_t hw_family;
    uint32_t hw_model;
    // In order to avoid the initialization of unnecessary extended features in
    // the Kernel for emulator (such as the snbep performance monitoring feature
    // in Xeon E5 series system, and the initialization of this feature crashes
    // the emulator), when the hardware family ID is equal to 6 and hardware
    // model ID is greater than 0x1f, we virtualize the returned eax to 0x106f1,
    // that is an old i7 system, so the emulator can still utilize the enough
    // extended features of the hardware, but doesn't crash.
    union cpuid_01h_eax {
        uint32_t raw;
        struct {
            uint32_t stepping_id     : 4;
            uint32_t model           : 4;
            uint32_t family_id       : 4;
            uint32_t processor_type  : 2;
            uint32_t reserved        : 2;
            uint32_t ext_model_id    : 4;
            uint32_t ext_family_id   : 8;
            uint32_t reserved2       : 4;
        };
    } eax;

    if (args == NULL)
        return;

    hax_supported = &cache.hax_supported[0];

    eax.raw = args->eax;

    hw_family = (eax.family_id != 0xf) ? eax.family_id
                : eax.family_id + (eax.ext_family_id << 4);
    hw_model = (eax.family_id == 0x6 || eax.family_id == 0xf)
               ? (eax.ext_model_id << 4) + eax.model : eax.model;

    if (hw_family == VIRT_FAMILY && hw_model > VIRT_MODEL) {
        args->eax = ((VIRT_FAMILY & 0xff0) << 16) | ((VIRT_FAMILY & 0xf) << 8) |
                    ((VIRT_MODEL & 0xf0) << 12) | ((VIRT_MODEL & 0xf) << 4) |
                    (VIRT_STEPPING & 0xf);
    }

    // Report all threads in one package XXXXX vapic currently, we hardcode it
    // to the maximal number of vcpus, but we should see the code in QEMU to
    // vapic initialization.
    args->ebx =
        // Bits 31..16 are hard-coded, with the original author's reasoning
        // given in the above comment. However, these values are not suitable
        // for SMP guests.
        // TODO: Use QEMU's values instead
        // EBX[31..24]: Initial APIC ID
        // EBX[23..16]: Maximum number of addressable IDs for logical processors
        // in this physical package
        (0x01 << 16) |
        // EBX[15..8]: CLFLUSH line size
        // Report a 64-byte CLFLUSH line size as QEMU does
        (0x08 << 8) |
        // EBX[7..0]: Brand index
        // 0 indicates that brand identification is not supported
        // (see IA SDM Vol. 3A 3.2, Table 3-14)
        0x00;

    // Report only the features specified, excluding any features not supported
    // by the host CPU, but including "hypervisor", which is desirable for VMMs.
    // TBD: This will need to be changed to emulate new features.
    args->ecx = (args->ecx & hax_supported->ecx) | FEATURE(HYPERVISOR);
    args->edx &= hax_supported->edx;
}

static void adjust_8000_0001(cpuid_args_t *args)
{
    hax_cpuid_entry *hax_supported;

    if (args == NULL)
        return;

    hax_supported = &cache.hax_supported[1];

    args->eax = args->ebx = 0;
    // Report only the features specified but turn off any features this
    // processor doesn't support.
    args->ecx &= hax_supported->ecx;
    args->edx &= hax_supported->edx;
}

static void execute_0000_0000(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    asm_cpuid(args);
    args->eax = (args->eax < MAX_BASIC_CPUID) ? args->eax : MAX_BASIC_CPUID;
}

static void execute_0000_0001(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    asm_cpuid(args);
    adjust_0000_0001(args);
}

static void execute_0000_0002(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    // These hard-coded values are questionable
    // TODO: Use QEMU's values instead
    args->eax = 0x03020101;
    args->ebx = 0;
    args->ecx = 0;
    args->edx = 0x0c040844;
}

static void execute_0000_000a(cpuid_args_t *args)
{
    struct cpu_pmu_info *pmu_info = &hax->apm_cpuid_0xa;

    if (args == NULL)
        return;

    args->eax = pmu_info->cpuid_eax;
    args->ebx = pmu_info->cpuid_ebx;
    args->ecx = 0;
    args->edx = pmu_info->cpuid_edx;
}

static void execute_4000_0000(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    // Most VMMs, including KVM, Xen, VMware and Hyper-V, use this unofficial
    // CPUID leaf, in conjunction with the "hypervisor" feature flag (c.f. case
    // 1 above), to identify themselves to the guest OS, in a similar manner to
    // CPUID leaf 0 for the CPU vendor ID. HAXM should return its own VMM vendor
    // ID, even though no guest OS recognizes it, because it may be running as a
    // guest VMM on top of another VMM such as KVM or Hyper-V, in which case
    // EBX, ECX and EDX represent the underlying VMM's vendor ID and should be
    // overridden.
    static const char kSignature[13] = "HAXMHAXMHAXM";
    const uint32_t *kVendorId = (const uint32_t *)kSignature;
    // Some VMMs use EAX to indicate the maximum CPUID leaf valid for the range
    // of [0x40000000, 0x4fffffff]
    args->eax = 0x40000000;
    args->ebx = kVendorId[0];
    args->ecx = kVendorId[1];
    args->edx = kVendorId[2];
}

static void execute_8000_0000(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    args->eax = MAX_EXTENDED_CPUID;
    args->ebx = args->ecx = args->edx = 0;
}

static void execute_8000_0001(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    asm_cpuid(args);
    adjust_8000_0001(args);
}

// Hard-coded following two Processor Brand String functions (0x80000002 and
// 0x80000003*) to report "Virtual CPU" at the middle of the CPU info string in
// the Kernel to indicate that the system is virtualized to run the emulator.
// * 0x80000004 shares 0x80000003 as they are same.

static void execute_8000_0002(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    args->eax = 0x74726956;
    args->ebx = 0x206c6175;
    args->ecx = 0x20555043;
    args->edx = 0x00000000;
}

static void execute_8000_0003(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    args->eax = args->ebx = args->ecx = args->edx = 0;
}

static void execute_8000_0006(cpuid_args_t *args)
{
    if (args == NULL)
        return;

    args->eax = args->ebx = args->edx = 0;
    args->ecx = 0x04008040;
}

static void execute_8000_0008(cpuid_args_t *args)
{
    uint8_t physical_address_size;

    if (args == NULL)
        return;

    asm_cpuid(args);
    // Bit mask to identify the reserved bits in paging structure high order
    // address field
    physical_address_size = (uint8_t)args->eax & 0xff;
    pw_reserved_bits_high_mask = ~((1 << (physical_address_size - 32)) - 1);

    args->ebx = args->ecx = args->edx = 0;
}

static void set_feature(hax_cpuid_entry *features, hax_cpuid *cpuid_info,
                        const cpuid_controller_t *cpuid_controller)
{
    hax_cpuid_entry *dest, *src;
    uint32_t leaf;

    if (features == NULL || cpuid_info == NULL)
        return;

    leaf = cpuid_controller->leaf;

    dest = find_cpuid_entry(features, CPUID_TOTAL_LEAVES, leaf, 0);
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
    cpuid_args_t args;
    const uint32_t kFixedFeatures =
        FEATURE(MCE)  |
        FEATURE(APIC) |
        FEATURE(MTRR) |
        FEATURE(PAT);

    if (dest == NULL || src == NULL)
        return;

    args.eax = src->eax;
    args.ebx = src->ebx;
    args.ecx = src->ecx;
    args.edx = src->edx;

    adjust_0000_0001(&args);

    dest->eax = args.eax;
    dest->ebx = args.ebx;
    dest->ecx = args.ecx;
    dest->edx = args.edx | kFixedFeatures;
}

static void set_leaf_0000_0015(hax_cpuid_entry *dest, hax_cpuid_entry *src)
{
    if (dest == NULL || src == NULL)
        return;

    if (src->eax == 0 || src->ebx == 0) {
        hax_log(HAX_LOGE, "%s: invalid values for CPUID.15H.\n", __func__);
        return;
    }

    *dest = *src;
}

static void set_leaf_0000_0016(hax_cpuid_entry *dest, hax_cpuid_entry *src)
{
    if (dest == NULL || src == NULL)
        return;

    if (src->eax == 0 || src->ebx == 0 || src->ecx == 0) {
        hax_log(HAX_LOGE, "%s: invalid values for CPUID.16H.\n", __func__);
        return;
    }

    // Processor Base Frequency (in MHz)
    dest->eax = src->eax & 0xffff;
    // Maximum Frequency (in MHz)
    dest->ebx = src->ebx & 0xffff;
    // Bus (Reference) Frequency (in MHz)
    dest->ecx = src->ecx & 0xffff;
    // Reserved
    dest->edx = 0;
    // (see Intel SDM Vol. 2A 3.2, Table 3-8).
}

static void set_leaf_8000_0001(hax_cpuid_entry *dest, hax_cpuid_entry *src)
{
    cpuid_args_t args;

    if (dest == NULL || src == NULL)
        return;

    args.eax = src->eax;
    args.ebx = src->ebx;
    args.ecx = src->ecx;
    args.edx = src->edx;

    adjust_8000_0001(&args);

    dest->eax = args.eax;
    dest->ebx = args.ebx;
    dest->ecx = args.ecx;
    dest->edx = args.edx;
}
