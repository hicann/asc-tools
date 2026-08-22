// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_DIAGNOSTIC_DEVICE_PROTOCOL_H
#define NPU_CHECK_DIAGNOSTIC_DEVICE_PROTOCOL_H

#include <cstdint>

namespace npu::sanitizer {

enum class DeviceSourceKind : uint32_t {
    MTE2 = 1,
    MTE3 = 2,
    FIXPIPE = 3,
    SET_WAIT_FLAG = 4,
    GET_RLS_BUF = 5,
};

constexpr uint32_t kDeviceEventFlagPredicated = 1u << 3u;

} // namespace npu::sanitizer

#endif
