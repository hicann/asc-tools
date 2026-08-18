// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_DIAGNOSTIC_DIAGNOSTIC_H
#define NPU_CHECK_DIAGNOSTIC_DIAGNOSTIC_H

#include <cstdint>
#include <string>

namespace npu::sanitizer {

enum class Severity : uint8_t {
    WARNING,
    ERROR,
};

enum class AccessKind : uint8_t {
    NONE,
    READ,
    WRITE,
};

enum class DiagnosticKind : uint8_t {
    OUT_OF_BOUNDS,
    USE_AFTER_FREE,
    AMBIGUOUS_ADDRESS,
    UNKNOWN_ADDRESS,
    RANGE_OVERFLOW,
    INVALID_FREE,
    DOUBLE_FREE,
    OVERLAPPING_ALLOCATION,
    INVALID_ALLOCATION,
    PENDING_OPERATION_DROPPED,
    MALFORMED_CALLBACK,
};

struct AllocationContext {
    bool present = false;
    uint64_t resourceId = 0;
    uint64_t generation = 0;
    uint64_t base = 0;
    uint64_t bytes = 0;
    uint32_t deviceId = 0;
};

struct InstructionContext {
    bool present = false;
    uint32_t pipeline = 0;
    uint32_t sourceKind = 0;
    uint32_t memorySpace = 0;
    uint32_t accessIndex = 0;
    uint32_t accessCount = 0;
    uint32_t layoutKind = 0;
    uint32_t siteId = 0;
    uint32_t blockId = 0;
    uint32_t blockType = 0;
    uint32_t deviceId = 0;
    uint32_t coreId = 0;
    uint64_t launchId = 0;
    uint64_t instrExecId = 0;
    uint64_t serialNo = 0;
    uint64_t binaryId = 0;
    uint64_t functionId = 0;
    uint64_t pc = 0;
    uint32_t flags = 0;
};

struct Diagnostic {
    DiagnosticKind kind = DiagnosticKind::MALFORMED_CALLBACK;
    Severity severity = Severity::ERROR;
    AccessKind access = AccessKind::NONE;
    std::string operation;
    uint64_t address = 0;
    uint64_t bytes = 0;
    AllocationContext allocation;
    InstructionContext instruction;
    std::string source;
    std::string detail;
    std::string suggestion;
    bool probable = false;
};

const char* DiagnosticCode(DiagnosticKind kind);
const char* SeverityName(Severity severity);
const char* AccessKindName(AccessKind kind);
const char* PipelineName(uint32_t pipeline);
std::string FormatDiagnostic(const Diagnostic& diagnostic, uint64_t ordinal);

} // namespace npu::sanitizer

#endif
