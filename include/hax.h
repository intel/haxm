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

// EPT2 refers to the new memory virtualization engine, which implements lazy
// allocation, and therefore greatly speeds up ALLOC_RAM and SET_RAM VM ioctls
// as well as brings down HAXM driver's memory footprint. It is mostly written
// in new source files (including core/include/memory.h, core/include/ept2.h,
// include/hax_host_mem.h and their respective .c/.cpp files), separate from
// the code for the legacy memory virtualization engine (which is scattered
// throughout core/memory.c, core/vm.c, core/ept.c, etc.). This makes it
// possible to select between the two engines at compile time, simply by
// defining (which selects the new engine) or undefining (which selects the old
// engine) the following macro.
// TODO: Completely remove the legacy engine and this macro when the new engine
// is considered stable.
#define CONFIG_HAX_EPT2

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

uint64_t get_hpfn_from_pmem(struct hax_vcpu_mem *pmem, uint64_t va);

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
 * Map the physical address into kernel address space
 * XXX please don't use this function for long-time map.
 * in Mac side, we utilize the IOMemoryDescriptor class to map this, and the
 * object have to be kept in a list till the vunmap. And when we do the vunmap,
 * we need search the list again, thus it will cost memory/performance issue
 */
void *hax_vmap(hax_pa_t pa, uint32_t size);
static inline void * hax_vmap_pfn(hax_pfn_t pfn)
{
    return hax_vmap(pfn << HAX_PAGE_SHIFT, HAX_PAGE_SIZE);
}

/*
 * unmap the memory mapped above
 */
void hax_vunmap(void *va, uint32_t size);
static inline void hax_vunmap_pfn(void *va)
{
    hax_vunmap((void*)((mword)va & ~HAX_PAGE_MASK), HAX_PAGE_SIZE);
}

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

#ifdef __cplusplus
}
#endif

/* Caller must ensure the page has been mapped alreay before going here. */
static inline unsigned char *hax_page_va(struct hax_page *page)
{
    return (unsigned char *)page->kva;
}

#define HAX_MAX_CPUS (sizeof(uint64_t) * 8)

/* Host SMP */
extern hax_cpumap_t cpu_online_map;
extern int max_cpus;

#ifdef __cplusplus
extern "C" {
#endif

int hax_smp_call_function(hax_cpumap_t *cpus, void(*scfunc)(void *param),
                          void *param);

uint32_t hax_cpuid(void);
int proc_event_pending(struct vcpu_t *vcpu);

void hax_disable_preemption(preempt_flag *eflags);
void hax_enable_preemption(preempt_flag *eflags);

void hax_enable_irq(void);
void hax_disable_irq(void);

int hax_em64t_enabled(void);

#ifdef __cplusplus
}
#endif

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

#endif  // HAX_H_
