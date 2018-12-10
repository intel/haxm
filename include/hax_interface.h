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

#define HAX_IOCTL_PLATFORM   0x40
#define HAX_IOCTL_EXTENSION  0x80

/* Legacy API
 * TODO: Remove all legacy calls after grace period (2020-01-01).
 */
#define HAX_IOCTL_VERSION__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x20, 0x900, struct hax_module_version)
#define HAX_IOCTL_CREATE_VM__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x21, 0x901, uint32_t)
#define HAX_IOCTL_DESTROY_VM__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOW,  0x22, 0x902, uint32_t)
#define HAX_IOCTL_CAPABILITY__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOR,  0x23, 0x910, struct hax_capabilityinfo)
#define HAX_IOCTL_SET_MEMLIMIT__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x24, 0x911, struct hax_set_memlimit)

#define HAX_VM_IOCTL_VCPU_CREATE__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x80, 0x902, uint32_t)
#define HAX_VM_IOCTL_ALLOC_RAM__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x81, 0x903, struct hax_alloc_ram_info)
#define HAX_VM_IOCTL_SET_RAM__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x82, 0x904, struct hax_set_ram_info)
#define HAX_VM_IOCTL_VCPU_DESTROY__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOR,  0x83, 0x905, uint32_t)
#define HAX_VM_IOCTL_ADD_RAMBLOCK__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOW,  0x85, 0x913, struct hax_ramblock_info)
#define HAX_VM_IOCTL_SET_RAM2__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x86, 0x914, struct hax_set_ram_info2)
#define HAX_VM_IOCTL_PROTECT_RAM__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0x87, 0x915, struct hax_protect_ram_info)

#define HAX_VCPU_IOCTL_RUN__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IO,   0xc0, 0x906, HAX_UNUSED)
#define HAX_VCPU_IOCTL_SETUP_TUNNEL__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0xc5, 0x90b, struct hax_tunnel_info)
#define HAX_VCPU_IOCTL_GET_REGS__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0xc8, 0x90e, struct vcpu_state_t)
#define HAX_VCPU_IOCTL_SET_REGS__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0xc7, 0x90d, struct vcpu_state_t)
#define HAX_VCPU_IOCTL_GET_FPU__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOR,  0xc4, 0x90a, struct fx_layout)
#define HAX_VCPU_IOCTL_SET_FPU__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOW,  0xc3, 0x909, struct fx_layout)
#define HAX_VCPU_IOCTL_GET_MSRS__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0xc2, 0x908, struct hax_msr_data)
#define HAX_VCPU_IOCTL_SET_MSRS__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0xc1, 0x907, struct hax_msr_data)
#define HAX_VCPU_IOCTL_INTERRUPT__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOWR, 0xc6, 0x90c, uint32_t)

// API 2.0
#define HAX_VM_IOCTL_NOTIFY_QEMU_VERSION__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOW,  0x84, 0x910, struct hax_qemu_version)
#define HAX_VCPU_IOCTL_DEBUG__LEGACY \
    HAX_LEGACY_IOCTL(HAX_IOW,  0xc9, 0x916, struct hax_debug_t)

/* API
 * ===
 * Each platform generates their own IOCTL-value by using the macro
 * HAX_IOCTL(access, code, type) with the following arguments:
 * - access: Arguments usage from userland perspective.
 *   - HAX_IO:    Driver ignores user arguments.
 *   - HAX_IOR:   Driver writes user arguments (read by user).
 *   - HAX_IOW:   Driver reads user arguments (written by user).
 *   - HAX_IOWR:  Driver reads+writes user arguments (written+read by user).
 * - code: Sequential number in range 0x00-0x3F, and maskable via:
 *   - HAX_IOCTL_PLATFORM  (0x40)  Platform-specific ioctl.
 *   - HAX_IOCTL_EXTENSION (0x80)  Extension-specific ioctl.
 * - type: User argument type.
 */
#define HAX_IOCTL_GET_API_VERSION \
    HAX_IOCTL(HAX_IOR,  0x00, struct hax_module_version)
#define HAX_IOCTL_CREATE_VM \
    HAX_IOCTL(HAX_IOR,  0x01, uint32_t)
#define HAX_IOCTL_DESTROY_VM \
    HAX_IOCTL(HAX_IOW,  0x02, uint32_t)
#define HAX_IOCTL_CAPABILITY \
    HAX_IOCTL(HAX_IOR,  0x03, struct hax_capabilityinfo)

#define HAX_VM_IOCTL_CREATE_VCPU \
    HAX_IOCTL(HAX_IOW,  0x00, uint32_t)
#define HAX_VM_IOCTL_DESTROY_VCPU \
    HAX_IOCTL(HAX_IOW,  0x01, uint32_t)
#define HAX_VM_IOCTL_SET_RAM \
    HAX_IOCTL(HAX_IOW,  0x02, struct hax_set_ram_info)
#define HAX_VM_IOCTL_ADD_RAMBLOCK \
    HAX_IOCTL(HAX_IOW,  0x03, struct hax_ramblock_info)
#define HAX_VM_IOCTL_SET_RAM2 \
    HAX_IOCTL(HAX_IOW,  0x04, struct hax_set_ram_info2)
#define HAX_VM_IOCTL_PROTECT_RAM \
    HAX_IOCTL(HAX_IOW,  0x05, struct hax_protect_ram_info)

#define HAX_VCPU_IOCTL_RUN \
    HAX_IOCTL(HAX_IO,   0x00, HAX_UNUSED)
#define HAX_VCPU_IOCTL_SETUP_TUNNEL \
    HAX_IOCTL(HAX_IOR,  0x01, struct hax_tunnel_info)
#define HAX_VCPU_IOCTL_GET_REGS \
    HAX_IOCTL(HAX_IOR,  0x02, struct vcpu_state_t)
#define HAX_VCPU_IOCTL_SET_REGS \
    HAX_IOCTL(HAX_IOW,  0x03, struct vcpu_state_t)
#define HAX_VCPU_IOCTL_GET_FPU \
    HAX_IOCTL(HAX_IOR,  0x04, struct fx_layout)
#define HAX_VCPU_IOCTL_SET_FPU \
    HAX_IOCTL(HAX_IOW,  0x05, struct fx_layout)
#define HAX_VCPU_IOCTL_GET_MSRS \
    HAX_IOCTL(HAX_IOWR, 0x06, struct hax_msr_data)
#define HAX_VCPU_IOCTL_SET_MSRS \
    HAX_IOCTL(HAX_IOWR, 0x07, struct hax_msr_data)
#define HAX_VCPU_IOCTL_INTERRUPT \
    HAX_IOCTL(HAX_IOW,  0x08, uint32_t)
#define HAX_VCPU_IOCTL_DEBUG \
    HAX_IOCTL(HAX_IOW,  0x09, struct hax_debug_t)

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

#define HAX_RAM_INFO_ROM     0x01  // read-only
#define HAX_RAM_INFO_INVALID 0x80  // unmapped, usually used for MMIO

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

#endif  // HAX_INTERFACE_H_
