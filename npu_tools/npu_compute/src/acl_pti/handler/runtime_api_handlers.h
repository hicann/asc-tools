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
 * @file runtime_api_handlers.h
 * @brief Declares registration of ACL PTI runtime API handlers.
 */
#ifndef NPU_TOOLS_NPU_COMPUTE_SRC_ACL_PTI_HANDLER_RUNTIME_API_HANDLERS_H
#define NPU_TOOLS_NPU_COMPUTE_SRC_ACL_PTI_HANDLER_RUNTIME_API_HANDLERS_H

namespace aclpti::handler {

/// Registers every supported runtime API handler with the injection hook.
bool RegisterRuntimeApiHandlers();

} // namespace aclpti::handler

#endif // NPU_TOOLS_NPU_COMPUTE_SRC_ACL_PTI_HANDLER_RUNTIME_API_HANDLERS_H
