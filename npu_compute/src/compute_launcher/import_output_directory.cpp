/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "import_output_directory.h"

#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>

namespace npu_compute::compute_launcher {
namespace {

constexpr char kNpuRepSuffix[] = ".npu-rep";
constexpr char kNestedNpuRepSuffix[] = ".npu.rep";
constexpr char kRepSuffix[] = ".rep";

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool FailErrno(const std::string& message, std::string* error)
{
    return Fail(message + ": " + std::error_code(errno, std::generic_category()).message(), error);
}

bool ReadStatus(const std::filesystem::path& path, std::filesystem::file_status* status, std::string* error)
{
    std::error_code statusError;
    *status = std::filesystem::symlink_status(path, statusError);
    if (statusError == std::errc::no_such_file_or_directory) {
        *status = std::filesystem::file_status(std::filesystem::file_type::not_found);
        return true;
    }
    if (statusError) {
        return Fail("inspect import output path failed: " + path.string() + ": " + statusError.message(), error);
    }
    return true;
}

std::filesystem::path ResolvePath(const std::filesystem::path& path, const std::filesystem::path& currentDirectory)
{
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    return (currentDirectory / path).lexically_normal();
}

bool RemoveSuffix(std::string* name)
{
    const char* const suffixes[] = {kNpuRepSuffix, kNestedNpuRepSuffix, kRepSuffix};
    for (const char* suffix : suffixes) {
        const std::size_t suffixLength = std::strlen(suffix);
        if (name->size() >= suffixLength && name->compare(name->size() - suffixLength, suffixLength, suffix) == 0) {
            name->resize(name->size() - suffixLength);
            return !name->empty();
        }
    }
    return false;
}

bool ResolveFinalPath(
    const std::filesystem::path& inputRep, const std::optional<std::string>& exportPath,
    std::filesystem::path* finalPath, std::string* error)
{
    std::error_code currentPathError;
    const std::filesystem::path currentDirectory = std::filesystem::current_path(currentPathError);
    if (currentPathError) {
        return Fail("get current directory failed: " + currentPathError.message(), error);
    }

    if (exportPath.has_value()) {
        if (exportPath->empty()) {
            return Fail("import export path is empty", error);
        }
        *finalPath = ResolvePath(*exportPath, currentDirectory);
    } else {
        std::string name = inputRep.filename().string();
        if (!RemoveSuffix(&name)) {
            return Fail("import input name must end with .npu-rep, .npu.rep, or .rep", error);
        }
        *finalPath = currentDirectory / name;
    }

    if (finalPath->filename().empty() || finalPath->filename() == "." || finalPath->filename() == "..") {
        return Fail("import output directory name is invalid: " + finalPath->string(), error);
    }

    std::filesystem::file_status parentStatus;
    if (!ReadStatus(finalPath->parent_path(), &parentStatus, error)) {
        return false;
    }
    if (!std::filesystem::is_directory(parentStatus)) {
        return Fail("import output parent is not a directory: " + finalPath->parent_path().string(), error);
    }

    std::filesystem::file_status finalStatus;
    if (!ReadStatus(*finalPath, &finalStatus, error)) {
        return false;
    }
    if (std::filesystem::exists(finalStatus)) {
        return Fail("import output directory already exists: " + finalPath->string(), error);
    }
    return true;
}

bool CreateTemporaryDirectory(
    const std::filesystem::path& finalPath, std::filesystem::path* temporaryPath, std::string* error)
{
    std::string pathTemplate =
        (finalPath.parent_path() / (".npu-compute-import-" + finalPath.filename().string() + ".tmp.XXXXXX")).string();
    pathTemplate.push_back('\0');
    char* created = ::mkdtemp(pathTemplate.data());
    if (created == nullptr) {
        return FailErrno("create import temporary directory failed", error);
    }
    *temporaryPath = created;
    return true;
}

bool SyncDirectory(const std::filesystem::path& directory, std::string* error)
{
    const int descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return FailErrno("open import output parent for sync failed", error);
    }
    while (::fsync(descriptor) != 0) {
        if (errno == EINTR) {
            continue;
        }
        const int syncError = errno;
        ::close(descriptor);
        errno = syncError;
        return FailErrno("sync import output parent failed", error);
    }
    if (::close(descriptor) != 0) {
        return FailErrno("close import output parent failed", error);
    }
    return true;
}

} // namespace

ImportOutputDirectory::~ImportOutputDirectory() { CleanupTemporaryDirectory(); }

bool ImportOutputDirectory::Create(
    const std::filesystem::path& inputRep, const std::optional<std::string>& exportPath,
    ImportOutputDirectory* directory, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (directory == nullptr) {
        return Fail("import output directory is null", error);
    }
    directory->CleanupTemporaryDirectory();
    directory->finalPath_.clear();

    if (!ResolveFinalPath(inputRep, exportPath, &directory->finalPath_, error)) {
        directory->finalPath_.clear();
        return false;
    }
    if (!CreateTemporaryDirectory(directory->finalPath_, &directory->temporaryPath_, error)) {
        directory->finalPath_.clear();
        return false;
    }
    return true;
}

const std::filesystem::path& ImportOutputDirectory::TemporaryPath() const { return temporaryPath_; }

const std::filesystem::path& ImportOutputDirectory::FinalPath() const { return finalPath_; }

bool ImportOutputDirectory::Publish(std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (temporaryPath_.empty() || finalPath_.empty()) {
        return Fail("import output directory is not initialized", error);
    }

    const long result =
        ::syscall(SYS_renameat2, AT_FDCWD, temporaryPath_.c_str(), AT_FDCWD, finalPath_.c_str(), RENAME_NOREPLACE);
    if (result != 0) {
        return FailErrno("publish import output directory failed", error);
    }
    temporaryPath_.clear();
    if (!SyncDirectory(finalPath_.parent_path(), error)) {
        std::error_code removeError;
        std::filesystem::remove_all(finalPath_, removeError);
        return false;
    }
    return true;
}

void ImportOutputDirectory::CleanupTemporaryDirectory() noexcept
{
    if (!temporaryPath_.empty()) {
        std::error_code error;
        std::filesystem::remove_all(temporaryPath_, error);
        temporaryPath_.clear();
    }
}

} // namespace npu_compute::compute_launcher
