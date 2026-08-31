// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_LOGGING_LOGGER_H
#define NPU_CHECK_LOGGING_LOGGER_H

#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace npu::sanitizer::logging {

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
};

struct LogSourceLocation {
    const char* file = nullptr;
    uint32_t line = 0;

    static constexpr LogSourceLocation Current(
        const char* file = __builtin_FILE(), uint32_t line = __builtin_LINE()) noexcept
    {
        return {file, line};
    }
};

class Logger {
public:
    using ErrorSink = std::function<void(const std::string&)>;

    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool Open(const std::string& path, LogLevel minimumLevel, std::string& error);
    void SetErrorSink(ErrorSink sink);
    void Debug(std::string_view message, LogSourceLocation location = LogSourceLocation::Current()) noexcept;
    void Info(std::string_view message, LogSourceLocation location = LogSourceLocation::Current()) noexcept;
    void Warning(std::string_view message, LogSourceLocation location = LogSourceLocation::Current()) noexcept;
    void Error(std::string_view message, LogSourceLocation location = LogSourceLocation::Current()) noexcept;
    void Flush() noexcept;

    std::string Path() const;
    static LogLevel ConfiguredLevel() noexcept;

private:
    void Write(LogLevel level, std::string_view message, LogSourceLocation location) noexcept;

    mutable std::mutex mutex_;
    std::ofstream stream_;
    std::string path_;
    LogLevel minimumLevel_ = LogLevel::DEBUG;
    ErrorSink errorSink_;
};

} // namespace npu::sanitizer::logging

#endif
