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

#ifndef HAX_INTERFACE_H_
#define HAX_INTERFACE_H_

/*
 * The interface to QEMU, notice:
 * 1) not include any file other than top level include
 * 2) will be shared by QEMU and kernel
 */

#include "hax_types.h"

#ifdef HAX_PLATFORM_DARWIN
#include "darwin/hax_interface_mac.h"
#endif
#ifdef HAX_PLATFORM_LINUX
#include "linux/hax_interface_linux.h"
#endif
#ifdef HAX_PLATFORM_NETBSD
#include "netbsd/hax_interface_netbsd.h"
#endif
#ifdef HAX_PLATFORM_WINDOWS
#include "windows/hax_interface_windows.h"
#endif

#include "vcpu_state.h"

struct vmx_msr {
    uint64_t entry;
    uint64_t value;
} PACKED;

/* fx_layout has 3 formats table 3-56, 512bytes */
struct fx_layout {
    uint16_t  fcw;
    uint16_t  fsw;
    uint8_t   ftw;
    uint8_t   res1;
    uint16_t  fop;
    union {
        struct {
            uint32_t  fip;
            uint16_t  fcs;
            uint16_t  res2;
        };
        uint64_t  fpu_ip;
    };
    union {
        struct {
            uint32_t  fdp;
            uint16_t  fds;
            uint16_t  res3;
        };
        uint64_t  fpu_dp;
    };
    uint32_t  mxcsr;
    uint32_t  mxcsr_mask;
    uint8_t   st_mm[8][16];
    uint8_t   mmx_1[8][16];
    uint8_t   mmx_2[8][16];
    uint8_t   pad[96];
} ALIGNED(16);

/*
 * TODO: Fixed array is stupid, but it makes Mac support a bit easier, since we
 * can avoid the memory map or copyin staff. We need to fix it in future.
 */

#define HAX_MAX_MSR_ARRAY 0x20
struct hax_msr_data {
    uint16_t nr_msr;
    uint16_t done;
    uint16_t pad[2];
    struct vmx_msr entries[HAX_MAX_MSR_ARRAY];
} PACKED;

#define HAX_IO_OUT 0
#define HAX_IO_IN  1

/* The area to communicate with device model */
struct hax_tunnel {
    uint32_t _exit_reason;
    uint32_t pad0;
    uint32_t _exit_status;
    uint32_t user_event_pending;
    int ready_for_interrupt_injection;
    int request_interrupt_window;

    union {
        struct {
            uint8_t _direction;
            uint8_t _df;
            uint16_t _size;
            uint16_t _port;
            uint16_t _count;
            /* Followed owned by HAXM, QEMU should not touch them */
            /* bit 1 is 1 means string io */
            uint8_t _flags;
            uint8_t _pad0;
            uint16_t _pad1;
            uint32_t _pad2;
            hax_vaddr_t _vaddr;
        } io;
        struct {
            hax_paddr_t gla;
        } mmio;
        struct {
            hax_paddr_t gpa;
#define HAX_PAGEFAULT_ACC_R  (1 << 0)
#define HAX_PAGEFAULT_ACC_W  (1 << 1)
#define HAX_PAGEFAULT_ACC_X  (1 << 2)
#define HAX_PAGEFAULT_PERM_R (1 << 4)
#define HAX_PAGEFAULT_PERM_W (1 << 5)
#define HAX_PAGEFAULT_PERM_X (1 << 6)
            uint32_t flags;
            uint32_t reserved1;
            uint64_t reserved2;
        } pagefault;
        struct {
            hax_paddr_t dummy;
        } state;
        struct {
            uint64_t rip;
            uint64_t dr6;
            uint64_t dr7;
        } debug;
    };
    uint64_t apic_base;
} PACKED;

struct hax_fastmmio {
    hax_paddr_t gpa;
    union {
        uint64_t value;
        hax_paddr_t gpa2;  /* since API v4 */
    };
    uint8_t size;
    uint8_t direction;
    uint16_t reg_index;  /* obsolete */
    uint32_t pad0;
    uint64_t _cr0;
    uint64_t _cr2;
    uint64_t _cr3;
    uint64_t _cr4;
} PACKED;

struct hax_module_version {
    uint32_t compat_version;
    uint32_t cur_version;
} PACKED;

#define HAX_CAP_STATUS_WORKING     (1 << 0)
#define HAX_CAP_MEMQUOTA           (1 << 1)
#define HAX_CAP_WORKSTATUS_MASK    0x01

#define HAX_CAP_FAILREASON_VT      (1 << 0)
#define HAX_CAP_FAILREASON_NX      (1 << 1)

#define HAX_CAP_EPT                (1 << 0)
#define HAX_CAP_FASTMMIO           (1 << 1)
#define HAX_CAP_UG                 (1 << 2)
#define HAX_CAP_64BIT_RAMBLOCK     (1 << 3)
#define HAX_CAP_64BIT_SETRAM       (1 << 4)
#define HAX_CAP_TUNNEL_PAGE        (1 << 5)
#define HAX_CAP_RAM_PROTECTION     (1 << 6)
#define HAX_CAP_DEBUG              (1 << 7)
#define HAX_CAP_IMPLICIT_RAMBLOCK  (1 << 8)
#define HAX_CAP_CPUID              (1 << 9)

struct hax_capabilityinfo {
    /*
     * bit 0: 1 - working, 0 - not working, possibly because NT/NX disabled
     * bit 1: 1 - memory limitation working, 0 - no memory limitation
     */
    uint16_t wstatus;
    /*
     * valid when not working
     * bit0: VT not enabeld
     * bit1: NX not enabled
     */
    /*
     * valid when working
     * bit0: EPT enabled
     * bit1: fastMMIO
     */
    uint16_t winfo;
    uint32_t win_refcount;
    uint64_t mem_quota;
} PACKED;

struct hax_tunnel_info {
    uint64_t va;
    uint64_t io_va;
    uint16_t size;
    uint16_t pad[3];
} PACKED;

struct hax_set_memlimit {
    uint8_t enable_memlimit;
    uint8_t pad[7];
    uint64_t memory_limit;
} PACKED;

struct hax_alloc_ram_info {
    uint32_t size;
    uint32_t pad;
    uint64_t va;
} PACKED;

struct hax_ramblock_info {
    uint64_t start_va;
    uint64_t size;
    uint64_t reserved;
} PACKED;

// Read-only mapping
#define HAX_RAM_INFO_ROM (1 << 0)
// Stand-alone mapping into a new HVA range
#define HAX_RAM_INFO_STANDALONE (1 << 6)

// Unmapped, usually used for MMIO
#define HAX_RAM_INFO_INVALID (1 << 7)

struct hax_set_ram_info {
    uint64_t pa_start;
    uint32_t size;
    uint8_t flags;
    uint8_t pad[3];
    uint64_t va;
} PACKED;

struct hax_set_ram_info2 {
    uint64_t pa_start;
    uint64_t size;
    uint64_t va;
    uint32_t flags;
    uint32_t reserved1;
    uint64_t reserved2;
} PACKED;

// No access (R/W/X) is allowed
#define HAX_RAM_PERM_NONE 0x0
// All accesses (R/W/X) are allowed
#define HAX_RAM_PERM_RWX  0x7
#define HAX_RAM_PERM_MASK 0x7
struct hax_protect_ram_info {
    uint64_t pa_start;
    uint64_t size;
    uint32_t flags;
    uint32_t reserved;
} PACKED;

/* This interface is support only after API version 2 */
struct hax_qemu_version {
    /* Current API version in QEMU*/
    uint32_t cur_version;
    /* The least API version supported by QEMU */
    uint32_t least_version;
} PACKED;

#define HAX_DEBUG_ENABLE     (1 << 0)
#define HAX_DEBUG_STEP       (1 << 1)
#define HAX_DEBUG_USE_SW_BP  (1 << 2)
#define HAX_DEBUG_USE_HW_BP  (1 << 3)

struct hax_debug_t {
    uint32_t control;
    uint32_t reserved;
    uint64_t dr[8];
} PACKED;

#define HAX_MAX_CPUID_ENTRIES 0x40

typedef struct hax_cpuid_entry {
    uint32_t function;
    uint32_t index;
    uint32_t flags;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t pad[3];
} hax_cpuid_entry;

// `hax_cpuid` is a variable-length type. The size of `hax_cpuid` itself is only
// 8 bytes. `entries` is just a body placeholder, which will not actually occupy
// memory. The accessible memory of `entries` is decided by the allocation from
// user space, and the array length is specified by `total`.

typedef struct hax_cpuid {
    uint32_t total;
    uint32_t pad;
    hax_cpuid_entry entries[0];
} hax_cpuid;

#endif  // HAX_INTERFACE_H_
