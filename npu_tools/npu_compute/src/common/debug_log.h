/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_TOOLS_NPU_COMPUTE_SRC_COMMON_DEBUG_LOG_H
#define NPU_TOOLS_NPU_COMPUTE_SRC_COMMON_DEBUG_LOG_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined(__GNUC__) || defined(__clang__)
#define NPU_COMPUTE_PRINTF_FORMAT(formatIndex, firstArgument) \
    __attribute__((format(printf, formatIndex, firstArgument)))
#else
#define NPU_COMPUTE_PRINTF_FORMAT(formatIndex, firstArgument)
#endif

namespace npucompute::detail {

inline bool DebugEnabled()
{
    const char* value = std::getenv("NPU_COMPUTE_DEBUG");
    return value != nullptr && std::strcmp(value, "1") == 0;
}

NPU_COMPUTE_PRINTF_FORMAT(2, 3)
inline void DebugLog(const char* component, const char* format, ...)
{
    if (!DebugEnabled()) {
        return;
    }

    const char* componentText = component != nullptr ? component : "unknown";
    const char* formatText = format != nullptr ? format : "";
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);
    std::fprintf(stderr, "[%s] ", componentText);
    std::va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, formatText, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
}

} // namespace npucompute::detail

#undef NPU_COMPUTE_PRINTF_FORMAT

#endif // NPU_TOOLS_NPU_COMPUTE_SRC_COMMON_DEBUG_LOG_H
