/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_CLI_PROCESS_RUNNER_H
#define NPU_CHECK_CLI_PROCESS_RUNNER_H

#include "options.h"

#include <string>

namespace npu::sanitizer::cli {

// 三类结果。退出码空间无法完全消歧 —— 应用自身完全可能返回 64、125 或 127 —— 因此
// 脚本必须靠结果摘要行来区分，而不是靠退出码。
enum class Outcome {
    kForwarded,   // 完整 Result 已转发，应用自身正常结束
    kAppFailed,   // 完整 Result 已转发，但应用自身失败
    kInfraFailed, // 握手 / 协议 / Result 不完整，本次检查没有可信结论
};

struct ResultSummary {
    Outcome outcome = Outcome::kInfraFailed;
    int hasErrors = -1;             // <0 表示 unknown（未收到完整 Result）
    bool truncated = false;         // 报告因触及总长上限被截断
    std::string childExit = "none"; // 退出码、"signal:N"，或没有子进程时的 "none"
    int exit = 125;
};

// 结果摘要行。任何路径下都必须输出到 stderr，格式属于对外兼容契约。
std::string FormatResultSummary(const ResultSummary& summary);

int RunApplication(const Options& options, const std::string& libraryPath);

} // namespace npu::sanitizer::cli

#endif
