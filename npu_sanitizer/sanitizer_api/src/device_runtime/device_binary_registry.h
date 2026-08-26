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

#include "device_symbolizer.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace aclsan::device_runtime {

class DeviceBinaryRegistry final {
public:
    DeviceBinaryRegistry();
    ~DeviceBinaryRegistry();

    DeviceBinaryRegistry(const DeviceBinaryRegistry&) = delete;
    DeviceBinaryRegistry& operator=(const DeviceBinaryRegistry&) = delete;

    bool RecordBinaryLoadFromData(uintptr_t binary, bool instrumented, const void* image, size_t imageBytes) noexcept;
    void RecordBinaryUnload(uintptr_t binary) noexcept;
    void RecordBinaryFunctionLookup(uintptr_t binary, uintptr_t function) noexcept;
    void RecordLatestBinaryFunctionLookup(uintptr_t function) noexcept;
    void MarkFunctionInstrumented(uintptr_t function) noexcept;
    bool IsFunctionInstrumented(uintptr_t function) const noexcept;
    void Reset() noexcept;

    CallStackResult ResolveCallStack(uint64_t pc) const noexcept;
    CallStackResult ResolveCallStackWithRunner(uint64_t pc, const CommandRunner& runner) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aclsan::device_runtime
