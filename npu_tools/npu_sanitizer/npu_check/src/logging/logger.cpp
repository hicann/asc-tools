// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "logging/logger.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <utility>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

namespace npu::sanitizer::logging {
namespace {

constexpr const char* kLogLevelEnvironment = "NPU_CHECK_LOG_LEVEL";

std::array<char, 32> Timestamp() noexcept
{
    std::array<char, 32> timestamp{};
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now - seconds).count();
    if (microseconds < 0) {
        microseconds += 1000000;
    }

    const std::time_t time = static_cast<std::time_t>(seconds.count());
    std::tm localTime{};
    if (localtime_r(&time, &localTime) == nullptr) {
        return timestamp;
    }

    std::array<char, 20> date{};
    if (std::strftime(date.data(), date.size(), "%Y-%m-%d-%H:%M:%S", &localTime) == 0) {
        return timestamp;
    }
    (void)std::snprintf(
        timestamp.data(), timestamp.size(), "%s.%03lld.%03lld", date.data(),
        static_cast<long long>(microseconds / 1000), static_cast<long long>(microseconds % 1000));
    return timestamp;
}

std::string_view FileName(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        return "<unknown>";
    }
    const std::string_view file(path);
    const size_t separator = file.find_last_of("/\\");
    return separator == std::string_view::npos ? file : file.substr(separator + 1);
}

const char* LogLevelName(LogLevel level) noexcept
{
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
    }
    return "ERROR";
}

} // namespace

bool Logger::Open(const std::string& path, LogLevel minimumLevel, std::string& error)
{
    if (path.empty()) {
        error = "npu_check log path is empty";
        return false;
    }
    const boost::filesystem::path filePath(path);
    boost::system::error_code filesystemError;
    if (!filePath.parent_path().empty()) {
        boost::filesystem::create_directories(filePath.parent_path(), filesystemError);
    }
    if (filesystemError) {
        error = "cannot create npu_check log directory: " + filesystemError.message();
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    stream_.open(path, std::ios::out | std::ios::trunc);
    if (!stream_.is_open()) {
        error = "cannot open npu_check log file: " + path;
        return false;
    }
    path_ = path;
    minimumLevel_ = minimumLevel;
    return true;
}

void Logger::SetErrorSink(ErrorSink sink)
{
    std::lock_guard<std::mutex> lock(mutex_);
    errorSink_ = std::move(sink);
}

void Logger::Debug(std::string_view message, LogSourceLocation location) noexcept
{
    Write(LogLevel::DEBUG, message, location);
}

void Logger::Info(std::string_view message, LogSourceLocation location) noexcept
{
    Write(LogLevel::INFO, message, location);
}

void Logger::Warning(std::string_view message, LogSourceLocation location) noexcept
{
    Write(LogLevel::WARNING, message, location);
}

void Logger::Error(std::string_view message, LogSourceLocation location) noexcept
{
    Write(LogLevel::ERROR, message, location);
}

void Logger::Flush() noexcept
{
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_.is_open()) {
            stream_.flush();
        }
    } catch (...) {
        return;
    }
}

std::string Logger::Path() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

LogLevel Logger::ConfiguredLevel() noexcept
{
    const char* value = std::getenv(kLogLevelEnvironment);
    if (value == nullptr) {
        return LogLevel::DEBUG;
    }
    const std::string level(value);
    if (level == "INFO") {
        return LogLevel::INFO;
    }
    if (level == "WARNING") {
        return LogLevel::WARNING;
    }
    if (level == "ERROR") {
        return LogLevel::ERROR;
    }
    return LogLevel::DEBUG;
}

void Logger::Write(LogLevel level, std::string_view message, LogSourceLocation location) noexcept
{
    try {
        ErrorSink errorSink;
        std::string errorMessage;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!stream_.is_open() || level < minimumLevel_) {
                return;
            }
            stream_ << '[' << LogLevelName(level) << "]NPU_CHECK(pid:" << getpid() << "):" << Timestamp().data() << " ["
                    << FileName(location.file) << ':' << location.line << "] " << message << '\n';
            stream_.flush();
            if (level == LogLevel::ERROR) {
                errorSink = errorSink_;
                errorMessage.assign(message);
            }
        }
        if (errorSink) {
            errorSink(errorMessage);
        }
    } catch (...) {
        return;
    }
}

} // namespace npu::sanitizer::logging
