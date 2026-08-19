/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"

#include <cstdint>

namespace aclsan {

void DispatchDeviceMemoryAccessData(const DeviceMemoryAccessDataArray& callbackData) noexcept
{
    constexpr AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION;
    constexpr AclsanCallbackId callbackId = ACLSAN_CBID_DEVICE_MEMORY_ACCESS;
    if (!IsCallbackEnabled(domain, callbackId)) {
        return;
    }
    for (const AclsanDeviceMemoryAccessData& data : callbackData) {
        if (!InvokeCallback(domain, callbackId, &data)) {
            ASC_SAN_ERROR("acl_san hook: DEVICE_MEMORY_ACCESS callback failed");
        }
    }
}

void DispatchDeviceSyncData(const AclsanDeviceSyncData& callbackData) noexcept
{
    constexpr AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION;
    constexpr AclsanCallbackId callbackId = ACLSAN_CBID_DEVICE_SYNC;
    if (!IsCallbackEnabled(domain, callbackId)) {
        return;
    }
    if (!InvokeCallback(domain, callbackId, &callbackData)) {
        ASC_SAN_ERROR("acl_san hook: DEVICE_SYNC callback failed");
    }
}

void DispatchSynchronizeEnd(void* stream, int result) noexcept
{
    constexpr AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_SYNCHRONIZE;
    constexpr AclsanCallbackId callbackId = ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END;
    if (!IsCallbackEnabled(domain, callbackId)) {
        return;
    }
    const AclsanSynchronizeData callbackData{
        {ACLSAN_API_VERSION, static_cast<uint32_t>(sizeof(AclsanSynchronizeData)), "aclrtSynchronizeStream", result, 0,
         0},
        stream};
    if (!InvokeCallback(domain, callbackId, &callbackData)) {
        ASC_SAN_ERROR("acl_san hook: STREAM_SYNC_END callback failed");
    }
}

} // namespace aclsan
