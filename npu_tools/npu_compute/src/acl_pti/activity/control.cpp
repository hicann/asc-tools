/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "control.h"

#include "acl_pti/callback/dispatcher.h"

namespace npu_compute::aclpti::activity {
namespace {

aclptiResult Validate(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, const aclptiActivityConfig* config)
{
    const auto& dispatcher = callback::GetDispatcher();
    if (!dispatcher.IsValidSubscriber(subscriber)) {
        return ACLPTI_ERROR_INVALID_SUBSCRIBER;
    }
    if (kind != ACLPTI_ACTIVITY_KIND_FULL || (config != nullptr && config->reserved != 0)) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    return ACLPTI_SUCCESS;
}

aclptiResult SetEnabled(
    aclptiSubscribeHandle subscriber, aclptiActivityKind kind, const aclptiActivityConfig* config, bool enabled)
{
    const aclptiResult result = Validate(subscriber, kind, config);
    if (result == ACLPTI_SUCCESS) {
        callback::GetDispatcher().SetActivityEnabled(subscriber, enabled);
    }
    return result;
}

} // namespace

aclptiResult Enable(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, const aclptiActivityConfig* config)
{
    return SetEnabled(subscriber, kind, config, true);
}

aclptiResult Disable(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, const aclptiActivityConfig* config)
{
    return SetEnabled(subscriber, kind, config, false);
}

} // namespace npu_compute::aclpti::activity
