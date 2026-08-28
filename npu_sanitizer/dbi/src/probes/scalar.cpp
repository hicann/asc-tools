/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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

// SET_MTE2_NZ_PARA, API ID 399.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_mte2_nz_para(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 399,
        config & 0xFFFFUL, 0UL, 0UL, 0UL, 0UL);
}

// MTE2_SRC_PARA, API ID 124.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_mte2_src_para(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 124, config,
        0UL, 0UL, 0UL, 0UL);
}

// LOOP0_STRIDE_NDDMA, API ID 132.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_loop0_stride_nddma(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 132, config,
        0UL, 0UL, 0UL, 0UL);
}

// LOOP1_STRIDE_NDDMA, API ID 133.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_loop1_stride_nddma(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 133, config,
        0UL, 0UL, 0UL, 0UL);
}

// LOOP2_STRIDE_NDDMA, API ID 134.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_loop2_stride_nddma(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 134, config,
        0UL, 0UL, 0UL, 0UL);
}

// LOOP3_STRIDE_NDDMA, API ID 135.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_loop3_stride_nddma(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 135, config,
        0UL, 0UL, 0UL, 0UL);
}

// LOOP4_STRIDE_NDDMA, API ID 136.
extern __attribute__((noinline)) __attribute__((weak)) __aicore__ void __sanitizer_report_set_loop4_stride_nddma(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint64_t config)
{
    aclsan::WriteTraceRecord(
        memInfo, pc, bid, aclsan::DeviceInstructionCategory::RegisterState, static_cast<uint16_t>(PIPE_S), 136, config,
        0UL, 0UL, 0UL, 0UL);
}
