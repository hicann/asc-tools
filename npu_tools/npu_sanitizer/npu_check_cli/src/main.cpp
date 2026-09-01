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
    // 结果摘要行在任何路径下都必须输出，包括还没开始跑检查的这些早期失败：
    // 脚本读到的是同一行格式，不必为不同失败阶段各写一套解析。
    const auto reportEarlyFailure = [](int exitCode) {
        npu::sanitizer::cli::ResultSummary summary;
        summary.outcome = npu::sanitizer::cli::Outcome::kInfraFailed;
        summary.exit = exitCode;
        std::cerr << npu::sanitizer::cli::FormatResultSummary(summary) << '\n';
        return exitCode;
    };

    if (!npu::sanitizer::cli::ParseOptions(argc, argv, options, error)) {
        std::cerr << "npu_check: " << error << "\n\n" << npu::sanitizer::cli::Usage();
        return reportEarlyFailure(64);
    }
    if (options.showHelp) {
        std::cout << npu::sanitizer::cli::Usage();
        return 0;
    }
    // 注入库定位没有命令行入口，也没有环境变量覆盖：候选顺序固定为
    // ASCEND_TOOLKIT_HOME 下的 lib64/lib，其次是 npu_check 自身所在目录及其同级 lib{,64}。
    //
    // 这里退 125 而不是 64：64 是用法错误，而定位失败属于 fork 前的准备错误 ——
    // 用户的命令行没有任何问题，是环境或安装不完整。两者必须能被脚本区分开。
    std::string libraryPath;
    if (!npu::sanitizer::cli::ResolveLibraryPath(std::string{}, libraryPath, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return reportEarlyFailure(125);
    }
    return npu::sanitizer::cli::RunApplication(options, libraryPath);
}
