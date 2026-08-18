// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/diagnostic.h"

#include "aclsan/aclsan_types.h"

#include <iomanip>
#include <sstream>

namespace npu::sanitizer {

const char* DiagnosticCode(DiagnosticKind kind)
{
    switch (kind) {
        case DiagnosticKind::OUT_OF_BOUNDS:
            return "NPU-MEM-OUT-OF-BOUNDS";
        case DiagnosticKind::USE_AFTER_FREE:
            return "NPU-MEM-USE-AFTER-FREE";
        case DiagnosticKind::AMBIGUOUS_ADDRESS:
            return "NPU-MEM-AMBIGUOUS-ADDRESS";
        case DiagnosticKind::UNKNOWN_ADDRESS:
            return "NPU-MEM-UNKNOWN-ADDRESS";
        case DiagnosticKind::RANGE_OVERFLOW:
            return "NPU-MEM-RANGE-OVERFLOW";
        case DiagnosticKind::INVALID_FREE:
            return "NPU-MEM-INVALID-FREE";
        case DiagnosticKind::DOUBLE_FREE:
            return "NPU-MEM-DOUBLE-FREE";
        case DiagnosticKind::OVERLAPPING_ALLOCATION:
            return "NPU-MEM-OVERLAPPING-ALLOCATION";
        case DiagnosticKind::INVALID_ALLOCATION:
            return "NPU-MEM-INVALID-ALLOCATION";
        case DiagnosticKind::PENDING_OPERATION_DROPPED:
            return "NPU-MEM-PENDING-OPERATION-DROPPED";
        case DiagnosticKind::MALFORMED_CALLBACK:
            return "NPU-CHECK-MALFORMED-CALLBACK";
    }
    return "NPU-CHECK-UNKNOWN";
}

const char* SeverityName(Severity severity) { return severity == Severity::ERROR ? "ERROR" : "WARNING"; }

const char* AccessKindName(AccessKind kind)
{
    switch (kind) {
        case AccessKind::READ:
            return "READ";
        case AccessKind::WRITE:
            return "WRITE";
        case AccessKind::NONE:
            return "NONE";
    }
    return "NONE";
}

const char* PipelineName(uint32_t pipeline)
{
    switch (pipeline) {
        case ACLSAN_PATCH_PIPELINE_MTE2:
            return "MTE2";
        case ACLSAN_PATCH_PIPELINE_MTE3:
            return "MTE3";
        case ACLSAN_PATCH_PIPELINE_FIXPIPE:
            return "FIXPIPE";
        case ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            return "SET_WAIT_FLAG";
        case ACLSAN_PATCH_PIPELINE_GET_RLS_BUF:
            return "GET_RLS_BUF";
        default:
            return "UNKNOWN";
    }
}

std::string FormatDiagnostic(const Diagnostic& diagnostic, uint64_t ordinal)
{
    std::ostringstream output;
    output << "========= NPU-CHECK\n";
    output << SeverityName(diagnostic.severity) << " " << ordinal << ": [" << DiagnosticCode(diagnostic.kind) << "] "
           << diagnostic.detail;
    if (diagnostic.probable) {
        output << " (probable)";
    }
    output << "\n";
    if (diagnostic.access != AccessKind::NONE) {
        output << "  access: " << AccessKindName(diagnostic.access) << " operation=" << diagnostic.operation
               << " address=0x" << std::hex << diagnostic.address << std::dec << " bytes=" << diagnostic.bytes << "\n";
    }
    if (diagnostic.allocation.present) {
        output << "  allocation: resource=" << diagnostic.allocation.resourceId
               << " generation=" << diagnostic.allocation.generation << " device=" << diagnostic.allocation.deviceId
               << " range=[0x" << std::hex << diagnostic.allocation.base << ",0x"
               << (diagnostic.allocation.base + diagnostic.allocation.bytes) << ")" << std::dec << "\n";
    }
    if (diagnostic.instruction.present) {
        output << "  instruction: pipeline=" << PipelineName(diagnostic.instruction.pipeline)
               << " source_kind=" << diagnostic.instruction.sourceKind
               << " memory_space=" << diagnostic.instruction.memorySpace
               << " access=" << diagnostic.instruction.accessIndex << "/" << diagnostic.instruction.accessCount
               << " layout=" << diagnostic.instruction.layoutKind << " site=" << diagnostic.instruction.siteId
               << " device=" << diagnostic.instruction.deviceId << " core=" << diagnostic.instruction.coreId
               << " block=" << diagnostic.instruction.blockId << " block_type=" << diagnostic.instruction.blockType
               << " launch=" << diagnostic.instruction.launchId << " instr_exec=" << diagnostic.instruction.instrExecId
               << " serial=" << diagnostic.instruction.serialNo << " pc=0x" << std::hex << diagnostic.instruction.pc
               << std::dec << " flags=0x" << std::hex << diagnostic.instruction.flags << std::dec << "\n";
    }
    if (!diagnostic.source.empty()) {
        output << "  source: " << diagnostic.source << "\n";
    }
    if (!diagnostic.suggestion.empty()) {
        output << "  suggestion: " << diagnostic.suggestion << "\n";
    }
    return output.str();
}

} // namespace npu::sanitizer
