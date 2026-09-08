/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tool_manager/kernel_attributes.h"

#include "injection/injection_hook.h"

namespace aclsan {

KernelAttributes QueryKernelAttributes(aclrtFuncHandle function) noexcept
{
    using FunctionAttributeGetter = aclError (*)(aclrtFuncHandle, aclrtFuncAttribute, int64_t*);
    const auto getFunctionAttribute =
        reinterpret_cast<FunctionAttributeGetter>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtGetFunctionAttribute));
    KernelAttributes attributes;
    if (getFunctionAttribute == nullptr) {
        return attributes;
    }

    int64_t kernelType = 0;
    attributes.kernelTypeStatus = getFunctionAttribute(function, ACL_FUNC_ATTR_KERNEL_TYPE, &kernelType);
    if (attributes.kernelTypeStatus == ACL_SUCCESS) {
        attributes.kernelType = kernelType;
    }

    int64_t kernelRatio = 0;
    attributes.kernelRatioStatus = getFunctionAttribute(function, ACL_FUNC_ATTR_KERNEL_RATIO, &kernelRatio);
    if (attributes.kernelRatioStatus == ACL_SUCCESS) {
        const uint32_t encodedRatio = static_cast<uint32_t>(kernelRatio);
        attributes.aicRatio = static_cast<uint16_t>(encodedRatio >> 16U);
        attributes.aivRatio = static_cast<uint16_t>(encodedRatio);
    }

    int64_t kernelSchedMode = 0;
    attributes.kernelSchedModeStatus =
        getFunctionAttribute(function, ACL_FUNC_ATTR_KERNEL_SCHED_MODE, &kernelSchedMode);
    if (attributes.kernelSchedModeStatus == ACL_SUCCESS) {
        attributes.kernelSchedMode = kernelSchedMode;
    }
    return attributes;
}

} // namespace aclsan
