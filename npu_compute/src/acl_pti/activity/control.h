/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
/**
 * @file control.h
 * @brief Validates subscriber activity requests and updates activity collection state.
 */
#ifndef NPU_COMPUTE_ACLPTI_ACTIVITY_CONTROL_H_
#define NPU_COMPUTE_ACLPTI_ACTIVITY_CONTROL_H_

#include "aclpti/aclpti_activity.h"
#include "aclpti/aclpti_types.h"

namespace npu_compute::aclpti::activity {

/// Validates and enables activity collection for a subscriber.
aclptiResult Enable(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, const aclptiActivityConfig* config);

/// Validates and disables activity collection for a subscriber.
aclptiResult Disable(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, const aclptiActivityConfig* config);

} // namespace npu_compute::aclpti::activity

#endif // NPU_COMPUTE_ACLPTI_ACTIVITY_CONTROL_H_
