/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "options.h"
#include "process_runner.h"

#include <iostream>

int main(int argc, char** argv)
{
    npu::sanitizer::cli::Options options{};
    std::string error;
    if (!npu::sanitizer::cli::ParseOptions(argc, argv, options, error)) {
        std::cerr << "npu_check: " << error << "\n\n" << npu::sanitizer::cli::Usage();
        return 64;
    }
    if (options.showHelp) {
        std::cout << npu::sanitizer::cli::Usage();
        return 0;
    }
    std::string libraryPath;
    if (!npu::sanitizer::cli::ResolveLibraryPath(options.libraryPath, libraryPath, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return 64;
    }
    return npu::sanitizer::cli::RunApplication(options, libraryPath);
}
