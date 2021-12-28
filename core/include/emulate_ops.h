/*
 * Copyright (c) 2018 Alexandro Sanchez Bach <alexandro@phi.nz>
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

#ifndef HAX_CORE_EMULATE_OPS_H_
#define HAX_CORE_EMULATE_OPS_H_

#include "types.h"

#define FASTOP_ALIGN  0x10
#define FASTOP_OFFSET(size) ( \
    ((size) == 8) ? (3 * FASTOP_ALIGN) : \
    ((size) == 4) ? (2 * FASTOP_ALIGN) : \
    ((size) == 2) ? (1 * FASTOP_ALIGN) : \
                    (0 * FASTOP_ALIGN))

/* Instruction handlers */
typedef void(ASMCALL em_handler_t)(void);
em_handler_t em_not;
em_handler_t em_neg;
em_handler_t em_inc;
em_handler_t em_dec;
em_handler_t em_add;
em_handler_t em_or;
em_handler_t em_adc;
em_handler_t em_sbb;
em_handler_t em_and;
em_handler_t em_sub;
em_handler_t em_xor;
em_handler_t em_test;
em_handler_t em_xadd;
em_handler_t em_cmp;
em_handler_t em_cmp_r;
em_handler_t em_bsf;
em_handler_t em_bsr;
em_handler_t em_bt;
em_handler_t em_bts;
em_handler_t em_btr;
em_handler_t em_btc;
em_handler_t em_rol;
em_handler_t em_ror;
em_handler_t em_rcl;
em_handler_t em_rcr;
em_handler_t em_shl;
em_handler_t em_shr;
em_handler_t em_sar;
em_handler_t em_bextr;
em_handler_t em_andn;

/* Dispatch handlers */
void ASMCALL fastop_dispatch(void *handler, uint64_t *dst,
                             uint64_t *src1, uint64_t *src2, uint64_t *flags);

#endif /* HAX_CORE_EMULATE_OPS_H_ */
