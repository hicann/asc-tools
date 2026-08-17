/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "staging_directory.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace npu_compute::compute_launcher {
namespace {

void SetError(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool StagingDirectory::Create(StagingDirectory* result, std::string* error)
{
    if (result == nullptr) {
        SetError("staging directory result is null", error);
        return false;
    }
    result->path_.clear();

    const char* temporary_root = std::getenv("TMPDIR");
    if (temporary_root == nullptr || temporary_root[0] == '\0') {
        temporary_root = "/tmp";
    }

    std::error_code filesystem_error;
    const std::filesystem::path absolute_root = std::filesystem::absolute(temporary_root, filesystem_error);
    if (filesystem_error) {
        SetError("resolve temporary directory failed: " + filesystem_error.message(), error);
        return false;
    }

    const std::string path_template = (absolute_root / "npu-compute-XXXXXX").lexically_normal().string();
    std::vector<char> writable_template(path_template.begin(), path_template.end());
    writable_template.push_back('\0');

    char* created = ::mkdtemp(writable_template.data());
    if (created == nullptr) {
        SetError("create staging directory failed: " + std::string(std::strerror(errno)), error);
        return false;
    }

    result->path_ = created;
    return true;
}

const std::string& StagingDirectory::Path() const noexcept { return path_; }

} // namespace npu_compute::compute_launcher
