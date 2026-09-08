// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "plog_sink.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <vector>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <sys/syscall.h>
#include <unistd.h>

namespace aclsan {
namespace {

// TODO: Switch to ASCTOOL once the supported CANN package provides it.
constexpr int32_t kPlogModuleId = static_cast<int32_t>(ASCENDCKERNEL);

using CheckLogLevelFn = int32_t (*)(int32_t moduleId, int32_t level);
using DlogRecordFn = void (*)(int32_t moduleId, int32_t level, const char* format, ...);

struct PlogApi {
    CheckLogLevelFn checkLogLevel = nullptr;
    DlogRecordFn dlogRecord = nullptr;
};

std::once_flag g_resolveOnce;
PlogApi g_resolvedApi;

int32_t ToDlogLevel(PlogLevel level) noexcept
{
    switch (level) {
        case PlogLevel::kDebug:
            return DLOG_DEBUG;
        case PlogLevel::kInfo:
            return DLOG_INFO;
        case PlogLevel::kWarning:
            return DLOG_WARN;
        case PlogLevel::kError:
            return DLOG_ERROR;
    }
    return DLOG_ERROR;
}

const char* FileName(const char* file) noexcept
{
    if (file == nullptr) {
        return "<unknown>";
    }
    const char* separator = std::strrchr(file, '/');
    return separator == nullptr ? file : separator + 1;
}

void ResolvePlogApi() noexcept
{
    const std::array<const char*, 2U> libraryNames = {"libunified_dlog.so", "libslog.so"};
    for (const char* libraryName : libraryNames) {
        void* const handle = dlopen(libraryName, RTLD_LAZY | RTLD_LOCAL);
        if (handle == nullptr) {
            continue;
        }
        const auto checkLogLevel = reinterpret_cast<CheckLogLevelFn>(dlsym(handle, "CheckLogLevel"));
        const auto dlogRecord = reinterpret_cast<DlogRecordFn>(dlsym(handle, "DlogRecord"));
        if ((checkLogLevel != nullptr) && (dlogRecord != nullptr)) {
            g_resolvedApi = {checkLogLevel, dlogRecord};
            return;
        }
        (void)dlclose(handle);
    }
}

PlogApi GetPlogApi()
{
    std::call_once(g_resolveOnce, ResolvePlogApi);
    return g_resolvedApi;
}

} // namespace

void WritePlog(
    PlogLevel level, std::string_view message, const char* file, uint32_t line, const char* function) noexcept
{
    try {
        const PlogApi api = GetPlogApi();
        if ((api.checkLogLevel == nullptr) || (api.dlogRecord == nullptr)) {
            return;
        }

        const int32_t dlogLevel = ToDlogLevel(level);
        if (api.checkLogLevel(kPlogModuleId, dlogLevel) != 1) {
            return;
        }

        constexpr size_t kMessageCapacity = 1024U;
        std::array<char, kMessageCapacity> formatted{};
        const int written = std::snprintf(
            formatted.data(), formatted.size(), "[%s:%u] %ld %s: ", FileName(file), line,
            static_cast<long>(syscall(SYS_gettid)), function == nullptr ? "<unknown>" : function);
        if (written < 0 || static_cast<size_t>(written) >= formatted.size() - 1U) {
            return;
        }
        const size_t prefixSize = static_cast<size_t>(written);
        const size_t capacity = formatted.size() - prefixSize - 1U;
        // CANN records are bounded. Split lines and long payloads without losing diagnostics.
        size_t offset = 0;
        do {
            const size_t newline = message.find('\n', offset);
            const size_t lineEnd = newline == std::string_view::npos ? message.size() : newline;
            const size_t bytes = std::min(capacity, lineEnd - offset);
            if (bytes != 0) {
                std::memcpy(formatted.data() + prefixSize, message.data() + offset, bytes);
            }
            formatted[prefixSize + bytes] = '\0';
            api.dlogRecord(kPlogModuleId | static_cast<int32_t>(DEBUG_LOG_MASK), dlogLevel, "%s", formatted.data());
            offset += bytes;
            if (offset == newline) {
                ++offset;
            }
        } while (offset < message.size());
    } catch (...) {
        return;
    }
}

void WritePlogFormat(
    PlogLevel level, const char* file, uint32_t line, const char* function, const char* format, ...) noexcept
{
    if (format == nullptr) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    try {
        const PlogApi api = GetPlogApi();
        if (api.checkLogLevel == nullptr || api.dlogRecord == nullptr ||
            api.checkLogLevel(kPlogModuleId, ToDlogLevel(level)) != 1) {
            va_end(arguments);
            return;
        }
        std::array<char, 1024> buffer{};
        va_list measured;
        va_copy(measured, arguments);
        const int length = std::vsnprintf(buffer.data(), buffer.size(), format, measured);
        va_end(measured);
        if (length >= 0 && static_cast<size_t>(length) < buffer.size()) {
            WritePlog(level, std::string_view(buffer.data(), static_cast<size_t>(length)), file, line, function);
        } else if (length >= 0) {
            std::vector<char> message(static_cast<size_t>(length) + 1U);
            const int written = std::vsnprintf(message.data(), message.size(), format, arguments);
            if (written >= 0 && static_cast<size_t>(written) < message.size()) {
                WritePlog(level, std::string_view(message.data(), static_cast<size_t>(written)), file, line, function);
            }
        }
    } catch (...) {
        // Logging must not change the result of a Runtime API call.
    }
    va_end(arguments);
}

} // namespace aclsan
