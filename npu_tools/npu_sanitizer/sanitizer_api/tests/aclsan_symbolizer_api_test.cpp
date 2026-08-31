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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

extern "C" void aclsanTestRecordDeviceBinarySource(const void* binary, const void* image, size_t imageBytes);
extern "C" void aclsanTestResetTraceRuntimeState();

namespace fs = std::filesystem;

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

    const fs::path work = fs::temp_directory_path() / "aclsan-symbolizer-api-test";
    const fs::path symbolizer = work / "fake-symbolizer";
    fs::remove_all(work);
    fs::create_directories(work);
    {
        std::ofstream script(symbolizer);
        script << "#!/bin/sh\n"
               << "printf 'CopyIn\\n/src/kernel.asc:46:5\\nAddKernel\\n/src/kernel.asc:58:1\\n'\n";
    }
    fs::permissions(
        symbolizer, fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec, fs::perm_options::replace);
    assert(setenv("ACLSAN_SYMBOLIZER", symbolizer.c_str(), 1) == 0);
    assert(setenv("NPU_CHECK_DBI_WORK_DIR", work.c_str(), 1) == 0);

    const std::vector<uint8_t> image{0x7f, 'E', 'L', 'F', 1, 2, 3, 4};
    aclsanTestRecordDeviceBinarySource(reinterpret_cast<const void*>(0x51), image.data(), image.size());
    assert(aclsanGetDeviceCallStack(0x170, result.get()) == ACLSAN_STATUS_SUCCESS);
    assert(result->binaryId != 0);
    assert(result->pc == 0x170);
    assert(result->depth == 2);
    assert(result->frames[0].line == 46);
    assert(result->frames[0].column == 5);
    assert(std::string(result->frames[0].functionName) == "CopyIn");
    assert(std::string(result->frames[0].fileName) == "/src/kernel.asc");

    aclsanTestResetTraceRuntimeState();
    assert(aclsanGetDeviceCallStack(0x170, result.get()) == ACLSAN_STATUS_ERROR_INVALID_STATE);
    fs::remove_all(work);
    unsetenv("ACLSAN_SYMBOLIZER");
    unsetenv("NPU_CHECK_DBI_WORK_DIR");
    return 0;
}
