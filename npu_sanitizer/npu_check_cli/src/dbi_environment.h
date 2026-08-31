/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_CLI_DBI_ENVIRONMENT_H
#define NPU_CHECK_CLI_DBI_ENVIRONMENT_H

#include <string>
#include <utility>
#include <vector>

namespace npu::sanitizer::cli {

using EnvironmentEntries = std::vector<std::pair<std::string, std::string>>;

// DBI 运行期需要的部署参数。Configure 改用注册表编码后只承载工具与子选项，路径这类
// 与协议无关的信息改由环境变量传给目标进程，因此这里不再复用协议结构体。
struct DbiSettings {
    std::string workDir;
    std::string probeCacheDir;
    bool strict = true;
    bool keepTemp = false;
};

EnvironmentEntries BuildDbiEnvironment(const DbiSettings& settings);
bool ApplyEnvironment(const EnvironmentEntries& entries, std::string& error);

} // namespace npu::sanitizer::cli

#endif
