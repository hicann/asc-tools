/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"

#include <cassert>
#include <memory>
#include <type_traits>

static_assert(std::is_same_v<decltype(&aclsanGetDeviceCallStack), AclsanStatus (*)(uint64_t, AclsanDeviceCallStack*)>);
static_assert(ACLSAN_CALL_STACK_MAX_DEPTH == 16U);
static_assert(ACLSAN_FUNCTION_NAME_MAX_BYTES == 4096U);
static_assert(ACLSAN_FILE_NAME_MAX_BYTES == 4096U);

int main()
{
    assert(aclsanGetDeviceCallStack(0x170, nullptr) == ACLSAN_STATUS_ERROR_INVALID_PARAMETER);

    auto result = std::make_unique<AclsanDeviceCallStack>();
    result->binaryId = 99;
    result->pc = 0;
    result->depth = 7;
    result->flags = ACLSAN_CALL_STACK_FLAG_TRUNCATED;
    result->frames[0].line = 42;

    assert(aclsanGetDeviceCallStack(0x170, result.get()) == ACLSAN_STATUS_ERROR_INVALID_STATE);
    assert(result->binaryId == 0);
    assert(result->pc == 0x170);
    assert(result->depth == 0);
    assert(result->flags == ACLSAN_CALL_STACK_FLAG_NONE);
    assert(result->frames[0].line == 0);
    return 0;
}
