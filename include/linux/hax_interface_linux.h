/*
 * Copyright (c) 2011 Intel Corporation
 * Copyright (c) 2018 Kryptos Logic
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

#ifndef HAX_LINUX_HAX_INTERFACE_LINUX_H_
#define HAX_LINUX_HAX_INTERFACE_LINUX_H_

#include <asm/ioctl.h>

#define HAX_DEVICE_TYPE 'H'

#define HAX_IOCTL_HAX_IO(code, type) \
    _IO(HAX_DEVICE_TYPE, code)
#define HAX_IOCTL_HAX_IOR(code, type) \
    _IOR(HAX_DEVICE_TYPE, code, type)
#define HAX_IOCTL_HAX_IOW(code, type) \
    _IOW(HAX_DEVICE_TYPE, code, type)
#define HAX_IOCTL_HAX_IOWR(code, type) \
    _IOWR(HAX_DEVICE_TYPE, code, type)

#define HAX_LEGACY_IOCTL(access, code_posix, code_windows, type) \
    HAX_IOCTL_##access(code_posix, type)
#define HAX_IOCTL(access, code, type) \
    HAX_IOCTL_##access(code, type)

#define HAX_KERNEL64_CS 0x80
#define HAX_KERNEL32_CS 0x08

#define is_compatible() 0

#endif  // HAX_LINUX_HAX_INTERFACE_LINUX_H_
