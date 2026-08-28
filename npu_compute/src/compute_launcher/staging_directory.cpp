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
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <unistd.h>

namespace npu_compute::compute_launcher {
namespace {

void SetError(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool StagingDirectory::Create(const std::filesystem::path& root, StagingDirectory* result, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (result == nullptr) {
        SetError("collection data directory result is null", error);
        return false;
    }
    result->path_.clear();

    if (root.empty() || !root.is_absolute()) {
        SetError("collection data directory root must be an absolute path", error);
        return false;
    }

    std::error_code filesystem_error;
    const std::filesystem::file_status root_status = std::filesystem::status(root, filesystem_error);
    if (filesystem_error) {
        SetError("inspect collection data directory root failed: " + filesystem_error.message(), error);
        return false;
    }
    if (!std::filesystem::is_directory(root_status)) {
        SetError("collection data directory root is not a directory: " + root.string(), error);
        return false;
    }

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    if (milliseconds.count() < 0) {
        SetError("collection data directory timestamp is before the Unix epoch", error);
        return false;
    }

    const std::string directory_name =
        "npu-compute-" + std::to_string(milliseconds.count()) + "-" + std::to_string(::getpid()) + "-XXXXXX";
    const std::string path_template = (root.lexically_normal() / directory_name).string();
    std::vector<char> writable_template(path_template.begin(), path_template.end());
    writable_template.push_back('\0');

    char* created = ::mkdtemp(writable_template.data());
    if (created == nullptr) {
        SetError("create collection data directory failed: " + std::string(std::strerror(errno)), error);
        return false;
    }

    result->path_ = created;
    return true;
}

const std::string& StagingDirectory::Path() const noexcept { return path_; }

} // namespace npu_compute::compute_launcher
