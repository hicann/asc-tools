/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_writer.h"

#include <boost/filesystem.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace npu_compute {
namespace {

constexpr char kFinalFileName[] = "HardwareInfo.jsonl";
constexpr char kLockFileName[] = ".hardware_info.lock";
constexpr char kTemporaryPrefix[] = "HardwareInfo.jsonl.tmp.";

class FileDescriptor {
public:
    explicit FileDescriptor(int value = -1) noexcept : value_(value) {}

    ~FileDescriptor()
    {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    int Get() const noexcept { return value_; }

    bool Close(std::string* error)
    {
        if (value_ < 0) {
            return true;
        }
        const int descriptor = value_;
        value_ = -1;
        if (::close(descriptor) != 0) {
            SetErrno(error, "close HardwareInfo temporary file failed", errno);
            return false;
        }
        return true;
    }

private:
    static void SetErrno(std::string* error, const std::string& message, int errorNumber)
    {
        if (error != nullptr) {
            *error = message + ": " + std::error_code(errorNumber, std::generic_category()).message();
        }
    }

    int value_;
};

void SetError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

void SetErrno(std::string* error, const std::string& message, int errorNumber)
{
    SetError(error, message + ": " + std::error_code(errorNumber, std::generic_category()).message());
}

bool LockExclusive(int descriptor, std::string* error)
{
    while (::flock(descriptor, LOCK_EX) != 0) {
        if (errno == EINTR) {
            continue;
        }
        SetErrno(error, "lock HardwareInfo output failed", errno);
        return false;
    }
    return true;
}

bool WriteAll(int descriptor, std::string_view content, std::string* error)
{
    std::size_t offset = 0;
    while (offset < content.size()) {
        const std::size_t remaining = content.size() - offset;
        const std::size_t request = std::min(remaining, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
        const ssize_t written = ::write(descriptor, content.data() + offset, request);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            SetErrno(error, "write HardwareInfo temporary file failed", errno);
            return false;
        }
        if (written == 0) {
            SetError(error, "write HardwareInfo temporary file returned zero");
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

bool SyncFile(int descriptor, std::string* error)
{
    while (::fsync(descriptor) != 0) {
        if (errno == EINTR) {
            continue;
        }
        SetErrno(error, "sync HardwareInfo temporary file failed", errno);
        return false;
    }
    return true;
}

class TemporaryFileCleanup {
public:
    explicit TemporaryFileCleanup(boost::filesystem::path path) : path_(std::move(path)) {}

    ~TemporaryFileCleanup()
    {
        if (active_) {
            ::unlink(path_.c_str());
        }
    }

    void Release() noexcept { active_ = false; }

private:
    boost::filesystem::path path_;
    bool active_ = true;
};

} // namespace

PublishResult PublishHardwareInfoJsonl(
    const boost::filesystem::path& outputDirectory, std::string_view jsonl, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }

    const boost::filesystem::path lockPath = outputDirectory / kLockFileName;
    FileDescriptor lockDescriptor(
        ::open(lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR));
    if (lockDescriptor.Get() < 0) {
        SetErrno(error, "open HardwareInfo lock file failed: " + lockPath.string(), errno);
        return PublishResult::Failed;
    }
    if (!LockExclusive(lockDescriptor.Get(), error)) {
        return PublishResult::Failed;
    }

    const boost::filesystem::path finalPath = outputDirectory / kFinalFileName;
    struct stat finalStatus {};
    if (::lstat(finalPath.c_str(), &finalStatus) == 0) {
        if (S_ISREG(finalStatus.st_mode)) {
            return PublishResult::AlreadyPublished;
        }
        SetError(error, "HardwareInfo output exists but is not a regular file: " + finalPath.string());
        return PublishResult::Failed;
    }
    if (errno != ENOENT) {
        SetErrno(error, "inspect HardwareInfo output failed: " + finalPath.string(), errno);
        return PublishResult::Failed;
    }

    const boost::filesystem::path temporaryPath =
        outputDirectory / (std::string(kTemporaryPrefix) + std::to_string(::getpid()));
    FileDescriptor temporaryDescriptor(
        ::open(temporaryPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR));
    if (temporaryDescriptor.Get() < 0) {
        SetErrno(error, "create HardwareInfo temporary file failed: " + temporaryPath.string(), errno);
        return PublishResult::Failed;
    }
    TemporaryFileCleanup cleanup(temporaryPath);

    if (!WriteAll(temporaryDescriptor.Get(), jsonl, error) || !SyncFile(temporaryDescriptor.Get(), error) ||
        !temporaryDescriptor.Close(error)) {
        return PublishResult::Failed;
    }
    if (::rename(temporaryPath.c_str(), finalPath.c_str()) != 0) {
        SetErrno(error, "publish HardwareInfo file failed: " + finalPath.string(), errno);
        return PublishResult::Failed;
    }

    cleanup.Release();
    return PublishResult::Published;
}

} // namespace npu_compute
