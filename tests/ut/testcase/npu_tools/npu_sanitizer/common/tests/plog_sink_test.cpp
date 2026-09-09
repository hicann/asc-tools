// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "plog_sink.h"
#include "plog_test_library.h"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <thread>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

int32_t g_checkCalls = 0;
int32_t g_recordCalls = 0;
int32_t g_checkModuleId = 0;
int32_t g_recordModuleId = 0;
int32_t g_level = -1;
bool g_enabled = true;
char g_message[2048] = {};

int32_t CheckLogLevelStub(int32_t moduleId, int32_t level)
{
    ++g_checkCalls;
    g_checkModuleId = moduleId;
    g_level = level;
    return g_enabled ? 1 : 0;
}

void DlogRecordStub(int32_t moduleId, int32_t level, const char* format, ...)
{
    ++g_recordCalls;
    g_recordModuleId = moduleId;
    g_level = level;
    va_list arguments;
    va_start(arguments, format);
    const char* message = va_arg(arguments, const char*);
    va_end(arguments);
    assert(std::strcmp(format, "%s") == 0);
    assert(message != nullptr);
    (void)std::strncpy(g_message, message, sizeof(g_message) - 1U);
}

void ResetCapture()
{
    g_checkCalls = 0;
    g_recordCalls = 0;
    g_checkModuleId = 0;
    g_recordModuleId = 0;
    g_level = -1;
    std::memset(g_message, 0, sizeof(g_message));
}

} // namespace

int main()
{
    using namespace npucheck;

    // The test library starts with logging disabled.
    plog_test::ResetApi();
    WritePlog(PlogLevel::kError, "must not throw", "source.cpp", 7U, "InstallHook");

    ResetCapture();
    plog_test::SetApi({CheckLogLevelStub, DlogRecordStub});
    WritePlog(PlogLevel::kWarning, "hook install failed", "source.cpp", 7U, "InstallHook");

    assert(g_checkCalls == 1);
    assert(g_recordCalls == 1);
    assert(g_checkModuleId == static_cast<int32_t>(ASCENDCKERNEL));
    assert(g_recordModuleId == (static_cast<int32_t>(ASCENDCKERNEL) | static_cast<int32_t>(DEBUG_LOG_MASK)));
    assert(g_level == DLOG_WARN);
    assert(
        std::string(g_message) ==
        "[source.cpp:7] " + std::to_string(syscall(SYS_gettid)) + " InstallHook: hook install failed");

    // Default source information must describe the caller, not the sink.
    for (auto level : {PlogLevel::kDebug, PlogLevel::kInfo, PlogLevel::kWarning, PlogLevel::kError}) {
        ResetCapture();
        const auto expectedLine = __LINE__ + 1;
        WritePlog(level, "record without a file logger");
        assert(g_recordCalls == 1);
        assert(
            std::string(g_message) == "[plog_sink_test.cpp:" + std::to_string(expectedLine) + "] " +
                                          std::to_string(syscall(SYS_gettid)) + " main: record without a file logger");
    }
    // A worker must emit its own Linux TID, rather than the process ID.
    std::thread worker([] {
        WritePlog(PlogLevel::kInfo, "worker", "thread.cpp", 3, "Worker");
        assert(std::string(g_message) == "[thread.cpp:3] " + std::to_string(syscall(SYS_gettid)) + " Worker: worker");
        assert(syscall(SYS_gettid) != getpid());
    });
    worker.join();
    ResetCapture();
    g_enabled = false;
    WritePlog(PlogLevel::kDebug, "filtered by CANN");
    assert(g_checkCalls == 1);
    assert(g_recordCalls == 0);
    g_enabled = true;

    ResetCapture();
    WritePlog(PlogLevel::kError, std::string(4096, 'x') + "\ncompiler-error-tail");
    assert(g_recordCalls > 1);
    assert(std::strstr(g_message, "compiler-error-tail") != nullptr);

    plog_test::ResetApi();
    return 0;
}
