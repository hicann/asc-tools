/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "launcher.h"

#include "injection_path.h"
#include "process_launcher.h"
#include "rep_directory_packer.h"
#include "rep_report_writer.h"
#include "report_name.h"
#include "staging_directory.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>

extern char** environ;

namespace npu_compute::compute_launcher {
namespace {

std::string Join(const std::vector<std::string>& items)
{
    std::string result;
    for (const std::string& item : items) {
        if (!result.empty()) {
            result += ",";
        }
        result += item;
    }
    return result;
}

void SetEnvironmentValue(const std::string& name, const std::string& value, std::vector<std::string>* environment)
{
    const std::string prefix = name + "=";
    for (auto iterator = environment->begin(); iterator != environment->end();) {
        if (iterator->compare(0, prefix.size(), prefix) == 0) {
            iterator = environment->erase(iterator);
        } else {
            ++iterator;
        }
    }
    environment->push_back(prefix + value);
}

bool BuildChildEnvironment(
    const CliConfig& config, const std::string& staging_directory, std::vector<std::string>* environment,
    std::string* error)
{
    std::string injection_path;
    if (!ResolveInjectionLibraryPath(&injection_path, error)) {
        return false;
    }

    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        environment->emplace_back(*entry);
    }
    const std::string sections = Join(config.sections);
    SetEnvironmentValue("ACL_API_INJECTION", injection_path, environment);
    SetEnvironmentValue("NPU_COMPUTE_SECTIONS", sections, environment);
    SetEnvironmentValue("NPU_COMPUTE_REPLAY_MODE", ReplayModeName(config.replay_mode), environment);
    SetEnvironmentValue("NPU_COMPUTE_OUTPUT", staging_directory, environment);
    SetEnvironmentValue("NPU_COMPUTE_CSV_OUTPUT_DIR", staging_directory, environment);
    return true;
}

bool ValidateHardwareInfoResult(const std::string& stagingDirectory, std::string* error)
{
    const std::string path = stagingDirectory + "/HardwareInfo.jsonl";
    struct stat status {};
    if (::lstat(path.c_str(), &status) != 0) {
        if (errno == ENOENT) {
            if (error != nullptr) {
                *error = "HardwareInfo.jsonl is missing";
            }
            return false;
        }
        if (error != nullptr) {
            *error = "inspect HardwareInfo.jsonl failed: " + std::string(std::strerror(errno));
        }
        return false;
    }
    if (!S_ISREG(status.st_mode)) {
        if (error != nullptr) {
            *error = "HardwareInfo.jsonl is not a regular file";
        }
        return false;
    }
    return true;
}

void SetStageError(const std::string& stage, const std::string& detail, std::string* error)
{
    if (error != nullptr) {
        *error = stage + ": " + detail;
    }
}

} // namespace

int LaunchTarget(const CliConfig& config, std::string* staging_directory, std::string* report_path, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (staging_directory != nullptr) {
        staging_directory->clear();
    }
    if (report_path != nullptr) {
        report_path->clear();
    }

    StagingDirectory staging;
    if (!StagingDirectory::Create(&staging, error)) {
        return 5;
    }
    if (staging_directory != nullptr) {
        *staging_directory = staging.Path();
    }

    ProcessLaunchRequest request;
    request.program = config.program;
    request.arguments = config.program_arguments;
    if (!BuildChildEnvironment(config, staging.Path(), &request.environment, error)) {
        return 5;
    }
    const int appResult = LaunchProcessAndWait(request, error);
    if (appResult != 0) {
        return appResult;
    }
    if (!ValidateHardwareInfoResult(staging.Path(), error)) {
        return kCollectionErrorExitCode;
    }

    std::vector<uint8_t> encoded;
    std::string stage_error;
    if (!PackDirectoryToRep(staging.Path(), &encoded, &stage_error)) {
        SetStageError("pack collection results failed", stage_error, error);
        return kReportErrorExitCode;
    }

    ReportTarget target;
    if (!ResolveReportTarget(config.export_path, &target, &stage_error)) {
        SetStageError("resolve report target failed", stage_error, error);
        return kReportErrorExitCode;
    }
    if (!PublishRepReport(encoded, target, &stage_error)) {
        SetStageError("publish report failed", stage_error, error);
        return kReportErrorExitCode;
    }
    if (report_path != nullptr) {
        *report_path = target.path.string();
    }
    return 0;
}

} // namespace npu_compute::compute_launcher
