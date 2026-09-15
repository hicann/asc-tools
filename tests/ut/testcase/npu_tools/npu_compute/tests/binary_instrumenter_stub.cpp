/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl_pti/profiling/binary_instrumenter.h"

namespace aclpti::profiling {
// Stub only the binary instrumentation boundary; binary ownership, lookup and replay
// execute the production implementation against the Runtime stub.
bool InstrumentKernelEnd(const void* data, std::size_t size, std::vector<char>& output)
{
    output.assign(static_cast<const char*>(data), static_cast<const char*>(data) + size);
    return true;
}
} // namespace aclpti::profiling
