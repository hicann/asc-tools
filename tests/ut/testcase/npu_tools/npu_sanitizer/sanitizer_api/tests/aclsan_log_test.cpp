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
#include "internal/aclsan_log.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>

namespace {

AclsanStatus ValidatePointer(const void* value)
{
    ACLSAN_CHECK_NULLPTR("TestApi", value);
    return ACLSAN_STATUS_SUCCESS;
}

std::string CaptureNullPointerLog()
{
    std::FILE* capture = std::tmpfile();
    assert(capture != nullptr);
    const int savedStderr = dup(STDERR_FILENO);
    assert(savedStderr >= 0);
    assert(dup2(fileno(capture), STDERR_FILENO) >= 0);

    assert(ValidatePointer(nullptr) == ACLSAN_STATUS_ERROR_INVALID_PARAMETER);

    assert(dup2(savedStderr, STDERR_FILENO) >= 0);
    assert(close(savedStderr) == 0);
    assert(std::fseek(capture, 0, SEEK_SET) == 0);

    std::array<char, 256> buffer{};
    const size_t bytes = std::fread(buffer.data(), 1, buffer.size() - 1, capture);
    assert(std::fclose(capture) == 0);
    return std::string(buffer.data(), bytes);
}

} // namespace

int main()
{
    int value = 0;
    assert(ValidatePointer(&value) == ACLSAN_STATUS_SUCCESS);

    const std::string logs = CaptureNullPointerLog();
    assert(logs.find("[ASC_SAN][ERROR] TestApi: value is nullptr") != std::string::npos);
    return 0;
}
