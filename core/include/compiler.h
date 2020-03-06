/*
 * Copyright (c) 2009 Intel Corporation
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

#ifndef HAX_CORE_COMPILER_H_
#define HAX_CORE_COMPILER_H_

// This file contains macros that will be automagically available in all
// C/C++/asm files.

// #include <stddef.h>

#undef offsetof    // We don't want offsetof from stddef.h

// We define our own version of offsetof.
// It is not standard-compliant but it allows taking the offset of a member of
// a non-POD structure.
// We have to use 1 instead of 0 so the compiler doesn't generate an error.
#define offsetof(type, mem) \
        ((uint32_t)((char *)&((const type *)1)->mem - (char *)((const type *)1)))

#define ALWAYS_INLINE           __attribute__ ((always_inline))
#define NOINLINE                __attribute__ ((noinline))
#define NORETURN                __attribute__ ((noreturn))
#define WEAK                    __attribute__ ((weak))
#define FORMAT(X,Y)             __attribute__ ((format (printf, (X),(Y))))
#define EXPECT_FALSE(X)         __builtin_expect((X), false)
#define EXPECT_TRUE(X)          __builtin_expect((X), true)

// This macro prevents the compiler from caching values across the point of
// call. No instruction is required on x86 (unless SSE is used).

#define ARRAY_ELEMENTS(x)       (sizeof(x)/sizeof((x)[0]))

#define STRINGIFY_(x)           #x
#define STRINGIFY(x)            STRINGIFY_(x)

#endif  // HAX_CORE_COMPILER_H_
