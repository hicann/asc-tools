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
    return SendFrame(fd, frame, error) == IoStatus::OK;
}

void RunClient(const std::string& socketPath, uint64_t sessionId, const std::string& nonce, ClientResult& result)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!std::filesystem::exists(socketPath) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    const int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        result.error = "client socket failed";
        return;
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socketPath.c_str(), socketPath.size() + 1);
    if (connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        result.error = "client connect failed";
        (void)close(fd);
        return;
    }
    if (!SetSocketTimeouts(fd, 2000, result.error)) {
        (void)close(fd);
        return;
    }

    HelloPayload hello{};
    hello.pid = static_cast<uint32_t>(getpid());
    hello.uid = static_cast<uint32_t>(getuid());
    hello.nonce = nonce;
    if (!SendClientFrame(fd, MessageType::CLIENT_HELLO, sessionId, 1, EncodeHello(hello), result.error)) {
        (void)close(fd);
        return;
    }

    Frame serverHello{};
    if (ReceiveFrame(fd, serverHello, result.error) != IoStatus::OK || serverHello.type != MessageType::SERVER_HELLO ||
        serverHello.sequence != 1) {
        if (result.error.empty()) {
            result.error = "invalid server hello";
        }
        (void)close(fd);
        return;
    }

    ToolConfig config{};
    config.toolName = "memcheck";
    config.workDir = "/tmp";
    if (!SendClientFrame(fd, MessageType::CONFIGURE, sessionId, 2, EncodeToolConfig(config), result.error) ||
        ReceiveFrame(fd, result.ready, result.error) != IoStatus::OK ||
        ReceiveFrame(fd, result.flowError, result.error) != IoStatus::OK) {
        (void)close(fd);
        return;
    }
    (void)close(fd);
}

TEST(UdsServerTest, SendsFlowErrorSynchronouslyAfterReady)
{
    constexpr uint64_t kSessionId = 90210;
    const std::string nonce = "npu-check-test-nonce";
    const std::string socketPath = "/tmp/npu_check_uds_" + std::to_string(static_cast<uint64_t>(getpid())) + ".sock";
    ScopedEnvironmentVariable socketPathEnvironment(kSocketPathEnv, socketPath);
    ScopedEnvironmentVariable sessionEnvironment(kSessionIdEnv, std::to_string(kSessionId));
    ScopedEnvironmentVariable nonceEnvironment(kSessionNonceEnv, nonce);
    ScopedEnvironmentVariable pidEnvironment(kCliPidEnv, std::to_string(static_cast<uint64_t>(getpid())));
    ScopedEnvironmentVariable timeoutEnvironment(kHandshakeTimeoutEnv, "2000");

    ClientResult clientResult{};
    std::thread client(RunClient, socketPath, kSessionId, nonce, std::ref(clientResult));

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

} // namespace
} // namespace npu::sanitizer::ipc
