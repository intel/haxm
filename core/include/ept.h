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

#ifndef HAX_CORE_EPT_H_
#define HAX_CORE_EPT_H_

#include "../../include/hax_types.h"
#include "vm.h"

#define ept_cap_rwX             ((uint64_t)1 << 0)
#define ept_cap_rWx             ((uint64_t)1 << 1)
#define ept_cap_rWX             ((uint64_t)1 << 2)
#define ept_cap_gaw21           ((uint64_t)1 << 3)
#define ept_cap_gaw30           ((uint64_t)1 << 4)
#define ept_cap_gaw39           ((uint64_t)1 << 5)
#define ept_cap_gaw48           ((uint64_t)1 << 6)
#define ept_cap_gaw57           ((uint64_t)1 << 7)

#define ept_cap_UC              ((uint64_t)1 << 8)
#define ept_cap_WC              ((uint64_t)1 << 9)
#define ept_cap_WT              ((uint64_t)1 << 12)
#define ept_cap_WP              ((uint64_t)1 << 13)
#define ept_cap_WB              ((uint64_t)1 << 14)

#define ept_cap_sp2M            ((uint64_t)1 << 16)
#define ept_cap_sp1G            ((uint64_t)1 << 17)
#define ept_cap_sp512G          ((uint64_t)1 << 18)
#define ept_cap_sp256T          ((uint64_t)1 << 19)

#define ept_cap_invept          ((uint64_t)1 << 20)
#define ept_cap_invept_ia       ((uint64_t)1 << 24)
#define ept_cap_invept_cw       ((uint64_t)1 << 25)
#define ept_cap_invept_ac       ((uint64_t)1 << 26)

#define ept_cap_invvpid         ((uint64_t)1 << 32)
#define ept_cap_invvpid_ia      ((uint64_t)1 << 40)
#define ept_cap_invvpid_cw      ((uint64_t)1 << 41)
#define ept_cap_invvpid_ac      ((uint64_t)1 << 42)
#define ept_cap_invvpid_cwpg    ((uint64_t)1 << 43)

#define INVALID_EPTP            ((uint64_t)~0ULL)

#define EPT_UNSUPPORTED_FEATURES \
        (ept_cap_sp2M | ept_cap_sp1G | ept_cap_sp512G | ept_cap_sp256T)

#define EPT_INVEPT_SINGLE_CONTEXT 1
#define EPT_INVEPT_ALL_CONTEXT    2

void invept(hax_vm_t *hax_vm, uint type);
bool ept_set_caps(uint64_t caps);

#endif  // HAX_CORE_EPT_H_
