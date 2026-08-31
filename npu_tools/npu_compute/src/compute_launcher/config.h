/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_CONFIG_H_
#define NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_CONFIG_H_

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace npu_compute::compute_launcher {

enum class ReplayMode {
    Kernel,
};

struct CliConfig {
    bool show_help = false;
    bool list_sections = false;
    std::vector<std::string> sections;
    ReplayMode replay_mode = ReplayMode::Kernel;
    bool replay_mode_specified = false;
    std::optional<std::string> import_path;
    std::optional<std::string> export_path;
    std::string program;
    std::vector<std::string> program_arguments;
};

bool ParseCli(int argc, char** argv, CliConfig* config, std::string* error);

const char* ReplayModeName(ReplayMode mode);

void PrintUsage(FILE* stream, const char* program);

void PrintSections(FILE* stream);

} // namespace npu_compute::compute_launcher

#endif // NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_CONFIG_H_
