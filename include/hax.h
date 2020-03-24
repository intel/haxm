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

#ifndef HAX_H_
#define HAX_H_

#include "hax_types.h"
#include "hax_list.h"
#include "hax_interface.h"

// TODO: Refactor proc_event_pending(), and then delete the following forward
// declaration
struct vcpu_t;

#define HAX_CUR_VERSION    0x0004
#define HAX_COMPAT_VERSION 0x0001

/* TBD */
#define for_each_vcpu(vcpu, vm)

/* Memory allocation flags */
/* The allocated memory will not be swapped out */
#define HAX_MEM_NONPAGE 0x1
/*
 * The allocated memory can be swapped out and
 * can't be used in critical code like ISR
 */
#define HAX_MEM_PAGABLE 0x2
/* below 4G */
// !!!! This will only for hax_page allocation to simplify handling
#define HAX_MEM_LOW_4G  0x4
/* The allocated memory will be physically continuous */
// !!!! Since no one use this flag, remove it
//#define HAX_MEM_CONTI  0x8
/* The allocation will not be blocked, thus works for interrupt handling */
/* XXX not sure how to achieve this, and if we have requirement for it */
#define HAX_MEM_NONBLOCK 0x10

/* When guest RAM size is >= 3.5GB, part of it is assigned GPAs above 4GB.
 * TODO: Support more than 4GB of guest RAM */
#define MAX_GMEM_G 5

/*
 * Take care of memory allocation function call. Unless specially stated, they
 * can't be used in interrupt/bottom half situation. Different Host OS have
 * different bottom half definition, like DPC level in Windows OS.
 */

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Allocate memory, which is mapped in kernel space already
 * alignment is cache line size alignment
 * NB: If flags set 0, will be NONPAGE, i.e. the memory is not continuous,
 * can be above 4G, allocation can be blocked
 * return mapped virtual address in kernel space, or NULL if OOM
 */
void *hax_vmalloc(uint32_t size, uint32_t flags);
/*
 * Free memory allocated with above function
 * this can only be used when va is allocated with hax_vmalloc() and flags == 0,
 * which should be most common situation
 */
void hax_vfree(void *va, uint32_t size);

void hax_vfree_flags(void *va, uint32_t size, uint32_t flags);

/*
 * Allocate aligned memory, which is mapped in kernel space already
 * For example, 256 get memory allocated at an address with bit 0-7 set 0
 * With (flags&HAX_MEM_CONTI) && alignment==PAGE_SIZE, it can allocate
 * continuous physical memory
 */
void *hax_vmalloc_aligned(uint32_t size, uint32_t flags, uint32_t alignment);

void hax_vfree_aligned(void *va, uint32_t size, uint32_t flags,
                       uint32_t alignment);

struct hax_vcpu_mem {
    uint32_t size;
    uint64_t uva;
    void *kva;
    void *hinfo;
};

int hax_clear_vcpumem(struct hax_vcpu_mem *mem);
int hax_setup_vcpumem(struct hax_vcpu_mem *vcpumem, uint64_t uva, uint32_t size,
                      int flags);

#define HAX_VCPUMEM_VALIDVA 0x1

enum hax_notify_event {
    HaxNoVtEvent = 0,
    HaxNoNxEvent,
    HaxNoEMT64Event,
    HaxVtDisable,
    HaxNxDisable,
    HaxVtEnableFailure
};

struct hax_slab_t;
typedef struct hax_slab_t *phax_slab_t;
/*
 * Mac and Windows does not export such slab interface to driver,
 * so it's in fact a dummy slab in mac/windows
 * XXX Currently we don't support the ctor/dtor function,
 * please raise it if needed
 */
phax_slab_t hax_slab_create(char *name, int size);
phax_slab_t hax_slab_alloc(phax_slab_t *type, uint32_t flags);
void hax_slab_free(phax_slab_t *type, void* cache);

/*
 * translate a virtual address in kernel address space to physical address
 * If need support for address space other than kernel map, please raise the
 * requirement
 */
hax_pa_t hax_pa(void *va);

/*
 * unmap the memory mapped above
 */
void hax_vunmap(void *va, uint32_t size);

struct hax_page;
typedef struct hax_page * phax_page;

phax_page hax_alloc_pages(int order, uint32_t flags, bool vmap);
#define hax_alloc_page(flags, vmap) hax_alloc_pages(0, flags, vmap)

void hax_free_pages(struct hax_page *page);

#define hax_free_page(ptr) hax_free_pages(ptr)

/*
 * Create a new hax page for a pfn/pfns
 * vmap decide whether the page should be mapped into kernel address space
 */
phax_page hax_create_pages(int order, uint32_t pfn, bool vmap);

hax_pfn_t hax_page2pfn(phax_page page);

void hax_clear_page(phax_page page);
void hax_set_page(phax_page page);

static inline uint64_t hax_page2pa(phax_page page)
{
    return hax_page2pfn(page) << HAX_PAGE_SHIFT;
}

#define hax_page_pa hax_page2pa
#define hax_page_pfn hax_page2pfn

void *hax_map_page(struct hax_page *page);

void hax_unmap_page(struct hax_page *page);

void hax_log(int level, const char *fmt, ...);
void hax_panic(const char *fmt, ...);

uint32_t hax_cpu_id(void);

#ifdef __cplusplus
}
#endif

/* Caller must ensure the page has been mapped alreay before going here. */
static inline unsigned char *hax_page_va(struct hax_page *page)
{
    return (unsigned char *)page->kva;
}

/* Utilities */
#define HAX_NOLOG       0xff
#define HAX_LOGPANIC    5
#define HAX_LOGE        4
#define HAX_LOGW        3
#define HAX_LOGI        2
#define HAX_LOGD        1
#define HAX_LOG_DEFAULT 3

#ifdef HAX_PLATFORM_DARWIN
#include "darwin/hax_mac.h"
#endif
#ifdef HAX_PLATFORM_LINUX
#include "linux/hax_linux.h"
#endif
#ifdef HAX_PLATFORM_NETBSD
#include "netbsd/hax_netbsd.h"
#endif
#ifdef HAX_PLATFORM_WINDOWS
#include "windows/hax_windows.h"
#endif

#define HAX_MAX_CPU_PER_GROUP (sizeof(hax_cpumask_t) * 8)
#define HAX_MAX_CPU_GROUP ((uint16_t)(~0ULL))
#define HAX_MAX_CPUS (HAX_MAX_CPU_PER_GROUP * HAX_MAX_CPU_GROUP)

typedef struct hax_cpu_group_t {
    hax_cpumask_t map;
    uint32_t num;
    uint16_t id;
} hax_cpu_group_t;

typedef struct hax_cpu_pos_t {
    uint16_t group;
    uint16_t bit;
} hax_cpu_pos_t;

typedef struct hax_cpumap_t {
    hax_cpu_group_t *cpu_map;
    hax_cpu_pos_t *cpu_pos;
    uint16_t group_num;
    uint32_t cpu_num;
} hax_cpumap_t;

typedef struct smp_call_parameter {
    void (*func)(void *);
    void *param;
    hax_cpumap_t *cpus;
} smp_call_parameter;

extern hax_cpumap_t cpu_online_map;

static inline void cpu2cpumap(uint32_t cpu_id, hax_cpu_pos_t *target)
{
    if (!target)
        return;

    if (cpu_id >= cpu_online_map.cpu_num) {
        target->group = (uint16_t)(~0ULL);
        target->bit = (uint16_t)(~0ULL);
    } else {
        target->group = cpu_online_map.cpu_pos[cpu_id].group;
        target->bit = cpu_online_map.cpu_pos[cpu_id].bit;
    }
}

static inline bool cpu_is_online(hax_cpumap_t *cpu_map, uint32_t cpu_id)
{
    hax_cpumask_t map;
    uint16_t group, bit;

    if (cpu_id >= cpu_map->cpu_num) {
        hax_log(HAX_LOGE, "Invalid cpu-%d\n", cpu_id);
        return 0;
    }

    group = cpu_map->cpu_pos[cpu_id].group;
    if (group != cpu_map->cpu_map[group].id) {
        hax_log(HAX_LOGE, "Group id doesn't match record\n", group);
        return 0;
    }

    bit = cpu_map->cpu_pos[cpu_id].bit;
    map = cpu_map->cpu_map[group].map;
    return !!(((hax_cpumask_t)1 << bit) & map);
}

static inline void get_online_map(void *param)
{
    hax_cpumap_t *omap = (hax_cpumap_t *)param;
    hax_cpu_group_t *cpu_map;
    hax_cpu_pos_t * cpu_pos;
    uint32_t cpu_id, group, bit;

    cpu_id = hax_cpu_id();
    group = cpu_id / HAX_MAX_CPU_PER_GROUP;
    bit = cpu_id % HAX_MAX_CPU_PER_GROUP;

    cpu_map = &(omap->cpu_map[group]);
    cpu_pos = &(omap->cpu_pos[cpu_id]);

    hax_test_and_set_bit(bit, &cpu_map->map);
    cpu_map->id = group;
    cpu_pos->group = group;
    cpu_pos->bit = bit;
}

static void cpu_info_exit(void)
{
    if (cpu_online_map.cpu_map)
        hax_vfree(cpu_online_map.cpu_map, cpu_online_map.group_num * sizeof(*cpu_online_map.cpu_map));
    if (cpu_online_map.cpu_pos)
        hax_vfree(cpu_online_map.cpu_pos, cpu_online_map.cpu_num * sizeof(*cpu_online_map.cpu_pos));
    memset(&cpu_online_map, 0, sizeof(cpu_online_map));
}

#ifdef __cplusplus
extern "C" {
#endif

int cpu_info_init(void);
#ifdef HAX_PLATFORM_DARWIN
hax_smp_func_ret_t smp_cfunction(void *param);
#endif
#ifdef HAX_PLATFORM_LINUX
hax_smp_func_ret_t smp_cfunction(void *param);
#endif
#ifdef HAX_PLATFORM_NETBSD
hax_smp_func_ret_t smp_cfunction(void *param, void *a2 __unused);
#endif
#ifdef HAX_PLATFORM_WINDOWS
hax_smp_func_ret_t smp_cfunction(void *param);
#endif

int hax_smp_call_function(hax_cpumap_t *cpus, void(*scfunc)(void *param),
                          void *param);

int proc_event_pending(struct vcpu_t *vcpu);

void hax_disable_preemption(preempt_flag *eflags);
void hax_enable_preemption(preempt_flag *eflags);

void hax_enable_irq(void);
void hax_disable_irq(void);

int hax_em64t_enabled(void);

#ifdef __cplusplus
}
#endif

#endif  // HAX_H_
