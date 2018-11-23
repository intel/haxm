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

#ifndef HAX_TYPES_H_
#define HAX_TYPES_H_

/* Detect architecture */
// x86 (32-bit)
#if defined(__i386__) || defined(_M_IX86)
#define HAX_ARCH_X86_32
#define ASMCALL __cdecl
// x86 (64-bit)
#elif defined(__x86_64__) || defined(_M_X64)
#define HAX_ARCH_X86_64
#define ASMCALL
#else
#error "Unsupported architecture"
#endif

/* Detect compiler */
// Clang
#if defined(__clang__)
#define HAX_COMPILER_CLANG
#define PACKED     __attribute__ ((packed))
#define ALIGNED(x) __attribute__ ((aligned(x)))
// GCC
#elif defined(__GNUC__)
#define HAX_COMPILER_GCC
#define PACKED     __attribute__ ((packed))
#define ALIGNED(x) __attribute__ ((aligned(x)))
#define __cdecl    __attribute__ ((__cdecl__,regparm(0)))
#define __stdcall  __attribute__ ((__stdcall__))
// MSVC
#elif defined(_MSC_VER)
#define HAX_COMPILER_MSVC
// FIXME: MSVC doesn't have a simple equivalent for PACKED.
//        Instead, The corresponding #pragma directives are added manually.
#define PACKED     
#define ALIGNED(x) __declspec(align(x))
#else
#error "Unsupported compiler"
#endif

/* Detect platform */
#ifndef HAX_TESTS // Prevent kernel-only exports from reaching userland code
// MacOS
#if defined(__MACH__)
#define HAX_PLATFORM_DARWIN
#include "darwin/hax_types_mac.h"
// Linux
#elif defined(__linux__)
#define HAX_PLATFORM_LINUX
#include "linux/hax_types_linux.h"
// NetBSD
#elif defined(__NetBSD__)
#define HAX_PLATFORM_NETBSD
#include "netbsd/hax_types_netbsd.h"
// Windows
#elif defined(_WIN32)
#define HAX_PLATFORM_WINDOWS
#include "windows/hax_types_windows.h"
#else
#error "Unsupported platform"
#endif
#else // !HAX_TESTS
#include <stdint.h>
#endif // HAX_TESTS

#define HAX_PAGE_SIZE  4096
#define HAX_PAGE_SHIFT 12
#define HAX_PAGE_MASK  0xfff

/* Common typedef for all platforms */
typedef uint64_t hax_pa_t;
typedef uint64_t hax_pfn_t;
typedef uint64_t hax_paddr_t;
typedef uint64_t hax_vaddr_t;

#endif  // HAX_TYPES_H_
