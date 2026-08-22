/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_ACL_SAN_LOG_H
#define ASCSAN_ACL_SAN_LOG_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

enum AclsanLogLevel { ACLSAN_LOG_DEBUG = 0, ACLSAN_LOG_INFO = 1, ACLSAN_LOG_WARNING = 2, ACLSAN_LOG_ERROR = 3 };

inline AclsanLogLevel AclsanGetLogLevel() noexcept
{
    const char* value = std::getenv("ASCEND_GLOBAL_LOG_LEVEL");
    if (value == nullptr || value[0] < '0' || value[0] > '3' || value[1] != '\0') {
        return ACLSAN_LOG_ERROR;
    }
    return static_cast<AclsanLogLevel>(value[0] - '0');
}

inline bool AclsanIsStdoutLogEnabled() noexcept
{
    // const char* value = std::getenv("ASCEND_SLOG_PRINT_TO_STDOUT");
    const char* value = std::getenv("NPU_SAN_DEBUG");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

inline const char* AclsanGetLogLevelName(AclsanLogLevel level) noexcept
{
    static const char* const levelNames[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    if (level < ACLSAN_LOG_DEBUG || level > ACLSAN_LOG_ERROR) {
        return "ERROR";
    }
    return levelNames[level];
}

inline void AclsanLogWrite(AclsanLogLevel level, const char* format, ...) noexcept
{
    if (format == nullptr || level < ACLSAN_LOG_DEBUG || level > ACLSAN_LOG_ERROR) {
        return;
    }
    if (level != ACLSAN_LOG_ERROR && (!AclsanIsStdoutLogEnabled() || level < AclsanGetLogLevel())) {
        return;
    }

    std::FILE* output = level == ACLSAN_LOG_ERROR ? stderr : stdout;
    (void)std::fprintf(output, "[ASC_SAN][%s] ", AclsanGetLogLevelName(level));
    std::va_list arguments;
    va_start(arguments, format);
    (void)std::vfprintf(output, format, arguments);
    va_end(arguments);
    (void)std::fputc('\n', output);
    (void)std::fflush(output);
}

#define ASC_SAN_DEBUG(...) AclsanLogWrite(ACLSAN_LOG_DEBUG, __VA_ARGS__)
#define ASC_SAN_INFO(...) AclsanLogWrite(ACLSAN_LOG_INFO, __VA_ARGS__)
#define ASC_SAN_WARNING(...) AclsanLogWrite(ACLSAN_LOG_WARNING, __VA_ARGS__)
#define ASC_SAN_ERROR(...) AclsanLogWrite(ACLSAN_LOG_ERROR, __VA_ARGS__)

#endif
