// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_SANITIZER_COMMON_PLOG_SINK_H
#define NPU_SANITIZER_COMMON_PLOG_SINK_H

#include <cstdint>
#include <string_view>

#include "dlog_pub.h"

namespace npucheck {

enum class PlogLevel : uint8_t {
    DEBUG = 0U,
    INFO = 1U,
    WARNING = 2U,
    ERROR = 3U,
};

void WritePlog(
    PlogLevel level, std::string_view message, const char* file = __builtin_FILE(), uint32_t line = __builtin_LINE(),
    const char* function = __builtin_FUNCTION()) noexcept;

void WritePlogFormat(
    PlogLevel level, const char* file, uint32_t line, const char* function, const char* format, ...) noexcept
    __attribute__((format(printf, 5, 6)));

} // namespace npucheck

// Compatibility imports for sanitizer_api, whose source namespace is unchanged.
namespace aclsan {
using npucheck::PlogLevel;
using npucheck::WritePlog;
using npucheck::WritePlogFormat;
} // namespace aclsan

// Format adapters only: levels and output are owned by the common CANN plog sink.
#define ACL_SAN_DEBUG(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ACL_SAN_INFO(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ACL_SAN_WARNING(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ACL_SAN_ERROR(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

// The caller provides ACL types; message is evaluated only when expression fails.
#define ACLSAN_RETURN_IF_ACL_ERROR(expression, message)              \
    do {                                                             \
        const aclError aclsanStatus = (expression);                  \
        if (aclsanStatus != ACL_SUCCESS) {                           \
            ACL_SAN_ERROR("%s: result=%d", (message), aclsanStatus); \
            return aclsanStatus;                                     \
        }                                                            \
    } while (false)

#define ACLSAN_CHECK_NULLPTR(apiName, argument)                       \
    do {                                                              \
        if ((argument) == nullptr) {                                  \
            ACL_SAN_ERROR("%s: %s is nullptr", (apiName), #argument); \
            return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;             \
        }                                                             \
    } while (false)

#endif // NPU_SANITIZER_COMMON_PLOG_SINK_H
