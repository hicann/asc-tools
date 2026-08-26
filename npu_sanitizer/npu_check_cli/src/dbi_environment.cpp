/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dbi_environment.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace npu::sanitizer::cli {

EnvironmentEntries BuildDbiEnvironment(const ipc::ToolConfig& config)
{
    EnvironmentEntries entries{
        {"NPU_CHECK_DBI_WORK_DIR", config.workDir},
        {"NPU_CHECK_DBI_CACHE_DIR", config.probeCacheDir},
        {"NPU_CHECK_DBI_STRICT", config.strict ? "1" : "0"},
        {"NPU_CHECK_DBI_KEEP_TEMP", config.keepTemp ? "1" : "0"},
        {"NPU_CHECK_DBI_COMPILER_ARG_COUNT", std::to_string(config.compileOptions.size())},
    };
    entries.reserve(entries.size() + config.compileOptions.size());
    for (size_t index = 0; index < config.compileOptions.size(); ++index) {
        entries.emplace_back("NPU_CHECK_DBI_COMPILER_ARG_" + std::to_string(index), config.compileOptions[index]);
    }
    return entries;
}

bool ApplyEnvironment(const EnvironmentEntries& entries, std::string& error)
{
    for (const auto& entry : entries) {
        if (setenv(entry.first.c_str(), entry.second.c_str(), 1) != 0) {
            const int setenvError = errno;
            error = "setenv '" + entry.first + "': " + std::strerror(setenvError);
            return false;
        }
    }
    return true;
}

} // namespace npu::sanitizer::cli
