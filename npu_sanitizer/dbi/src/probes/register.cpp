/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "trace_record.h"

// SET_PADDING, API ID 392.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_padding(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t value)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 392, value,
        0UL, 0UL, 0UL, 0UL);
}
