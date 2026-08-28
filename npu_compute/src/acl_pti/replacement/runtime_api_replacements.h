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
 * @file runtime_api_replacements.h
 * @brief Declares registration of ACL PTI runtime API replacements.
 */
#ifndef NPU_COMPUTE_ACLPTI_REPLACEMENT_RUNTIME_API_REPLACEMENTS_H_
#define NPU_COMPUTE_ACLPTI_REPLACEMENT_RUNTIME_API_REPLACEMENTS_H_

namespace npu_compute::aclpti::replacement {

/// Registers every supported runtime API replacement with the injection hook.
bool RegisterRuntimeApiReplacements();

} // namespace npu_compute::aclpti::replacement

#endif // NPU_COMPUTE_ACLPTI_REPLACEMENT_RUNTIME_API_REPLACEMENTS_H_
