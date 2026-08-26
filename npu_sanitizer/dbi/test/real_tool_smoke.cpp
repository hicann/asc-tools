// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi_pipeline.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 9) {
        std::cerr << "usage: real_tool_smoke <input> <output> <arch> <arg-size> <toolchain-root> "
                     "<source-root> <work-dir> <cache-dir>\n";
        return 2;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long parsedArgSize = std::strtoul(argv[4], &end, 10);
    if (errno != 0 || end == argv[4] || *end != '\0' || parsedArgSize == 0 || parsedArgSize > UINT32_MAX) {
        std::cerr << "invalid argument size: " << argv[4] << '\n';
        return 2;
    }

    aclsan::DbiRequest request{};
    request.inputKernel = argv[1];
    request.outputKernel = argv[2];
    request.arch = argv[3];
    request.argSize = static_cast<uint32_t>(parsedArgSize);
    request.probeGroups = {aclsan::ProbeGroup::Mte2};
    request.toolchainRoot = argv[5];
    request.sourceRoot = argv[6];
    request.workDirectory = argv[7];
    request.cacheDirectory = argv[8];
    request.keepTemp = true;

    const auto result = aclsan::RunDbiPipeline(request);
    if (!result.success) {
        std::cerr << result.stage << ": " << result.diagnostic << '\n';
        return 1;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(result.patchedPath, error) ||
        std::filesystem::file_size(result.patchedPath, error) == 0) {
        std::cerr << "pipeline returned an empty patched output: " << result.patchedPath << '\n';
        return 1;
    }
    std::cout << result.patchedPath << '\n';
    return 0;
}
