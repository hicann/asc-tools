// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_IPC_UDS_SERVER_H
#define NPU_CHECK_IPC_UDS_SERVER_H

#include "uds_transport.h"
#include "wire_protocol.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace npu::sanitizer::ipc {

class UdsServer {
public:
    UdsServer() = default;
    ~UdsServer();
    UdsServer(const UdsServer&) = delete;
    UdsServer& operator=(const UdsServer&) = delete;

    bool StartAndHandshake(ToolConfig& config, std::string& error);
    bool SendReady(const std::string& message, std::string& error);
    bool Publish(MessageType type, const std::string& message);
    void SendInitializationError(const std::string& message);
    void SendFlowError(const std::string& message) noexcept;
    void Shutdown(const std::string& summary, const std::string& sessionEnd);

    uint64_t SessionId() const;
    uint64_t DroppedMessages() const;
    bool TransportComplete() const;

private:
    struct QueuedMessage {
        MessageType type = MessageType::LOG;
        std::vector<uint8_t> payload;
    };

    bool LoadEnvironment(std::string& error);
    bool CreateListener(std::string& error);
    bool AcceptClient(std::string& error);
    // 收一帧并做统一校验：跳过 must-ignore 类型、校验 session_id、校验 sequence 严格
    // 递增、再确认类型符合预期。
    bool ReceiveChecked(Frame& frame, MessageType expected, DeadlineMs deadline, std::string& error);
    bool ExchangeHandshake(ToolConfig& config, std::string& error);
    bool SendSynchronous(
        MessageType type, const std::vector<uint8_t>& payload, DeadlineMs deadline, std::string& error);
    void StartPublisher();
    void PublisherLoop();
    void CloseDescriptors();

    int listenFd_ = -1;
    int clientFd_ = -1;
    // 抽象命名空间地址名（含前导 '@'），不是文件路径。
    std::string udsName_;
    uint64_t sessionId_ = 0;
    uint32_t expectedCliPid_ = 0;
    int handshakeTimeoutMs_ = 10000;
    // 握手阶段的绝对截止时刻，在 AcceptClient 中一次算出，覆盖 accept、Hello 往返、
    // Configure 接收到 Ready 发出的全过程。Ready 之后的发送不再受它约束。
    DeadlineMs handshakeDeadline_ = kNoDeadline;
    uint64_t sendSequence_ = 1;
    // 期望从 CLI 方向收到的下一个 sequence。两个方向各自维护计数器，互不影响。
    uint64_t receiveSequence_ = 1;
    std::mutex sendMutex_;

    mutable std::mutex queueMutex_;
    std::condition_variable queueReady_;
    std::deque<QueuedMessage> queue_;
    std::thread publisher_;
    bool publisherStarted_ = false;
    bool closing_ = false;
    bool transportComplete_ = true;
    uint64_t droppedMessages_ = 0;
    static constexpr size_t kMaxQueuedMessages = 1024;
    // listen 的 backlog。抽象地址没有权限位，任何进程都能发起 connect，取 1 会让一个
    // 抢先连接就堵死真正的 CLI，因此留出余量并配合循环 accept 丢弃不合格连接。
    static constexpr int kListenBacklog = 8;
    // Ready 之后发送结果的超时上限。CLI 侧的等待不设超时，但发送侧不能照搬：若 CLI 已死
    // 或长时间不读，sendmsg 会在发送缓冲填满后持续 EAGAIN，无限等待 POLLOUT 会把目标
    // 应用卡在退出路径上、NPU 资源不释放。超时即放弃发送，让应用正常退出。
    static constexpr int kResultSendTimeoutMs = 30000;
};

} // namespace npu::sanitizer::ipc

#endif
