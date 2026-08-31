/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_RUNTIME_HOOK_H
#define ACLSAN_RUNTIME_HOOK_H

#include "aclsan/aclsan_api.h"
#include "injection/injection_hook.h"

#include <set>

namespace aclsan {

void ApplyRuntimeHooks(const std::set<aclrtApiId>& requiredHooks) noexcept;

} // namespace aclsan

#endif
