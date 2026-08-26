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

#include "acl/acl_rt.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aclsan {

namespace device_runtime {
struct CallStackResult;
}

struct PreparedTraceLaunch {
    bool instrumented = false;
    uint64_t launchId = 0;
    uint32_t blockCount = 0;
    uint32_t recordsPerBlock = 0;
    void* deviceBuffer = nullptr;
    std::vector<uint8_t> hostBuffer;
    std::vector<uint8_t> arguments;
    std::vector<aclrtPlaceHolderInfo> placeholders;
};

void RecordTraceBinaryLoadFromData(
    aclrtBinHandle binary, bool instrumented, const void* image, size_t imageBytes) noexcept;
void RecordTraceBinaryUnload(aclrtBinHandle binary) noexcept;
void RecordTraceBinaryFunctionLookup(aclrtBinHandle binary, aclrtFuncHandle function) noexcept;
void RecordTraceFunctionLookup(aclrtFuncHandle function) noexcept;
void MarkTraceFunctionInstrumented(aclrtFuncHandle function) noexcept;

aclError PrepareTraceLaunch(
    aclrtFuncHandle function, uint32_t blockCount, const void* hostArgs, size_t argsSize,
    const aclrtPlaceHolderInfo* placeholders, size_t placeholderCount, PreparedTraceLaunch& prepared) noexcept;
void CompleteTraceLaunch(
    PreparedTraceLaunch&& prepared, aclrtFuncHandle function, aclrtStream stream, aclError launchResult) noexcept;
void CollectTraceStream(aclrtStream stream) noexcept;
void ResetTraceRuntimeState() noexcept;
device_runtime::CallStackResult ResolveTraceDeviceCallStack(uint64_t pc) noexcept;

} // namespace aclsan
