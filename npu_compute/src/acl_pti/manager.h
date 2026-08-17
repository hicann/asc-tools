/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_MANAGER_H_
#define NPU_COMPUTE_ACLPTI_MANAGER_H_

#include "aclpti/aclpti_range_profiler.h"
#include "aclpti/aclpti_types.h"
#include "acl_pti/profiling/range_profiler.h"
#include "acl_pti/profiling/replay_memory.h"

namespace npu_compute::aclpti {

class Manager {
public:
    aclptiResult Initialize();
    aclptiResult EnsureCallbackDomainsRegistered();
    aclptiResult SetSections(const aclptiRangeProfilerSetConfigParams* params);

private:
    bool initialized_ = false;
    bool callbacksRegistered_ = false;
    profiling::ReplayMemory replayMemory_;
    profiling::RangeProfiler rangeProfiler_;
};

Manager& GetManager();

} // namespace npu_compute::aclpti

#endif // NPU_COMPUTE_ACLPTI_MANAGER_H_
