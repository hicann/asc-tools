// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "ipc/uds_server.h"

#include "uds_transport.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace npu::sanitizer::ipc {
namespace {

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(const char* name, const std::string& value) : name_(name)
    {
        if (const char* current = std::getenv(name); current != nullptr) {
            original_ = current;
        }
        (void)setenv(name, value.c_str(), 1);
    }

    ~ScopedEnvironmentVariable()
    {
        if (original_.has_value()) {
            (void)setenv(name_.c_str(), original_->c_str(), 1);
        } else {
            (void)unsetenv(name_.c_str());
        }
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;

private:
    std::string name_;
    std::optional<std::string> original_;
};

struct ClientResult {
    Frame ready;
    Frame flowError;
    std::string error;
};

bool SendClientFrame(
    int fd, MessageType type, uint64_t sessionId, uint64_t sequence, std::vector<uint8_t> payload, std::string& error)
{
    Frame frame{};
    frame.type = type;
    frame.sessionId = sessionId;
    frame.sequence = sequence;
    frame.payload = std::move(payload);
    return SendFrame(fd, frame, DeadlineAfterMs(2000), error) == IoStatus::OK;
}

// sendMustIgnoreFrame 为真时，在 CLIENT_HELLO 与 CONFIGURE 之间插入一帧 must-ignore
// 类型的消息，用来验证 Server 跳过它之后仍能正确接上后续帧。
//
// 这里刻意不给默认实参：std::thread 会把函数退化成函数指针，而默认实参属于声明、不属于
// 函数类型，因此通过指针调用时不会被填充，少传一个参数是编译错误而不是"取默认值"。
void RunClient(const std::string& udsName, uint64_t sessionId, ClientResult& result, bool sendMustIgnoreFrame)
{
    sockaddr_un address{};
    socklen_t addrLen = 0;
    if (!BuildAbstractAddress(udsName, address, addrLen, result.error)) {
        return;
    }

    // 抽象地址不落文件系统，没法用"文件是否存在"来等 Server 就绪，只能重试 connect。
    // Server 还没 bind 或还没 listen 时都表现为 ECONNREFUSED。
    const DeadlineMs deadline = DeadlineAfterMs(2000);
    int fd = -1;
    while (true) {
        fd = CreateSeqpacketSocket(result.error);
        if (fd < 0) {
            return;
        }
        if (connect(fd, reinterpret_cast<const sockaddr*>(&address), addrLen) == 0) {
            break;
        }
        (void)close(fd);
        fd = -1;
        if (MonotonicNowMs() >= deadline) {
            result.error = "client connect timed out";
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // 发送方向的序号严格自增，被跳过的帧同样占一个序号。
    uint64_t sequence = 1;

    HelloPayload hello{};
    hello.pid = static_cast<uint32_t>(getpid());
    hello.uid = static_cast<uint32_t>(getuid());
    if (!SendClientFrame(fd, MessageType::CLIENT_HELLO, sessionId, sequence++, EncodeHello(hello), result.error)) {
        (void)close(fd);
        return;
    }

    Frame serverHello{};
    if (ReceiveFrame(fd, serverHello, deadline, result.error) != IoStatus::OK ||
        serverHello.type != MessageType::SERVER_HELLO || serverHello.sequence != 1) {
        if (result.error.empty()) {
            result.error = "invalid server hello";
        }
        (void)close(fd);
        return;
    }

    if (sendMustIgnoreFrame &&
        !SendClientFrame(
            fd, MessageType::DIAGNOSTIC_STREAM, sessionId, sequence++, EncodeText("ignore me"), result.error)) {
        (void)close(fd);
        return;
    }

    ToolConfig config{};
    config.toolName = "memcheck";
    config.workDir = "/tmp";
    if (!SendClientFrame(fd, MessageType::CONFIGURE, sessionId, sequence++, EncodeToolConfig(config), result.error) ||
        ReceiveFrame(fd, result.ready, deadline, result.error) != IoStatus::OK ||
        ReceiveFrame(fd, result.flowError, deadline, result.error) != IoStatus::OK) {
        (void)close(fd);
        return;
    }
    (void)close(fd);
}

TEST(UdsServerTest, SendsFlowErrorSynchronouslyAfterReady)
{
    constexpr uint64_t kSessionId = 90210;
    // '@' 前缀表示抽象命名空间；带上 pid 避免并发跑用例时撞车。
    const std::string udsName = "@npu_check_uds_test_" + std::to_string(static_cast<uint64_t>(getpid()));
    ScopedEnvironmentVariable udsNameEnvironment(kUdsNameEnv, udsName);
    ScopedEnvironmentVariable sessionEnvironment(kSessionIdEnv, std::to_string(kSessionId));
    ScopedEnvironmentVariable pidEnvironment(kCliPidEnv, std::to_string(static_cast<uint64_t>(getpid())));
    ScopedEnvironmentVariable timeoutEnvironment(kHandshakeTimeoutEnv, "2000");

    ClientResult clientResult{};
    std::thread client(RunClient, udsName, kSessionId, std::ref(clientResult), false);

    UdsServer server;
    ToolConfig config{};
    std::string serverError;
    const bool handshakeSucceeded = server.StartAndHandshake(config, serverError);
    const bool readySucceeded = handshakeSucceeded && server.SendReady("tool=memcheck", serverError);
    if (readySucceeded) {
        server.SendFlowError("callback processing failed");
    }
    client.join();
    server.Shutdown({}, {});

    ASSERT_TRUE(handshakeSucceeded) << serverError;
    ASSERT_TRUE(readySucceeded) << serverError;
    ASSERT_TRUE(clientResult.error.empty()) << clientResult.error;
    EXPECT_EQ(config.toolName, "memcheck");
    EXPECT_EQ(clientResult.ready.type, MessageType::READY);
    EXPECT_EQ(clientResult.ready.sequence, 2U);
    EXPECT_EQ(clientResult.flowError.type, MessageType::ERROR);
    EXPECT_EQ(clientResult.flowError.sequence, 3U);
    std::string flowError;
    ASSERT_TRUE(DecodeText(clientResult.flowError.payload, flowError, clientResult.error));
    EXPECT_EQ(flowError, "callback processing failed");
}

// 回归测试：must-ignore 帧必须被跳过，且不能打乱后续帧的序号校验。
//
// 曾经的写法把"跳过 must-ignore"放在序号校验之前，跳过一帧就少消耗一个序号，导致紧随
// 其后的 CONFIGURE 被误判为 non-contiguous、握手直接失败。这里在 CLIENT_HELLO 与
// CONFIGURE 之间插一帧 DIAGNOSTIC_STREAM，正是当年会踩中的场景。
TEST(UdsServerTest, SkipsMustIgnoreFramesWithoutBreakingSequence)
{
    constexpr uint64_t kSessionId = 90211;
    const std::string udsName = "@npu_check_uds_skip_test_" + std::to_string(static_cast<uint64_t>(getpid()));
    ScopedEnvironmentVariable udsNameEnvironment(kUdsNameEnv, udsName);
    ScopedEnvironmentVariable sessionEnvironment(kSessionIdEnv, std::to_string(kSessionId));
    ScopedEnvironmentVariable pidEnvironment(kCliPidEnv, std::to_string(static_cast<uint64_t>(getpid())));
    ScopedEnvironmentVariable timeoutEnvironment(kHandshakeTimeoutEnv, "2000");

    ClientResult clientResult{};
    std::thread client(RunClient, udsName, kSessionId, std::ref(clientResult), true);

    UdsServer server;
    ToolConfig config{};
    std::string serverError;
    const bool handshakeSucceeded = server.StartAndHandshake(config, serverError);
    const bool readySucceeded = handshakeSucceeded && server.SendReady("tool=memcheck", serverError);
    if (readySucceeded) {
        server.SendFlowError("callback processing failed");
    }
    client.join();
    server.Shutdown({}, {});

    // 跳过那一帧之后，CONFIGURE 仍应被正常接收并解出，握手照常完成。
    ASSERT_TRUE(handshakeSucceeded) << serverError;
    ASSERT_TRUE(readySucceeded) << serverError;
    ASSERT_TRUE(clientResult.error.empty()) << clientResult.error;
    EXPECT_EQ(config.toolName, "memcheck");
    EXPECT_EQ(clientResult.ready.type, MessageType::READY);
}

} // namespace
} // namespace npu::sanitizer::ipc
