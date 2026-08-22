/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_dispatch_cb.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"

#include <cstdint>

namespace aclsan {

bool AclsanCallbackDispatcher::IsSupportedResourceCallback(AclsanCallbackId callbackId) noexcept
{
    return callbackId == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC || callbackId == ACLSAN_CBID_RESOURCE_MEMORY_FREE;
}

void AclsanCallbackDispatcher::Dispatch(
    AclsanCallbackDomain domain, AclsanCallbackId callbackId, const void* callbackData, const char* eventName) noexcept
{
    if (callbackData == nullptr) {
        ASC_SAN_ERROR(
            "acl_san dispatch rejected null callback data: event=%s domain=%u id=%u",
            eventName == nullptr ? "unknown" : eventName, static_cast<uint32_t>(domain),
            static_cast<uint32_t>(callbackId));
        return;
    }
    if (!IsCallbackEnabled(domain, callbackId)) {
        return;
    }
    if (!InvokeCallback(domain, callbackId, callbackData)) {
        ASC_SAN_ERROR(
            "acl_san dispatch failed: event=%s domain=%u id=%u", eventName == nullptr ? "unknown" : eventName,
            static_cast<uint32_t>(domain), static_cast<uint32_t>(callbackId));
    }
}

void AclsanCallbackDispatcher::DispatchResource(
    AclsanCallbackId callbackId, const AclsanResourceData& callbackData) noexcept
{
    if (!IsSupportedResourceCallback(callbackId)) {
        ASC_SAN_ERROR("acl_san dispatch rejected resource callback id=%u", static_cast<uint32_t>(callbackId));
        return;
    }
    Dispatch(ACLSAN_CB_DOMAIN_RESOURCE, callbackId, &callbackData, callbackData.common.apiName);
}

void AclsanCallbackDispatcher::DispatchSynchronizeEnd(const AclsanSynchronizeData& callbackData) noexcept
{
    Dispatch(
        ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &callbackData,
        "SYNCHRONIZE_STREAM_SYNC_END");
}

void AclsanCallbackDispatcher::DispatchDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& callbackData) noexcept
{
    constexpr AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION;
    constexpr AclsanCallbackId callbackId = ACLSAN_CBID_DEVICE_MEMORY_ACCESS;
    Dispatch(domain, callbackId, &callbackData, "DEVICE_MEMORY_ACCESS");
}

void AclsanCallbackDispatcher::DispatchDeviceMemoryAccess(const DeviceMemoryAccessDataArray& callbackData) noexcept
{
    for (const AclsanDeviceMemoryAccessData& data : callbackData) {
        DispatchDeviceMemoryAccess(data);
    }
}

void AclsanCallbackDispatcher::DispatchDeviceSync(const AclsanDeviceSyncData& callbackData) noexcept
{
    Dispatch(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC, &callbackData, "DEVICE_SYNC");
}

} // namespace aclsan
