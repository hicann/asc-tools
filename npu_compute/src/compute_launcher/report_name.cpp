/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "report_name.h"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <string>
#include <system_error>

#include <sys/random.h>

namespace npu_compute::compute_launcher {
namespace {

constexpr char kReportSuffix[] = ".npu-rep";
constexpr std::size_t kMaximumNameAttempts = 128U;

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool SystemEpochMilliseconds(std::uint64_t* value, void*, std::string* error)
{
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    if (milliseconds.count() < 0) {
        return Fail("system time is before the Unix epoch", error);
    }
    *value = static_cast<std::uint64_t>(milliseconds.count());
    return true;
}

bool SystemRandomBytes(std::array<std::uint8_t, 4>* value, void*, std::string* error)
{
    std::size_t offset = 0;
    while (offset < value->size()) {
        const ssize_t result = ::getrandom(value->data() + offset, value->size() - offset, 0);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return Fail("getrandom failed: " + std::error_code(errno, std::generic_category()).message(), error);
        }
        if (result == 0) {
            return Fail("getrandom returned zero bytes", error);
        }
        offset += static_cast<std::size_t>(result);
    }
    return true;
}

bool HasReportSuffix(const std::filesystem::path& path)
{
    const std::string value = path.filename().string();
    return value.size() >= sizeof(kReportSuffix) - 1U &&
           value.compare(value.size() - (sizeof(kReportSuffix) - 1U), sizeof(kReportSuffix) - 1U, kReportSuffix) == 0;
}

std::filesystem::path ResolvePath(const std::filesystem::path& path, const std::filesystem::path& current_directory)
{
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    return (current_directory / path).lexically_normal();
}

bool ReadStatus(const std::filesystem::path& path, std::filesystem::file_status* status, std::string* error)
{
    std::error_code status_error;
    *status = std::filesystem::symlink_status(path, status_error);
    if (status_error == std::errc::no_such_file_or_directory) {
        *status = std::filesystem::file_status(std::filesystem::file_type::not_found);
        return true;
    }
    if (status_error) {
        return Fail("inspect report path failed: " + path.string() + ": " + status_error.message(), error);
    }
    return true;
}

std::string FormatRandomId(const std::array<std::uint8_t, 4>& bytes)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(8U, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index * 2U] = kHex[bytes[index] >> 4U];
        result[index * 2U + 1U] = kHex[bytes[index] & 0x0fU];
    }
    return result;
}

bool GenerateTargetInDirectory(
    const std::filesystem::path& directory, const ReportNameSources& sources, ReportTarget* target, std::string* error)
{
    std::filesystem::file_status directory_status;
    if (!ReadStatus(directory, &directory_status, error)) {
        return false;
    }
    if (!std::filesystem::is_directory(directory_status)) {
        return Fail("report output path is not a directory: " + directory.string(), error);
    }

    std::uint64_t epoch_milliseconds = 0;
    if (!sources.epoch_milliseconds(&epoch_milliseconds, sources.context, error)) {
        return false;
    }
    for (std::size_t attempt = 0; attempt < kMaximumNameAttempts; ++attempt) {
        std::array<std::uint8_t, 4> random_bytes{};
        if (!sources.random_bytes(&random_bytes, sources.context, error)) {
            return false;
        }
        const std::filesystem::path candidate = directory / ("report_" + std::to_string(epoch_milliseconds) + "_" +
                                                             FormatRandomId(random_bytes) + kReportSuffix);
        std::filesystem::file_status candidate_status;
        if (!ReadStatus(candidate, &candidate_status, error)) {
            return false;
        }
        if (!std::filesystem::exists(candidate_status)) {
            target->path = candidate;
            return true;
        }
    }
    return Fail("unable to generate a unique report name after 128 attempts", error);
}

bool ResolveExplicitTarget(const std::filesystem::path& path, ReportTarget* target, std::string* error)
{
    const std::filesystem::path parent = path.parent_path();
    std::filesystem::file_status parent_status;
    if (!ReadStatus(parent, &parent_status, error)) {
        return false;
    }
    if (!std::filesystem::is_directory(parent_status)) {
        return Fail("report output parent is not a directory: " + parent.string(), error);
    }

    std::filesystem::file_status target_status;
    if (!ReadStatus(path, &target_status, error)) {
        return false;
    }
    if (std::filesystem::exists(target_status)) {
        if (!std::filesystem::is_regular_file(target_status)) {
            return Fail("report target exists but is not a regular file: " + path.string(), error);
        }
        return Fail("report target already exists: " + path.string(), error);
    }

    target->path = path;
    return true;
}

} // namespace

bool ResolveReportTarget(const std::optional<std::string>& export_path, ReportTarget* target, std::string* error)
{
    std::error_code current_path_error;
    const std::filesystem::path current_directory = std::filesystem::current_path(current_path_error);
    if (current_path_error) {
        if (target != nullptr) {
            *target = {};
        }
        return Fail("get current directory failed: " + current_path_error.message(), error);
    }
    ReportNameSources sources;
    sources.current_directory = current_directory;
    sources.epoch_milliseconds = &SystemEpochMilliseconds;
    sources.random_bytes = &SystemRandomBytes;
    return ResolveReportTargetWithSources(export_path, sources, target, error);
}

bool ResolveReportTargetWithSources(
    const std::optional<std::string>& export_path, const ReportNameSources& sources, ReportTarget* target,
    std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (target == nullptr) {
        return Fail("report target output is null", error);
    }
    *target = {};
    if (sources.current_directory.empty() || !sources.current_directory.is_absolute()) {
        return Fail("report current directory must be an absolute path", error);
    }
    if (sources.epoch_milliseconds == nullptr || sources.random_bytes == nullptr) {
        return Fail("report name sources are incomplete", error);
    }

    if (!export_path.has_value()) {
        return GenerateTargetInDirectory(sources.current_directory, sources, target, error);
    }
    if (export_path->empty()) {
        return Fail("report export path is empty", error);
    }

    const std::filesystem::path resolved = ResolvePath(*export_path, sources.current_directory);
    std::filesystem::file_status resolved_status;
    if (!ReadStatus(resolved, &resolved_status, error)) {
        return false;
    }
    if (std::filesystem::is_directory(resolved_status)) {
        return GenerateTargetInDirectory(resolved, sources, target, error);
    }
    if (!HasReportSuffix(resolved)) {
        return Fail(
            "report export path must be an existing directory or end with "
            ".npu-rep: " +
                resolved.string(),
            error);
    }
    return ResolveExplicitTarget(resolved, target, error);
}

} // namespace npu_compute::compute_launcher
