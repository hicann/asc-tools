/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "npu_compute/prof_api_stub.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    if (argc != 2) {
        return 1;
    }
    unsetenv("NPU_COMPUTE_TEST_INIT_SUCCESS");
    unsetenv("NPU_COMPUTE_TEST_INITIALIZED");
    if (setenv("ACL_API_INJECTION", argv[1], 1) != 0) {
        return 1;
    }
    if (ProfApiLoadApiInjectionFromEnv() == 0) {
        std::fprintf(stderr, "first initialization unexpectedly succeeded\n");
        return 1;
    }
    if (setenv("NPU_COMPUTE_TEST_INIT_SUCCESS", "1", 1) != 0 || ProfApiLoadApiInjectionFromEnv() != 0) {
        std::fprintf(stderr, "retry initialization failed\n");
        return 1;
    }
    const char* initialized = std::getenv("NPU_COMPUTE_TEST_INITIALIZED");
    if (initialized == nullptr || initialized[0] != '1') {
        std::fprintf(stderr, "retry returned without calling acltoolInitialize\n");
        return 1;
    }
    return 0;
}
