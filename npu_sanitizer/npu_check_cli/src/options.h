/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_CLI_OPTIONS_H
#define NPU_CHECK_CLI_OPTIONS_H

#include "wire_protocol.h"

#include <string>
#include <vector>

namespace npu::sanitizer::cli {

struct Options {
    ipc::ToolConfig toolConfig;
    std::string libraryPath;
    int handshakeTimeoutMs = 10000;
    bool showHelp = false;
    std::vector<std::string> application;
};

bool ParseOptions(int argc, char** argv, Options& options, std::string& error);
bool ResolveLibraryPath(const std::string& requested, std::string& resolved, std::string& error);
std::string Usage();

} // namespace npu::sanitizer::cli

#endif
