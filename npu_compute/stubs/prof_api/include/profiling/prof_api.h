/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#pragma once

#include "profiling/prof_common.h"

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

std::int32_t MsprofRegisterDataCallback(std::uint32_t type, void* func);
std::int32_t MsprofStart(std::uint32_t dataType, const void* data, std::uint32_t length);
std::int32_t MsprofStop(std::uint32_t dataType, const void* data, std::uint32_t length);

#ifdef __cplusplus
}
#endif
