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

#ifndef HAX_CORE_DEBUG_H_
#define HAX_CORE_DEBUG_H_

#if 0
enum log_level {
    error       = 0x00000001,
    warn        = 0x00000002,
    info        = 0x00000004,

    init        = 0x00000010,
    mem         = 0x00000020,
    mtrr        = 0x00000040,
    msr         = 0x00000080,
    pci         = 0x00000100,
    vmx         = 0x00000200,
    vmwrite     = 0x00000400,

    dispatch    = 0x00001000,
    trace_intr  = 0x00002000,

    acpi        = 0x00010000,
    apic        = 0x00020000,
    irq         = 0x00040000,

    map         = 0x00100000,
    vtlb        = 0x00200000,
    ept         = 0x00400000,
    vtd         = 0x00800000,

    vcpu        = 0x01000000,
    taskswitch  = 0x02000000,
    cpuid       = 0x04000000,
    cr_access   = 0x08000000,
    v86         = 0x10000000,
    emul        = 0x20000000,
    emul1       = 0x40000000,
    translate   = 0x80000000,

    vpic        = 0x100000000,      // bit 32
    vapic       = 0x200000000,
    vioapic     = 0x400000000,
    msi         = 0x800000000,

    perf        = 0x10000000000,    // bit 40
    plugin      = 0x20000000000,

    vvmx        = 0x100000000000,   // bit 44
    vvmx1       = 0x200000000000,
    vvmx2       = 0x400000000000
};
#endif

typedef enum log_level log_level;

#endif  // HAX_CORE_DEBUG_H_
