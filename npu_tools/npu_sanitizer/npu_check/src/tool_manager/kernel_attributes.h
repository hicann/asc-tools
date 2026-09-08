/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_TOOL_MANAGER_KERNEL_ATTRIBUTES_H
#define NPU_CHECK_TOOL_MANAGER_KERNEL_ATTRIBUTES_H

#include "acl/acl_rt.h"

#include <cstdint>

namespace aclsan {

struct KernelAttributes {
    int64_t kernelType = 0;
    aclError kernelTypeStatus = ACL_ERROR_UNINITIALIZE;
    uint16_t aicRatio = 0;
    uint16_t aivRatio = 0;
    aclError kernelRatioStatus = ACL_ERROR_UNINITIALIZE;
    int64_t kernelSchedMode = 0;
    aclError kernelSchedModeStatus = ACL_ERROR_UNINITIALIZE;
};

KernelAttributes QueryKernelAttributes(aclrtFuncHandle function) noexcept;

} // namespace aclsan

#endif
