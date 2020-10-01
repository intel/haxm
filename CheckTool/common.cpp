/*
 * Copyright (c) 2020 Intel Corporation
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

#include <iostream>

#include "arg_parser.h"
#include "common.h"
#include "feature_detector.h"

namespace haxm {
namespace check_util {

CheckResult ParseArguments(int &argc, char* argv[], bool &is_verbose) {
    haxm::check_util::ArgParser arg_parser(argc, argv, {"-h", "--help", "-v",
                                           "--verbose"});

    if (!arg_parser.Verify()) {
        std::cout << "checktool unknown option: " << arg_parser.error()
                  << std::endl;
        std::cout << "Usage: checktool [-h | --help] [-v | --verbose]"
                  << std::endl;
        return haxm::check_util::kError;
    }

    if (arg_parser.Test("-h") || arg_parser.Test("--help")) {
        std::cout << "CheckTool version " << APP_VERSION << std::endl;
        std::cout << "-v, --verbose Show detailed system information"
                  << std::endl;
        return haxm::check_util::kFail;
    }

    if (arg_parser.Test("-v") || arg_parser.Test("--verbose")) {
        is_verbose = true;
    }

    return haxm::check_util::kPass;
}

int Check(bool is_verbose) {
    int ret = 0;

    haxm::check_util::FeatureDetector fd;
    haxm::check_util::CheckResult detect_res = fd.Detect();

    if (detect_res == haxm::check_util::kError) {
        ret = -1;
    } else if (detect_res == haxm::check_util::kFail) {
        ret = 1;
    }

    if (is_verbose) {
        fd.Print();
    }

    return ret;
}

}  // namespace check_util
}  // namespace haxm
