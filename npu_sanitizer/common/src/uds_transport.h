/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_COMMON_UDS_TRANSPORT_H
#define NPU_CHECK_COMMON_UDS_TRANSPORT_H

#include "wire_protocol.h"

#include <cstdint>
#include <string>

namespace npu::sanitizer::ipc {

enum class IoStatus : uint8_t {
    OK,
    CLOSED,
    TIMEOUT,
    SYSTEM_ERROR,
    PROTOCOL_ERROR,
};

IoStatus SendFrame(int fd, const Frame& frame, std::string& error);
IoStatus ReceiveFrame(int fd, Frame& frame, std::string& error);
bool SetSocketTimeouts(int fd, int timeoutMs, std::string& error);
bool WaitReadable(int fd, int timeoutMs, std::string& error);

} // namespace npu::sanitizer::ipc

#endif
