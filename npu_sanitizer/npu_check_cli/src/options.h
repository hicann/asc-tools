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

// 命令行解析结果。ToolId / OptionValue / ToolRequest 的唯一定义见 common/wire_protocol.h。
struct Options {
    std::vector<ipc::ToolRequest> tools; // 本次启用的工具及各自子选项，已按注册表规则规范化排序
    std::string logFile;                 // 用户指定的日志目录或文件；空表示写 stdout
    int handshakeTimeoutMs = 10000;      // [内部] 建连 + 握手 + Configure + Ready 的总超时
    int errorExitCode = 0;               // [内部] 0 表示不覆盖应用退出码
    bool showHelp = false;
    std::vector<std::string> application; // application[0] 为程序，其余为原样 argv
};

bool ParseOptions(int argc, char** argv, Options& options, std::string& error);
bool ResolveLibraryPath(const std::string& requested, std::string& resolved, std::string& error);
std::string Usage();

} // namespace npu::sanitizer::cli

#endif
