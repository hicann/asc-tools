/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "uds_client.h"

#include "uds_transport.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace npu::sanitizer::cli {

UdsClient::~UdsClient() { Close(); }

namespace {

// connect 重试节奏：首次 20 ms，指数退避至上限 100 ms。
constexpr int kInitialBackoffMs = 20;
constexpr int kMaxBackoffMs = 100;

// connect 失败后是否值得重试。抽象命名空间下这三个 errno 分别对应目标进程启动过程中的
// 不同阶段，都属于"还没准备好"而不是"失败"：
//   ECONNREFUSED —— 地址尚未被 bind，或已 bind 但还没 listen；
//   EAGAIN       —— 已经 listen，但 backlog 暂时排满；
//   EINTR        —— 被 CLI 自己安装的信号转发器打断。
// 注意这里没有 ENOENT：那是文件路径 socket 才会出现的错误，地址不经过文件系统查找之后
// "还没 bind"与"bind 了还没 listen"就合并成同一个 ECONNREFUSED，无法再从 errno 区分。
bool IsRetryableConnectErrno(int value)
{
    return value == ECONNREFUSED || value == EAGAIN || value == EWOULDBLOCK || value == EINTR;
}

} // namespace

bool UdsClient::ConnectWithRetry(
    const std::string& udsName, ipc::DeadlineMs deadline, pid_t childPid, std::string& error)
{
    sockaddr_un address{};
    socklen_t addrLen = 0;
    if (!ipc::BuildAbstractAddress(udsName, address, addrLen, error)) {
        return false;
    }

    const ipc::DeadlineMs start = ipc::MonotonicNowMs();
    int backoffMs = kInitialBackoffMs;
    int lastError = ECONNREFUSED;
    uint64_t attempt = 1;
    while (true) {
        if (ipc::MonotonicNowMs() >= deadline) {
            error = std::string("connect timed out: ") + std::strerror(lastError);
            return false;
        }

        // 建连阶段是整个流程里唯一需要显式探测子进程的地方：此时还没有 fd 可以 poll，
        // 对端的死亡不会以 POLLHUP 的形式浮现出来。连上之后就不再需要这段探测。
        //
        // 两个细节不能省：waitid 的 flags 必须含 WEXITED，否则直接返回 EINVAL，这段
        // 提前退出的优化会静默失效（正确性不受影响，表现只是每次都白等满整个超时）；
        // siginfo_t 必须预清零并检查 si_pid，因为带 WNOHANG 且无状态变化时 waitid 返回
        // 0 却不填充 info，只看返回值会把每一轮都误判成"子进程已退出"。
        siginfo_t childInfo{};
        childInfo.si_pid = 0;
        if (waitid(P_PID, static_cast<id_t>(childPid), &childInfo, WEXITED | WNOHANG | WNOWAIT) == 0 &&
            childInfo.si_pid == childPid) {
            error = "child exited before injection handshake";
            return false;
        }

        // 每次尝试都用全新的 fd：connect 失败后 socket 的状态是未定义的，复用不可移植。
        fd_ = ipc::CreateSeqpacketSocket(error);
        if (fd_ < 0) {
            return false;
        }
        if (connect(fd_, reinterpret_cast<const sockaddr*>(&address), addrLen) == 0) {
            Log("[UDS] phase=connect attempt=" + std::to_string(attempt) +
                " errno=0 elapsed_ms=" + std::to_string(ipc::MonotonicNowMs() - start));
            return true;
        }
        lastError = errno;
        Close();
        if (!IsRetryableConnectErrno(lastError)) {
            error = std::string("connect: ") + std::strerror(lastError);
            return false;
        }

        // 退避同样受同一个 deadline 约束，不能睡过头。
        const int64_t remain = deadline - ipc::MonotonicNowMs();
        if (remain <= 0) {
            error = std::string("connect timed out: ") + std::strerror(lastError);
            return false;
        }
        const auto napMs = static_cast<int>(std::min<int64_t>(remain, backoffMs));
        std::this_thread::sleep_for(std::chrono::milliseconds(napMs));
        backoffMs = std::min(backoffMs * 2, kMaxBackoffMs);
        ++attempt;
    }
}

bool UdsClient::Send(
    ipc::MessageType type, const std::vector<uint8_t>& payload, ipc::DeadlineMs deadline, std::string& error)
{
    ipc::Frame frame{};
    frame.type = type;
    frame.sessionId = sessionId_;
    frame.sequence = sendSequence_++;
    frame.payload = payload;
    return ipc::SendFrame(fd_, frame, deadline, error) == ipc::IoStatus::OK;
}

bool UdsClient::CheckServerIdentity(uint32_t childPid, ipc::DeadlineMs deadline, std::string& error)
{
    ucred peer{};
    socklen_t peerSize = sizeof(peer);
    if (getsockopt(fd_, SOL_SOCKET, SO_PEERCRED, &peer, &peerSize) != 0) {
        error = std::string("getsockopt(SO_PEERCRED): ") + std::strerror(errno);
        return false;
    }
    if (peer.uid != getuid() || static_cast<uint32_t>(peer.pid) != childPid) {
        error = "UDS server credentials do not match the child process";
        return false;
    }
    ipc::Frame response{};
    if (Receive(response, deadline, error) != ipc::IoStatus::OK) {
        return false;
    }
    if (response.type != ipc::MessageType::SERVER_HELLO) {
        error = "invalid server hello frame";
        return false;
    }
    ipc::HelloPayload hello{};
    if (!ipc::DecodeHello(response.payload, hello, error)) {
        return false;
    }
    // 对端 minor 取自 ServerHello 的帧头，与本实现取较小值。收到更高的 minor 不是错误。
    negotiatedMinor_ = ipc::NegotiateMinor(response.minor);
    // Hello 里的 pid/uid 必须与 SO_PEERCRED 的结果交叉比对：前者是对端自行声明的，
    // 后者由内核填写。两者不一致说明对端实现有问题或存在冒充，按协议错误断连。
    if (hello.pid != childPid || hello.uid != getuid()) {
        error = "server hello identity mismatch";
        return false;
    }
    return true;
}

bool UdsClient::ConnectAndConfigure(
    const std::string& udsName, uint64_t sessionId, uint32_t childPid, ipc::DeadlineMs deadline,
    const ipc::ConfigureRequest& configure, std::string& error)
{
    sessionId_ = sessionId;
    sendSequence_ = 1;
    receiveSequence_ = 1;

    // deadline 是调用方在 fork 之前算好的绝对时刻，覆盖 connect 重试、Hello 往返、
    // Configure 发送、Ready 接收的全过程，而不是每步各算一次 —— 后者最坏会把用户
    // 设定的超时放大到步数倍，破坏该值"总时长"的语义。
    const ipc::DeadlineMs start = ipc::MonotonicNowMs();
    if (!ConnectWithRetry(udsName, deadline, static_cast<pid_t>(childPid), error)) {
        return false;
    }
    ipc::HelloPayload hello{};
    hello.pid = static_cast<uint32_t>(getpid());
    hello.uid = static_cast<uint32_t>(getuid());
    if (!Send(ipc::MessageType::CLIENT_HELLO, ipc::EncodeHello(hello), deadline, error) ||
        !CheckServerIdentity(childPid, deadline, error)) {
        Log("[UDS] phase=handshake result=failed");
        return false;
    }
    Log("[UDS] phase=handshake peer_pid=" + std::to_string(childPid) + " peer_uid=" + std::to_string(getuid()) +
        " cred_match=1 negotiated_minor=" + std::to_string(negotiatedMinor_) + " result=ok");

    // 在发送之前查：用户请求了当前协商版本不支持的选项时，直接报"版本不支持"，
    // 而不是把对端无法理解的配置送上线路再等它回 Error。
    if (!ipc::ValidateConfigureMinor(configure, negotiatedMinor_, error)) {
        return false;
    }
    const auto encodedConfig = ipc::EncodeConfigure(configure);
    // 空编码只可能来自超限；Configure 本身允许为空工具集合以外的任何合法组合。
    if (encodedConfig.empty() && !configure.tools.empty()) {
        error = "cannot encode tool configuration";
        return false;
    }
    size_t optionCount = 0;
    for (const auto& tool : configure.tools) {
        optionCount += tool.options.size();
    }
    Log("[UDS] phase=configure tool_count=" + std::to_string(configure.tools.size()) + " option_count=" +
        std::to_string(optionCount) + " length=" + std::to_string(ipc::kWireHeaderSize + encodedConfig.size()) +
        " payload_size=" + std::to_string(encodedConfig.size()));
    if (!Send(ipc::MessageType::CONFIGURE, encodedConfig, deadline, error)) {
        return false;
    }
    ipc::Frame response{};
    if (Receive(response, deadline, error) != ipc::IoStatus::OK) {
        return false;
    }
    if (response.type == ipc::MessageType::ERROR) {
        ipc::ErrorPayload failure{};
        std::string decodeError;
        if (!ipc::DecodeError(response.payload, failure, decodeError)) {
            error = "injected library returned a malformed initialization error: " + decodeError;
            return false;
        }
        // domain/code 是稳定取值，进诊断文本的前缀；message 只原样转述，不参与任何判定。
        error =
            "injected library initialization failed (domain=" + std::to_string(static_cast<unsigned>(failure.domain)) +
            " code=" + std::to_string(failure.code) + "): " + failure.message;
        return false;
    }
    if (response.type != ipc::MessageType::READY) {
        error = "expected READY frame";
        return false;
    }
    // Ready 的 payload 必须为空；非空说明对端实现与本协议版本不一致。
    if (!response.payload.empty()) {
        error = "READY frame carries an unexpected payload";
        return false;
    }
    Log("[UDS] phase=wait_ready elapsed_ms=" + std::to_string(ipc::MonotonicNowMs() - start) + " result=ready");
    // 握手到此结束，deadline 使命完成。之后进入采集阶段，等待由调用方按 kNoDeadline 驱动。
    return true;
}

ipc::IoStatus UdsClient::Receive(ipc::Frame& frame, ipc::DeadlineMs deadline, std::string& error)
{
    ipc::IoStatus status = ipc::ReceiveFrame(fd_, frame, deadline, error);
    if (status != ipc::IoStatus::OK) {
        return status;
    }
    if (frame.sessionId != sessionId_) {
        error = "received frame for another session";
        return ipc::IoStatus::PROTOCOL_ERROR;
    }
    if (frame.sequence != receiveSequence_) {
        error = "non-contiguous server sequence";
        return ipc::IoStatus::PROTOCOL_ERROR;
    }
    ++receiveSequence_;
    return ipc::IoStatus::OK;
}

void UdsClient::Close()
{
    if (fd_ >= 0) {
        (void)close(fd_);
        fd_ = -1;
    }
}

} // namespace npu::sanitizer::cli
