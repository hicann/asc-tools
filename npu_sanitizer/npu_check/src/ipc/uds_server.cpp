// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "ipc/uds_server.h"

#include "uds_transport.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace npu::sanitizer::ipc {
namespace {

std::optional<uint64_t> ParseUnsigned(const char* text, uint64_t maximum)
{
    if (text == nullptr || text[0] == '\0') {
        return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > maximum) {
        return std::nullopt;
    }
    return static_cast<uint64_t>(parsed);
}

std::string ErrnoMessage(const char* operation) { return std::string(operation) + ": " + std::strerror(errno); }

} // namespace

UdsServer::~UdsServer() { Shutdown({}, {}); }

bool UdsServer::LoadEnvironment(std::string& error)
{
    const char* path = std::getenv(kSocketPathEnv);
    const char* session = std::getenv(kSessionIdEnv);
    const char* nonce = std::getenv(kSessionNonceEnv);
    const char* cliPid = std::getenv(kCliPidEnv);
    if (path == nullptr || path[0] == '\0' || nonce == nullptr || nonce[0] == '\0') {
        error = "missing UDS path or session nonce environment";
        return false;
    }
    const auto parsedSession = ParseUnsigned(session, std::numeric_limits<uint64_t>::max());
    const auto parsedPid = ParseUnsigned(cliPid, std::numeric_limits<uint32_t>::max());
    if (!parsedSession || *parsedSession == 0 || !parsedPid || *parsedPid == 0) {
        error = "invalid UDS session or CLI PID environment";
        return false;
    }
    socketPath_ = path;
    nonce_ = nonce;
    sessionId_ = *parsedSession;
    expectedCliPid_ = static_cast<uint32_t>(*parsedPid);

    const char* timeout = std::getenv(kHandshakeTimeoutEnv);
    if (timeout != nullptr) {
        const auto parsedTimeout = ParseUnsigned(timeout, 120000);
        if (!parsedTimeout || *parsedTimeout < 100) {
            error = "invalid handshake timeout environment";
            return false;
        }
        handshakeTimeoutMs_ = static_cast<int>(*parsedTimeout);
    }
    return true;
}

bool UdsServer::CreateListener(std::string& error)
{
    if (socketPath_.size() >= sizeof(sockaddr_un::sun_path)) {
        error = "UDS path exceeds sockaddr_un limit";
        return false;
    }
    listenFd_ = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (listenFd_ < 0) {
        error = ErrnoMessage("socket");
        return false;
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socketPath_.c_str(), socketPath_.size() + 1);
    (void)unlink(socketPath_.c_str());
    if (bind(listenFd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        error = ErrnoMessage("bind");
        return false;
    }
    if (listen(listenFd_, 1) != 0) {
        error = ErrnoMessage("listen");
        return false;
    }
    return true;
}

bool UdsServer::AcceptClient(std::string& error)
{
    if (!WaitReadable(listenFd_, handshakeTimeoutMs_, error)) {
        return false;
    }
    do {
        clientFd_ = accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
    } while (clientFd_ < 0 && errno == EINTR);
    if (clientFd_ < 0) {
        error = ErrnoMessage("accept4");
        return false;
    }
    ucred peer{};
    socklen_t peerSize = sizeof(peer);
    if (getsockopt(clientFd_, SOL_SOCKET, SO_PEERCRED, &peer, &peerSize) != 0) {
        error = ErrnoMessage("getsockopt(SO_PEERCRED)");
        return false;
    }
    if (peer.uid != getuid() || static_cast<uint32_t>(peer.pid) != expectedCliPid_) {
        error = "UDS peer credentials do not match the launcher";
        return false;
    }
    if (!SetSocketTimeouts(clientFd_, handshakeTimeoutMs_, error)) {
        return false;
    }
    return true;
}

bool UdsServer::ExchangeHandshake(ToolConfig& config, std::string& error)
{
    Frame helloFrame{};
    if (ReceiveFrame(clientFd_, helloFrame, error) != IoStatus::OK) {
        return false;
    }
    if (helloFrame.type != MessageType::CLIENT_HELLO || helloFrame.sessionId != sessionId_ ||
        helloFrame.sequence != 1) {
        error = "invalid client hello frame";
        return false;
    }
    HelloPayload hello{};
    if (!DecodeHello(helloFrame.payload, hello, error)) {
        return false;
    }
    if (hello.pid != expectedCliPid_ || hello.uid != getuid() || hello.nonce != nonce_) {
        error = "client hello identity mismatch";
        return false;
    }

    HelloPayload serverHello{};
    serverHello.pid = static_cast<uint32_t>(getpid());
    serverHello.uid = static_cast<uint32_t>(getuid());
    serverHello.nonce = nonce_;
    if (!SendSynchronous(MessageType::SERVER_HELLO, EncodeHello(serverHello), error)) {
        return false;
    }

    Frame configFrame{};
    if (ReceiveFrame(clientFd_, configFrame, error) != IoStatus::OK) {
        return false;
    }
    if (configFrame.type != MessageType::CONFIGURE || configFrame.sessionId != sessionId_ ||
        configFrame.sequence != 2) {
        error = "invalid configuration frame";
        return false;
    }
    return DecodeToolConfig(configFrame.payload, config, error);
}

bool UdsServer::StartAndHandshake(ToolConfig& config, std::string& error)
{
    if (!LoadEnvironment(error) || !CreateListener(error) || !AcceptClient(error) ||
        !ExchangeHandshake(config, error)) {
        return false;
    }
    return true;
}

bool UdsServer::SendSynchronous(MessageType type, const std::vector<uint8_t>& payload, std::string& error)
{
    std::lock_guard<std::mutex> sendLock(sendMutex_);
    Frame frame{};
    frame.type = type;
    frame.sessionId = sessionId_;
    frame.sequence = sendSequence_++;
    frame.payload = payload;
    if (SendFrame(clientFd_, frame, error) != IoStatus::OK) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        transportComplete_ = false;
        return false;
    }
    return true;
}

bool UdsServer::SendReady(const std::string& message, std::string& error)
{
    if (!SendSynchronous(MessageType::READY, EncodeText(message), error)) {
        return false;
    }
    if (!SetSocketTimeouts(clientFd_, 500, error)) {
        return false;
    }
    StartPublisher();
    return true;
}

void UdsServer::StartPublisher()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (publisherStarted_) {
        return;
    }
    publisherStarted_ = true;
    publisher_ = std::thread(&UdsServer::PublisherLoop, this);
}

bool UdsServer::Publish(MessageType type, const std::string& message)
{
    auto payload = EncodeText(message);
    if (payload.empty() && !message.empty()) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        ++droppedMessages_;
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (clientFd_ < 0 || closing_ || !transportComplete_ || queue_.size() >= kMaxQueuedMessages) {
            ++droppedMessages_;
            return false;
        }
        queue_.push_back({type, std::move(payload)});
    }
    queueReady_.notify_one();
    return true;
}

void UdsServer::PublisherLoop()
{
    while (true) {
        QueuedMessage message{};
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueReady_.wait(lock, [this] { return closing_ || !queue_.empty(); });
            if (queue_.empty() && closing_) {
                return;
            }
            message = std::move(queue_.front());
            queue_.pop_front();
        }
        std::string error;
        if (!SendSynchronous(message.type, message.payload, error)) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            droppedMessages_ += queue_.size();
            queue_.clear();
            closing_ = true;
            return;
        }
    }
}

void UdsServer::SendInitializationError(const std::string& message)
{
    if (clientFd_ < 0) {
        return;
    }
    std::string ignored;
    (void)SendSynchronous(MessageType::ERROR, EncodeText(message), ignored);
}

void UdsServer::SendFlowError(const std::string& message) noexcept
{
    try {
        if (clientFd_ < 0) {
            return;
        }
        std::string ignored;
        (void)SendSynchronous(MessageType::ERROR, EncodeText(message), ignored);
    } catch (...) {
        return;
    }
}

void UdsServer::Shutdown(const std::string& summary, const std::string& sessionEnd)
{
    bool shouldJoin = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (publisherStarted_ && !closing_) {
            if (!summary.empty()) {
                queue_.push_back({MessageType::SUMMARY, EncodeText(summary)});
            }
            if (!sessionEnd.empty()) {
                queue_.push_back({MessageType::SESSION_END, EncodeText(sessionEnd)});
            }
            closing_ = true;
        }
        shouldJoin = publisher_.joinable();
    }
    queueReady_.notify_all();
    if (shouldJoin) {
        publisher_.join();
    } else if (clientFd_ >= 0 && !sessionEnd.empty()) {
        std::string ignored;
        if (!summary.empty()) {
            (void)SendSynchronous(MessageType::SUMMARY, EncodeText(summary), ignored);
        }
        (void)SendSynchronous(MessageType::SESSION_END, EncodeText(sessionEnd), ignored);
    }
    CloseDescriptors();
}

void UdsServer::CloseDescriptors()
{
    if (clientFd_ >= 0) {
        (void)close(clientFd_);
        clientFd_ = -1;
    }
    if (listenFd_ >= 0) {
        (void)close(listenFd_);
        listenFd_ = -1;
    }
    if (!socketPath_.empty()) {
        (void)unlink(socketPath_.c_str());
        socketPath_.clear();
    }
}

uint64_t UdsServer::SessionId() const { return sessionId_; }

uint64_t UdsServer::DroppedMessages() const
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    return droppedMessages_;
}

bool UdsServer::TransportComplete() const
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    return transportComplete_;
}

} // namespace npu::sanitizer::ipc
