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

#include <atomic>
#include <thread>
#include <vector>

int main()
{
    const char input[] = "\x7f"
                         "ELF-test";
    std::vector<char> output;
    // A failed linker invocation must not invalidate the extracted asset cache.
    if (aclpti::profiling::InstrumentKernelEnd(input, sizeof(input), output)) {
        return 1;
    }
    std::atomic<bool> success{true};
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&] {
            std::vector<char> patched;
            if (!aclpti::profiling::InstrumentKernelEnd(input, sizeof(input), patched) ||
                patched != std::vector<char>(input, input + sizeof(input))) {
                success = false;
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    if (!aclpti::profiling::InstrumentKernelEnd(input, sizeof(input), output)) {
        return 2;
    }
    return success ? 0 : 3;
}
