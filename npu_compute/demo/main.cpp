/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl/acl_rt.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" int aclrtInit();

namespace {

struct KernelArgs {
    std::uint8_t* value;
};

bool ParseInteger(const char* value, int* result)
{
    if (value == nullptr || result == nullptr || value[0] == '\0') {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == nullptr || end[0] != '\0' || parsed < 0 || parsed > 255) {
        return false;
    }
    *result = static_cast<int>(parsed);
    return true;
}

const char* EnvironmentValue(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? "" : value;
}

int ReleaseAndReturn(void* dev_ptr, int result)
{
    const int free_result = aclrtFree(dev_ptr);
    if (free_result != 0) {
        std::fprintf(stderr, "[demo] aclrtFree failed: %d\n", free_result);
    }
    return result == 0 ? free_result : result;
}

} // namespace

int main(int argc, char** argv)
{
    int sleep_milliseconds = 0;
    int requested_exit_code = 0;
    std::fprintf(
        stderr, "[demo] sections=%s replay=%s\n", EnvironmentValue("NPU_COMPUTE_SECTIONS"),
        EnvironmentValue("NPU_COMPUTE_REPLAY_MODE"));
    std::fprintf(stderr, "[demo] output=%s\n", EnvironmentValue("NPU_COMPUTE_OUTPUT"));
    for (int index = 1; index < argc; ++index) {
        std::fprintf(stderr, "[demo] argv[%d]=%s\n", index, argv[index]);
        if (std::strcmp(argv[index], "--sleep-ms") == 0 && index + 1 < argc) {
            if (!ParseInteger(argv[++index], &sleep_milliseconds)) {
                std::fprintf(stderr, "[demo] invalid --sleep-ms value\n");
                return 2;
            }
            std::fprintf(stderr, "[demo] argv[%d]=%s\n", index, argv[index]);
        } else if (std::strcmp(argv[index], "--exit-code") == 0 && index + 1 < argc) {
            if (!ParseInteger(argv[++index], &requested_exit_code)) {
                std::fprintf(stderr, "[demo] invalid --exit-code value\n");
                return 2;
            }
            std::fprintf(stderr, "[demo] argv[%d]=%s\n", index, argv[index]);
        }
    }

    int result = aclrtInit();
    if (result != 0) {
        std::fprintf(stderr, "[demo] aclrtInit failed: %d\n", result);
        return result;
    }

    result = aclrtSetDevice(0);
    if (result != 0) {
        std::fprintf(stderr, "[demo] aclrtSetDevice failed: %d\n", result);
        return result;
    }

    void* dev_ptr = nullptr;
    result = aclrtMalloc(&dev_ptr, 4096, ACL_MEM_MALLOC_HUGE_FIRST);
    std::fprintf(stderr, "[demo] aclrtMalloc result=%d ptr=%p\n", result, dev_ptr);
    if (result != 0) {
        return result;
    }

    result = aclrtMemset(dev_ptr, 4096, 0, 4096);
    if (result != 0) {
        std::fprintf(stderr, "[demo] aclrtMemset failed: %d\n", result);
        return ReleaseAndReturn(dev_ptr, result);
    }

    std::uint8_t initial_value = 5;
    result = aclrtMemcpy(dev_ptr, 4096, &initial_value, 1, ACL_MEMCPY_HOST_TO_DEVICE);
    if (result != 0) {
        std::fprintf(stderr, "[demo] aclrtMemcpy H2D failed: %d\n", result);
        return ReleaseAndReturn(dev_ptr, result);
    }

    KernelArgs args{static_cast<std::uint8_t*>(dev_ptr)};
    result = aclrtLaunchKernel(nullptr, 1, &args, sizeof(args), nullptr);
    if (result != 0) {
        std::fprintf(stderr, "[demo] aclrtLaunchKernel failed: %d\n", result);
        return ReleaseAndReturn(dev_ptr, result);
    }

    std::uint8_t output = 0;
    result = aclrtMemcpy(&output, sizeof(output), dev_ptr, 1, ACL_MEMCPY_DEVICE_TO_HOST);
    if (result == 0 && output != 6) {
        std::fprintf(stderr, "[demo] unexpected kernel output: %u\n", static_cast<unsigned>(output));
        result = -1;
    }
    result = ReleaseAndReturn(dev_ptr, result);
    if (result != 0) {
        return result;
    }

    if (sleep_milliseconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_milliseconds));
    }
    if (requested_exit_code != 0) {
        std::fprintf(stderr, "[demo] exiting with requested status %d\n", requested_exit_code);
        return requested_exit_code;
    }
    std::fprintf(stderr, "[demo] completed\n");
    return 0;
}
