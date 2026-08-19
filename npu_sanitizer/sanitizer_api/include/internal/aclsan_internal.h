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
#include "internal/aclsan_device_record.h"
#include "npu_compute/injection_hook.h"

#include <array>
#include <cstddef>
#include <set>

namespace aclsan {

AclsanStatus ApplyRuntimeHooks(const std::set<aclrtApiId>& requiredHooks) noexcept;
bool IsRuntimeHookStatePoisoned() noexcept;
bool IsCallbackEnabled(AclsanCallbackDomain domain, AclsanCallbackId id) noexcept;
bool InvokeCallback(AclsanCallbackDomain domain, AclsanCallbackId id, const void* callbackData) noexcept;

constexpr std::size_t kDataCopyAccessCount = 2;
using DeviceMemoryAccessDataArray = std::array<AclsanDeviceMemoryAccessData, kDataCopyAccessCount>;

DeviceMemoryAccessDataArray TranslateDeviceMemoryAccessData(const DeviceRecord& record) noexcept;
AclsanDeviceSyncData TranslateDeviceSyncData(const DeviceRecord& record) noexcept;
void DispatchDeviceMemoryAccessData(const DeviceMemoryAccessDataArray& callbackData) noexcept;
void DispatchDeviceSyncData(const AclsanDeviceSyncData& callbackData) noexcept;
void DispatchMockDeviceRecords() noexcept;
void DispatchSynchronizeEnd(void* stream, int result) noexcept;

} // namespace aclsan

#endif
