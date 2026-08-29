// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#pragma once

#include "dbi_pipeline.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace aclsan {

struct BinaryInstrumentationConfig {
    std::string arch;
    std::vector<ProbeGroup> probeGroups;
    std::string toolchainRoot;
    std::string sourceRoot;
    std::string workDirectory;
    std::string cacheDirectory;
    bool strict = false;
    bool keepTemp = false;
    std::vector<std::string> compilerArgs;
    std::vector<std::string> tuneArgs;
};

enum class BinaryInstrumentationStatus : uint32_t { Skipped, Instrumented, Failed };

struct BinaryInstrumentationResult {
    BinaryInstrumentationStatus status = BinaryInstrumentationStatus::Skipped;
    std::vector<uint8_t> binary;
    std::string stage;
    std::string diagnostic;
};

using DbiPipelineRunner = DbiResult (*)(const DbiRequest&, void*);
using InstrumentedBinaryConsumer = int32_t (*)(const void*, size_t, void*);

// acl_san and the DBI engine may use different libstdc++ ABIs. Keep this runtime boundary POD-only.
struct RuntimeBinaryInstrumentationResult {
    BinaryInstrumentationStatus status = BinaryInstrumentationStatus::Skipped;
    uint32_t strict = 0;
    int32_t consumerStatus = 0;
};

static_assert(std::is_trivially_copyable_v<RuntimeBinaryInstrumentationResult>);
static_assert(sizeof(RuntimeBinaryInstrumentationResult) == 12);

BinaryInstrumentationConfig DefaultBinaryInstrumentationConfig();
BinaryInstrumentationConfig DefaultBinaryInstrumentationConfig(uint32_t probeGroupMask);
BinaryInstrumentationResult InstrumentBinary(
    const BinaryInstrumentationConfig& config, const void* data, size_t length, DbiPipelineRunner runner,
    void* runnerData = nullptr);
BinaryInstrumentationResult InstrumentBinary(
    const BinaryInstrumentationConfig& config, const void* data, size_t length);
RuntimeBinaryInstrumentationResult InstrumentRuntimeBinary(
    const void* data, size_t length, uint32_t probeGroupMask, InstrumentedBinaryConsumer consumer, void* consumerData,
    DbiPipelineRunner runner, void* runnerData = nullptr) noexcept;
// Cross-target callers use this overload; the runner overload above is a same-ABI test seam.
RuntimeBinaryInstrumentationResult InstrumentRuntimeBinary(
    const void* data, size_t length, uint32_t probeGroupMask, InstrumentedBinaryConsumer consumer,
    void* consumerData) noexcept;

} // namespace aclsan
