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

#ifndef HAX_DARWIN_HAX_INTERFACE_MAC_H_
#define HAX_DARWIN_HAX_INTERFACE_MAC_H_

#include <mach/mach_types.h>

/* The mac specific interface to qemu because of mac's
 * special handling like hax tunnel allocation etc */
/* HAX model level ioctl */
#define HAX_IOCTL_VERSION _IOWR(0, 0x20, struct hax_module_version)
#define HAX_IOCTL_CREATE_VM _IOWR(0, 0x21, uint32_t)
#define HAX_IOCTL_DESTROY_VM _IOW(0, 0x22, uint32_t)
#define HAX_IOCTL_CAPABILITY _IOR(0, 0x23, struct hax_capabilityinfo)
#define HAX_IOCTL_SET_MEMLIMIT _IOWR(0, 0x24, struct hax_set_memlimit)

// Only for backward compatibility with old Qemu.
#define HAX_VM_IOCTL_VCPU_CREATE_ORIG _IOR(0, 0x80, int)

#define HAX_VM_IOCTL_VCPU_CREATE _IOWR(0, 0x80, uint32_t)
#define HAX_VM_IOCTL_ALLOC_RAM _IOWR(0, 0x81, struct hax_alloc_ram_info)
#define HAX_VM_IOCTL_SET_RAM _IOWR(0, 0x82, struct hax_set_ram_info)
#define HAX_VM_IOCTL_VCPU_DESTROY _IOR(0, 0x83, uint32_t)
#define HAX_VM_IOCTL_ADD_RAMBLOCK _IOW(0, 0x85, struct hax_ramblock_info)
#define HAX_VM_IOCTL_SET_RAM2 _IOWR(0, 0x86, struct hax_set_ram_info2)
#define HAX_VM_IOCTL_PROTECT_RAM _IOWR(0, 0x87, struct hax_protect_ram_info)

#define HAX_VCPU_IOCTL_RUN _IO(0, 0xc0)
#define HAX_VCPU_IOCTL_SET_MSRS _IOWR(0, 0xc1, struct hax_msr_data)
#define HAX_VCPU_IOCTL_GET_MSRS _IOWR(0, 0xc2, struct hax_msr_data)

#define HAX_VCPU_IOCTL_SET_FPU _IOW(0, 0xc3, struct fx_layout)
#define HAX_VCPU_IOCTL_GET_FPU _IOR(0, 0xc4, struct fx_layout)

#define HAX_VCPU_IOCTL_SETUP_TUNNEL _IOWR(0, 0xc5, struct hax_tunnel_info)
#define HAX_VCPU_IOCTL_INTERRUPT _IOWR(0, 0xc6, uint32_t)
#define HAX_VCPU_SET_REGS _IOWR(0, 0xc7, struct vcpu_state_t)
#define HAX_VCPU_GET_REGS _IOWR(0, 0xc8, struct vcpu_state_t)

/* API 2.0 */
#define HAX_VM_IOCTL_NOTIFY_QEMU_VERSION _IOW(0, 0x84, struct hax_qemu_version)

#define HAX_IOCTL_VCPU_DEBUG _IOW(0, 0xc9, struct hax_debug_t)

// `hax_cpuid *` is specified as the size of data buffer because `hax_cpuid` is
// a variable-length type. When ioctl() is invoked, the argument of user data
// should pass the address of the pointer to `hax_cpuid`.
#define HAX_VCPU_IOCTL_SET_CPUID _IOW(0, 0xca, struct hax_cpuid *)

#define HAX_KERNEL64_CS 0x80
#define HAX_KERNEL32_CS 0x08
#ifdef __i386__
#define is_compatible() 1
#else
#define is_compatible() 0
#endif

#endif  // HAX_DARWIN_HAX_INTERFACE_MAC_H_
