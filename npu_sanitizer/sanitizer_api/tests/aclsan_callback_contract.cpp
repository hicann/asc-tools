/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_cbdata_common.h"
#include "aclsan/aclsan_cbdata_device.h"
#include "aclsan/aclsan_cbdata_resource.h"
#include "aclsan/aclsan_cbdata_synchronize.h"
#include "aclsan/aclsan_callback.h"
#include "aclsan/aclsan_api.h"

#include <cstdint>
#include <type_traits>

template <typename T, typename = void>
struct HasInstrType : std::false_type {};

template <typename T>
struct HasInstrType<T, std::void_t<decltype(&T::instrType)>> : std::true_type {};

static_assert(std::is_same_v<AclsanStatus, std::uint32_t>);
static_assert(std::is_standard_layout_v<AclsanCallbackCommonData>);
static_assert(std::is_standard_layout_v<AclsanResourceData>);
static_assert(std::is_standard_layout_v<AclsanSynchronizeData>);
static_assert(std::is_standard_layout_v<AclsanDeviceMemoryAccessData>);
static_assert(std::is_standard_layout_v<AclsanDeviceSyncData>);
static_assert(ACLSAN_DEVICE_PIPE_INVALID == 0);
static_assert(ACLSAN_DEVICE_PIPE_SCALAR == 1);
static_assert(ACLSAN_DEVICE_PIPE_MTE2 == 3);
static_assert(ACLSAN_DEVICE_PIPE_MTE3 == 4);
static_assert(ACLSAN_DEVICE_PIPE_FIXPIPE == 5);
static_assert(!HasInstrType<AclsanDeviceSyncData>::value);
static_assert(sizeof(AclsanDeviceSyncData) == 72);
static_assert(
    std::is_same_v<decltype(&aclsanSubscribe), AclsanStatus (*)(AclsanSubscriberHandle*, AclsanCallbackFunc, void*)>);
static_assert(std::is_same_v<decltype(&aclsanUnsubscribe), AclsanStatus (*)(AclsanSubscriberHandle)>);
static_assert(std::is_same_v<
              decltype(&aclsanEnableCallback),
              AclsanStatus (*)(uint32_t, AclsanSubscriberHandle, AclsanCallbackDomain, AclsanCallbackId)>);
static_assert(std::is_same_v<
              decltype(&aclsanEnableDomain), AclsanStatus (*)(AclsanSubscriberHandle, AclsanCallbackDomain, uint32_t)>);
static_assert(std::is_same_v<
              decltype(&aclsanGetCallbackState),
              AclsanStatus (*)(AclsanSubscriberHandle, AclsanCallbackDomain, AclsanCallbackId, uint32_t*)>);
static_assert(std::is_same_v<decltype(&aclsanGetDeviceCallStack), AclsanStatus (*)(uint64_t, AclsanDeviceCallStack*)>);

int main() { return ACLSAN_STATUS_SUCCESS; }
