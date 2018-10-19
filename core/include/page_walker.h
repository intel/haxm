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

#ifndef HAX_CORE_PAGE_WALKER_H_
#define HAX_CORE_PAGE_WALKER_H_

typedef uint64_t ADDRESS;

#define ALIGN_BACKWARD(__address, __bytes)  \
        ((ADDRESS)(__address) & ~((__bytes) - 1))

#define IN
#define OUT

#define PAGE_4KB_MASK       (PAGE_SIZE_4K - 1)
#define PAGE_2MB_MASK       (PAGE_SIZE_2M - 1)
#define PAGE_4MB_MASK       (PAGE_SIZE_4M - 1)
#define PAGE_1GB_MASK       (PAGE_SIZE_1G - 1)

#define PW_INVALID_GPA (~((uint64_t)0))
#define PW_NUM_OF_PDPT_ENTRIES_IN_32_BIT_MODE 4

/*
 * Function: pw_perform_page_walk
 * Description: The function performs page walk over guest page tables for
 *              specific virtual address
 * Input:
 *       vcpu        - gcpu handle
 *       virt_addr   - Virtual address to perform page walk for
 *       access      - Access descriptor (read/write, user/supervisor)
 *       set_ad_bits - If TRUE, A/D bits will be set in guest table
 *       is_fetch    - Indicates whether it is a fetch access
 * Output:
 *       gpa_out - Final guest physical address
 *       order   - PG_ORDER_4K, PG_ORDER_2M, PG_ORDER_4M, PG_ORDER_1G
 */

uint32_t pw_perform_page_walk(IN struct vcpu_t *vcpu, IN uint64_t virt_addr,
                              IN uint32_t access, OUT uint64_t *gpa_out,
                              OUT uint *order, IN bool set_ad_bits,
                              IN bool is_fetch);

#endif  // HAX_CORE_PAGE_WALKER_H_
