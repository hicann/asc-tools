/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "common/debug_log.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <unistd.h>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

template <typename Function>
bool CaptureStderr(Function function, std::string* output)
{
    FILE* capture = std::tmpfile();
    if (capture == nullptr) {
        return false;
    }

    const int savedStderr = dup(STDERR_FILENO);
    if (savedStderr < 0) {
        std::fclose(capture);
        return false;
    }

    bool success = std::fflush(stderr) == 0 && dup2(fileno(capture), STDERR_FILENO) >= 0;
    if (success) {
        function();
        success = std::fflush(stderr) == 0;
    }
    success = dup2(savedStderr, STDERR_FILENO) >= 0 && success;
    close(savedStderr);

    if (success) {
        std::rewind(capture);
        char buffer[256];
        std::size_t count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), capture)) != 0) {
            output->append(buffer, count);
        }
        success = std::ferror(capture) == 0;
    }

    success = std::fclose(capture) == 0 && success;
    return success;
}

} // namespace

int main()
{
    unsetenv("NPU_COMPUTE_DEBUG");
    CHECK(!npu_compute::detail::DebugEnabled());
    std::string output;
    CHECK(CaptureStderr([] { npu_compute::detail::DebugLog("debug_log_test", "disabled"); }, &output));
    CHECK(output.empty());

    CHECK(setenv("NPU_COMPUTE_DEBUG", "0", 1) == 0);
    CHECK(!npu_compute::detail::DebugEnabled());
    output.clear();
    CHECK(CaptureStderr([] { npu_compute::detail::DebugLog("debug_log_test", "disabled"); }, &output));
    CHECK(output.empty());

    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    CHECK(npu_compute::detail::DebugEnabled());
    output.clear();
    CHECK(CaptureStderr([] { npu_compute::detail::DebugLog("debug_log_test", "enabled value=%d", 1); }, &output));
    CHECK(output == "[debug_log_test] enabled value=1\n");

    output.clear();
    CHECK(CaptureStderr([] { npu_compute::detail::DebugLog(nullptr, "value=%d", 2); }, &output));
    CHECK(output == "[unknown] value=2\n");

    output.clear();
    CHECK(CaptureStderr([] { npu_compute::detail::DebugLog("debug_log_test", nullptr); }, &output));
    CHECK(output == "[debug_log_test] \n");

    return 0;
}
