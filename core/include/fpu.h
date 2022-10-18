/*
 * Copyright (c) 2022 Intel Corporation
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

#ifndef HAX_CORE_FPU_H_
#define HAX_CORE_FPU_H_

#include "hax_types.h"

// Intel SDM Vol. 1: 13.1 XSAVE-Supported Features and State-Component Bitmaps
// XSAVE-supported features
enum xfeature {
    // Legacy states
    XFEATURE_FP,                 // x87 state
    XFEATURE_SSE,                // SSE state
    // Extended states
    XFEATURE_YMM,                // AVX state
    XFEATURE_BNDREGS,            // MPX state (BNDREGS)
    XFEATURE_BNDCSR,             // MPX state (BNDCSR)
    XFEATURE_OPMASK,             // AVX-512 state (opmask)
    XFEATURE_ZMM_Hi256,          // AVX-512 state (ZMM_Hi256)
    XFEATURE_Hi16_ZMM,           // AVX-512 state (Hi16_ZMM)
    XFEATURE_PT,                 // PT state
    XFEATURE_PKRU,               // PKRU state
    XFEATURE_PASID,              // PASID state
    XFEATURE_CET_U,              // CET state (CET_U)
    XFEATURE_CET_S,              // CET state (CET_S)
    XFEATURE_HDC,                // HDC state
    XFEATURE_RESERVED,           // Reserved
    XFEATURE_LBR,                // LBR state
    XFEATURE_HWP,                // HWP state
    // State maximum
    XFEATURE_MAX
};

// State-component bitmaps
#define XFEATURE_MASK_FP         (1 << XFEATURE_FP)
#define XFEATURE_MASK_SSE        (1 << XFEATURE_SSE)
#define XFEATURE_MASK_YMM        (1 << XFEATURE_YMM)
#define XFEATURE_MASK_BNDREGS    (1 << XFEATURE_BNDREGS)
#define XFEATURE_MASK_BNDCSR     (1 << XFEATURE_BNDCSR)
#define XFEATURE_MASK_OPMASK     (1 << XFEATURE_OPMASK)
#define XFEATURE_MASK_ZMM_Hi256  (1 << XFEATURE_ZMM_Hi256)
#define XFEATURE_MASK_Hi16_ZMM   (1 << XFEATURE_Hi16_ZMM)
#define XFEATURE_MASK_PT         (1 << XFEATURE_PT)
#define XFEATURE_MASK_PKRU       (1 << XFEATURE_PKRU)
#define XFEATURE_MASK_PASID      (1 << XFEATURE_PASID)
#define XFEATURE_MASK_CET_U      (1 << XFEATURE_CET_U)
#define XFEATURE_MASK_CET_S      (1 << XFEATURE_CET_S)
#define XFEATURE_MASK_HDC        (1 << XFEATURE_HDC)
#define XFEATURE_MASK_LBR        (1 << XFEATURE_LBR)
#define XFEATURE_MASK_HWP        (1 << XFEATURE_HWP)

#define XFEATURE_MASK_LEGACY     (XFEATURE_MASK_FP | XFEATURE_MASK_SSE)
#define XFEATURE_MASK_AVX512     (XFEATURE_MASK_OPMASK    | \
                                  XFEATURE_MASK_ZMM_Hi256 | \
                                  XFEATURE_MASK_Hi16_ZMM)
// Bit 63 is used for special functionality in some bitmaps and does not
// correspond to any state component.
#define XFEATURE_MASK_EXTENDED   (~(XFEATURE_MASK_LEGACY | (1ULL << 63)))

// XCR
#define HAX_SUPPORTED_XCR0       (XFEATURE_MASK_FP | XFEATURE_MASK_SSE | \
                                  XFEATURE_MASK_YMM)

#define XCR_XFEATURE_ENABLED_MASK  0x00000000

#endif  // HAX_CORE_FPU_H_
