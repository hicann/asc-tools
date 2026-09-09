// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"
#include "options.h"
#include "uds_client.h"

#include <gtest/gtest.h>
#include <cstdlib>
#include <set>
#include <thread>
#include <unistd.h>

namespace {
AclsanCallbackFunc callback = nullptr;
void* userdata = nullptr;
std::set<std::pair<AclsanCallbackDomain, AclsanCallbackId>> enabled;
size_t enableCalls = 0;

void Emit(AclsanCallbackDomain domain, AclsanCallbackId id, const void* data)
{
    if (enabled.count({domain, id}) != 0)
        callback(userdata, domain, id, data);
}

struct Session {
    std::string text;
    std::string error;
    uint16_t flags = 0;
    int initialized = -1;
};

Session RunSession(std::vector<std::string> args, bool memoryError, bool syncError, bool failSync = false)
{
    enabled.clear();
    enableCalls = 0;
    std::vector<char*> argv;
    for (auto& arg : args)
        argv.push_back(arg.data());
    npucheck::Options options;
    Session result;
    if (!npucheck::ParseOptions(argv.size(), argv.data(), options, result.error))
        return result;
    const auto name = "@npu-check-manager-" + std::to_string(getpid());
    constexpr uint64_t sessionId = 93091;
    setenv(npucheck::ipc::kUdsNameEnv, name.c_str(), 1);
    setenv(npucheck::ipc::kSessionIdEnv, "93091", 1);
    setenv(npucheck::ipc::kCliPidEnv, std::to_string(getpid()).c_str(), 1);
    setenv(npucheck::ipc::kHandshakeTimeoutEnv, "5000", 1);
    npucheck::ipc::ConfigureRequest config;
    config.tools = options.tools;
    std::thread client([&] {
        npucheck::ipc::UdsClient transport;
        if (!transport.ConnectAndConfigure(
                name, sessionId, getpid(), npucheck::ipc::DeadlineAfterMs(5000), config, result.error))
            return;
        while (true) {
            npucheck::ipc::Frame frame;
            if (transport.Receive(frame, npucheck::ipc::DeadlineAfterMs(5000), result.error) !=
                npucheck::ipc::IoStatus::OK)
                return;
            if (frame.type != npucheck::ipc::MessageType::RESULT) {
                result.error = "expected Result";
                return;
            }
            result.text.append(frame.payload.begin(), frame.payload.end());
            result.flags = frame.flags;
            if ((frame.flags & npucheck::ipc::kFlagMore) == 0)
                return;
        }
    });
    npucheck::ToolManager manager;
    result.initialized = manager.Initialize();
    if (result.initialized == 0) {
        AclsanResourceData allocation{};
        allocation.common.version = ACLSAN_API_VERSION;
        allocation.common.size = sizeof(allocation);
        allocation.ptr = reinterpret_cast<void*>(0x1000);
        allocation.bytes = 32;
        allocation.memorySpace = ACLSAN_MEMORY_SPACE_DEVICE;
        allocation.resourceId = 1;
        Emit(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &allocation);
        AclsanDeviceMemoryAccessData memory{};
        memory.header.version = ACLSAN_API_VERSION;
        memory.header.size = sizeof(memory);
        memory.header.pc = 0x170;
        memory.address = memoryError ? 0x1018 : 0x1000;
        memory.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_GM;
        memory.accessMode = ACLSAN_DEVICE_MEMORY_ACCESS_READ;
        memory.accessCount = 1;
        memory.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
        memory.layout.range.bytes = 16;
        Emit(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS, &memory);
        if (syncError) {
            AclsanDeviceSyncData sync{};
            sync.header.version = ACLSAN_API_VERSION;
            sync.header.size = sizeof(sync);
            sync.header.pc = 0x180;
            sync.syncKind = ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG;
            sync.action = ACLSAN_DEVICE_SYNC_ACTION_WAIT;
            sync.scope = ACLSAN_DEVICE_SYNC_SCOPE_BLOCK;
            sync.srcPipe = ACLSAN_DEVICE_PIPE_VECTOR;
            sync.dstPipe = ACLSAN_DEVICE_PIPE_MTE2;
            sync.objectId = 7;
            Emit(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC, &sync);
        }
        AclsanSynchronizeData sync{};
        sync.common.version = ACLSAN_API_VERSION;
        sync.common.size = sizeof(sync);
        sync.common.result = failSync ? 1 : 0;
        Emit(ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &sync);
        manager.Finalize();
    }
    client.join();
    unsetenv(npucheck::ipc::kUdsNameEnv);
    unsetenv(npucheck::ipc::kSessionIdEnv);
    unsetenv(npucheck::ipc::kCliPidEnv);
    unsetenv(npucheck::ipc::kHandshakeTimeoutEnv);
    return result;
}
} // namespace

// Mock only the public sanitizer boundary; the CLI parser, UDS exchange,
// ToolManager, checkers and reporter all run their production implementations.
extern "C" AclsanStatus aclsanSubscribe(AclsanSubscriberHandle* handle, AclsanCallbackFunc cb, void* data)
{
    callback = cb;
    userdata = data;
    *handle = reinterpret_cast<AclsanSubscriberHandle>(0x1);
    return ACLSAN_STATUS_SUCCESS;
}
extern "C" AclsanStatus aclsanUnsubscribe(AclsanSubscriberHandle)
{
    callback = nullptr;
    return ACLSAN_STATUS_SUCCESS;
}
extern "C" AclsanStatus aclsanEnableCallback(
    uint32_t, AclsanSubscriberHandle, AclsanCallbackDomain domain, AclsanCallbackId id)
{
    ++enableCalls;
    enabled.insert({domain, id});
    return ACLSAN_STATUS_SUCCESS;
}
extern "C" AclsanStatus aclsanGetDeviceCallStack(uint64_t, AclsanDeviceCallStack*)
{
    return ACLSAN_STATUS_ERROR_INVALID_STATE;
}

TEST(ToolManagerTest, CombinedToolsReportBothAndSubscribeSharedCallbackOnce)
{
    const auto result = RunSession(
        {"npu-check", "--tools", "synccheck", "--tools", "memcheck", "--tools", "memcheck", "/bin/true"}, true, true);
    ASSERT_EQ(result.initialized, 0);
    ASSERT_TRUE(result.error.empty()) << result.error;
    EXPECT_EQ(enableCalls, 6U);
    EXPECT_EQ(enabled.size(), 6U);
    EXPECT_NE(result.text.find("tool=memcheck"), std::string::npos);
    EXPECT_NE(result.text.find("tool=synccheck"), std::string::npos);
    EXPECT_NE(result.text.find("status=complete"), std::string::npos);
    EXPECT_NE(result.text.find("Line information unavailable."), std::string::npos);
    EXPECT_EQ(result.text.find("api_status_"), std::string::npos);
    EXPECT_EQ(result.text.find("[CALL-STACK] pc="), std::string::npos);
    EXPECT_NE(result.flags & npucheck::ipc::kFlagHasErrors, 0U);
}

TEST(ToolManagerTest, ErrorFlagAggregatesEitherTool)
{
    for (auto errors : {std::pair{false, false}, std::pair{true, false}, std::pair{false, true}}) {
        const auto result = RunSession(
            {"npu-check", "--tools", "memcheck", "--tools", "synccheck", "/bin/true"}, errors.first, errors.second);
        ASSERT_EQ(result.initialized, 0);
        ASSERT_TRUE(result.error.empty()) << result.error;
        EXPECT_EQ((result.flags & npucheck::ipc::kFlagHasErrors) != 0, errors.first || errors.second);
        EXPECT_NE(result.text.find("status=complete"), std::string::npos);
    }
}

TEST(ToolManagerTest, SingleCheckerAndFailedSyncRetainIndependentState)
{
    const auto single = RunSession({"npu-check", "--tools", "synccheck", "/bin/true"}, true, true);
    ASSERT_TRUE(single.error.empty()) << single.error;
    EXPECT_EQ(enableCalls, 3U);
    EXPECT_EQ(single.text.find("tool=memcheck"), std::string::npos);
    EXPECT_NE(single.text.find("tool=synccheck"), std::string::npos);
    const auto incomplete =
        RunSession({"npu-check", "--tools", "memcheck", "--tools", "synccheck", "/bin/true"}, true, true, true);
    ASSERT_TRUE(incomplete.error.empty()) << incomplete.error;
    EXPECT_NE(incomplete.text.find("status=incomplete"), std::string::npos);
    EXPECT_NE(incomplete.text.find("pending_device_operations=1"), std::string::npos);
    EXPECT_NE(incomplete.flags & npucheck::ipc::kFlagHasErrors, 0U);
}
