/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "injection_path.h"

#include <unistd.h>

#include <array>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <system_error>
#include <vector>

namespace npu_compute::compute_launcher {
namespace {

constexpr const char* kInjectionLibraryName = "libnpu-compute.so";
constexpr std::size_t kInitialPathBufferSize = 256;
constexpr std::size_t kMaximumPathBufferSize = 1024 * 1024;

bool ReadExecutablePath(boost::filesystem::path* path, std::string* error)
{
    std::vector<char> buffer(kInitialPathBufferSize);
    while (buffer.size() <= kMaximumPathBufferSize) {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            if (error != nullptr) {
                *error = "read /proc/self/exe failed";
            }
            return false;
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            *path = std::string(buffer.data(), static_cast<std::size_t>(length));
            return true;
        }
        buffer.resize(buffer.size() * 2);
    }

    if (error != nullptr) {
        *error = "executable path is too long";
    }
    return false;
}

bool IsReadableRegularFile(const boost::filesystem::path& path)
{
    boost::system::error_code status_error;
    const bool is_regular = boost::filesystem::is_regular_file(path, status_error);
    return !status_error && is_regular && access(path.c_str(), R_OK) == 0;
}

} // namespace

bool ResolveInjectionLibraryPath(std::string* path, std::string* error)
{
    if (path == nullptr) {
        if (error != nullptr) {
            *error = "injection library output path is null";
        }
        return false;
    }

    boost::filesystem::path executable;
    if (!ReadExecutablePath(&executable, error)) {
        return false;
    }

    const boost::filesystem::path executable_directory = executable.parent_path();
    const std::array<boost::filesystem::path, 2> candidates = {
        executable_directory / kInjectionLibraryName,
        executable_directory / ".." / "tools" / "npu_tools" / "lib64" / kInjectionLibraryName,
    };

    for (const boost::filesystem::path& candidate : candidates) {
        boost::system::error_code canonical_error;
        const boost::filesystem::path canonical = boost::filesystem::canonical(candidate, canonical_error);
        if (!canonical_error && IsReadableRegularFile(canonical)) {
            *path = canonical.string();
            return true;
        }
    }

    if (error != nullptr) {
        *error = "libnpu-compute.so was not found next to npu-compute, or in ../tools/npu_tools/lib64";
    }
    return false;
}

} // namespace npu_compute::compute_launcher
