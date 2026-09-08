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

namespace aclsan {

enum class PlogLevel : uint8_t {
    kDebug = 0U,
    kInfo = 1U,
    kWarning = 2U,
    kError = 3U,
};

void WritePlog(
    PlogLevel level, std::string_view message, const char* file = __builtin_FILE(), uint32_t line = __builtin_LINE(),
    const char* function = __builtin_FUNCTION()) noexcept;

void WritePlogFormat(
    PlogLevel level, const char* file, uint32_t line, const char* function, const char* format, ...) noexcept
    __attribute__((format(printf, 5, 6)));

} // namespace aclsan

#endif // NPU_SANITIZER_COMMON_PLOG_SINK_H
