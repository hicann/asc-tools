/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_ACL_SAN_INTERNAL_H
#define ACLSAN_ACL_SAN_INTERNAL_H

#include "aclsan/aclsan_api.h"
#include "cce_instr/cce_instr_struct_dma.h"
#include "cce_instr/cce_instr_struct_sync.h"
#include "internal/aclsan_device_record.h"
#include "internal/aclsan_dispatch_cb.h"
#include "npu_compute/injection_hook.h"

#include <optional>
#include <set>
#include <variant>

namespace sanitizer {
struct ProbeParseResult;
}

namespace aclsan {
namespace probe {
struct CallStackResult;
}

using CceInstructionParamField = std::variant<
    sanitizer::CopyGmToUbufAlignV2ParamField, sanitizer::CopyGmToCbufAlignV2ParamField,
    sanitizer::CopyUbufToGmAlignV2ParamField, sanitizer::CopyGmToCbufV2ParamField, sanitizer::FlagParamField,
    sanitizer::BufferParamField>;

using CceTraceCallbackData = std::variant<DeviceMemoryAccessDataArray, AclsanDeviceSyncData>;

struct TraceCallbackContext {
    uint64_t transferBytes;
    uint64_t instrExecId;
    uint64_t serialNo;
    uint32_t coreId;
};

AclsanStatus ApplyRuntimeHooks(const std::set<aclrtApiId>& requiredHooks) noexcept;
AclsanStatus ResolveActiveDeviceCallStack(uint64_t pc, probe::CallStackResult* result) noexcept;
bool IsRuntimeHookStatePoisoned() noexcept;
bool IsCallbackEnabled(AclsanCallbackDomain domain, AclsanCallbackId id) noexcept;
bool InvokeCallback(AclsanCallbackDomain domain, AclsanCallbackId id, const void* callbackData) noexcept;

DeviceMemoryAccessDataArray TranslateDeviceMemoryAccessData(const DeviceRecord& record) noexcept;
AclsanDeviceSyncData TranslateDeviceSyncData(const DeviceRecord& record) noexcept;
std::optional<CceInstructionParamField> TranslateRawTraceRecord(const sanitizer::AscsanRawTraceRecord& record) noexcept;
std::optional<CceTraceCallbackData> TranslateRawTraceToCallbackData(
    const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context) noexcept;
void DispatchProbeRecords(const sanitizer::ProbeParseResult& parseResult) noexcept;

} // namespace aclsan

#endif
