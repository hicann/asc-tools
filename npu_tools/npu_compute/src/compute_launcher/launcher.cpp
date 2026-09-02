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
#include <cstdlib>
#include <cstring>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern char** environ;

namespace npu_compute::compute_launcher {
namespace {

constexpr char kCollectionActiveEnvironment[] = "NPU_COMPUTE_COLLECTION_ACTIVE";
constexpr char kCollectionOutputEnvironment[] = "NPU_COMPUTE_OUTPUT";
constexpr char kNestedCollectionMarker[] = ".npu-compute-nested-collection";
constexpr char kNestedCollectionError[] = "nested npu-compute collection is not supported";

class FileDescriptor {
public:
    explicit FileDescriptor(int descriptor) : descriptor_(descriptor) {}
    ~FileDescriptor()
    {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    int Get() const { return descriptor_; }

private:
    int descriptor_ = -1;
};

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool FailErrno(const std::string& message, int error_number, std::string* error)
{
    return Fail(message + ": " + std::string(std::strerror(error_number)), error);
}

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
    const CliConfig& config, const std::string& collection_data_directory, std::vector<std::string>* environment,
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
    SetEnvironmentValue("NPU_COMPUTE_OUTPUT", collection_data_directory, environment);
    SetEnvironmentValue("NPU_COMPUTE_CSV_OUTPUT_DIR", collection_data_directory, environment);
    SetEnvironmentValue(kCollectionActiveEnvironment, "1", environment);
    return true;
}

bool IsNestedCollection()
{
    const char* value = std::getenv(kCollectionActiveEnvironment);
    return value != nullptr && std::strcmp(value, "1") == 0;
}

bool RecordNestedCollection(std::string* error)
{
    const char* collection_directory = std::getenv(kCollectionOutputEnvironment);
    if (collection_directory == nullptr || collection_directory[0] == '\0') {
        return Fail("nested collection detection failed: collection data directory is not set", error);
    }

    const boost::filesystem::path marker_path = boost::filesystem::path(collection_directory) / kNestedCollectionMarker;
    FileDescriptor marker(
        ::open(marker_path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, S_IRUSR | S_IWUSR));
    if (marker.Get() < 0) {
        return FailErrno("nested collection detection failed", errno, error);
    }
    return true;
}

bool ConsumeNestedCollectionMarker(
    const boost::filesystem::path& collection_directory, bool* detected, std::string* error)
{
    *detected = false;
    const boost::filesystem::path marker_path = collection_directory / kNestedCollectionMarker;
    if (::unlink(marker_path.c_str()) != 0) {
        if (errno == ENOENT) {
            return true;
        }
        return FailErrno("nested collection detection failed", errno, error);
    }
    *detected = true;
    return true;
}

bool ValidateHardwareInfoResult(const std::string& collectionDataDirectory, std::string* error)
{
    const std::string path = collectionDataDirectory + "/HardwareInfo.jsonl";
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

int LaunchTarget(
    const CliConfig& config, std::string* collection_data_directory, std::string* report_path, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (collection_data_directory != nullptr) {
        collection_data_directory->clear();
    }
    if (report_path != nullptr) {
        report_path->clear();
    }

    if (IsNestedCollection()) {
        if (!RecordNestedCollection(error)) {
            return kInternalErrorExitCode;
        }
        if (error != nullptr) {
            *error = kNestedCollectionError;
        }
        return kCollectionErrorExitCode;
    }

    std::string stage_error;
    ReportTarget target;
    if (!ResolveReportTarget(config.export_path, &target, &stage_error)) {
        SetStageError("resolve report target failed", stage_error, error);
        return kReportErrorExitCode;
    }

    boost::system::error_code current_directory_error;
    const boost::filesystem::path current_directory = boost::filesystem::current_path(current_directory_error);
    if (current_directory_error) {
        SetStageError("get current directory failed", current_directory_error.message(), error);
        return kInternalErrorExitCode;
    }

    StagingDirectory collection_data;
    if (!StagingDirectory::Create(current_directory, &collection_data, error)) {
        return kInternalErrorExitCode;
    }
    const auto finishCollection = [&](int result) {
        std::string cleanup_error;
        if (!collection_data.RemoveIfEmpty(&cleanup_error)) {
            SetStageError("finalize collection data directory failed", cleanup_error, error);
            result = kInternalErrorExitCode;
        }
        if (collection_data_directory != nullptr) {
            *collection_data_directory = collection_data.Path();
        }
        return result;
    };

    ProcessLaunchRequest request;
    request.program = config.program;
    request.arguments = config.program_arguments;
    if (!BuildChildEnvironment(config, collection_data.Path(), &request.environment, error)) {
        return finishCollection(kInternalErrorExitCode);
    }
    const int appResult = LaunchProcessAndWait(request, error);
    bool nested_collection_detected = false;
    if (!ConsumeNestedCollectionMarker(collection_data.Path(), &nested_collection_detected, error)) {
        return finishCollection(kInternalErrorExitCode);
    }
    if (nested_collection_detected) {
        if (error != nullptr) {
            *error = kNestedCollectionError;
        }
        return finishCollection(kCollectionErrorExitCode);
    }
    if (appResult != 0) {
        return finishCollection(appResult);
    }
    if (!ValidateHardwareInfoResult(collection_data.Path(), error)) {
        return finishCollection(kCollectionErrorExitCode);
    }

    std::vector<uint8_t> encoded;
    if (!PackDirectoryToRep(collection_data.Path(), &encoded, &stage_error)) {
        SetStageError("pack collection results failed", stage_error, error);
        return finishCollection(kReportErrorExitCode);
    }

    if (!PublishRepReport(encoded, target, &stage_error)) {
        SetStageError("publish report failed", stage_error, error);
        return finishCollection(kReportErrorExitCode);
    }
    if (report_path != nullptr) {
        *report_path = target.path.string();
    }
    return finishCollection(0);
}

} // namespace npu_compute::compute_launcher
