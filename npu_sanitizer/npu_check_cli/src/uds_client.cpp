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

bool UdsClient::ConnectWithRetry(const std::string& socketPath, int timeoutMs, pid_t childPid, std::string& error)
{
    if (socketPath.size() >= sizeof(sockaddr_un::sun_path)) {
        error = "UDS path exceeds sockaddr_un limit";
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    int lastError = ENOENT;
    while (std::chrono::steady_clock::now() < deadline) {
        siginfo_t childInfo{};
        if (waitid(P_PID, static_cast<id_t>(childPid), &childInfo, WEXITED | WNOHANG | WNOWAIT) == 0 &&
            childInfo.si_pid == childPid) {
            error = "child exited before injection handshake";
            return false;
        }
        fd_ = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
        if (fd_ < 0) {
            error = std::string("socket: ") + std::strerror(errno);
            return false;
        }
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, socketPath.c_str(), socketPath.size() + 1);
        if (connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0) {
            return ipc::SetSocketTimeouts(fd_, timeoutMs, error);
        }
        lastError = errno;
        Close();
        if (lastError != ENOENT && lastError != ECONNREFUSED) {
            error = std::string("connect: ") + std::strerror(lastError);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    error = std::string("connect timeout: ") + std::strerror(lastError);
    return false;
}

bool UdsClient::Send(ipc::MessageType type, const std::vector<uint8_t>& payload, std::string& error)
{
    ipc::Frame frame{};
    frame.type = type;
    frame.sessionId = sessionId_;
    frame.sequence = sendSequence_++;
    frame.payload = payload;
    return ipc::SendFrame(fd_, frame, error) == ipc::IoStatus::OK;
}

bool UdsClient::CheckServerIdentity(uint32_t childPid, const std::string& nonce, std::string& error)
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
    if (Receive(response, error) != ipc::IoStatus::OK) {
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
    if (hello.pid != childPid || hello.uid != getuid() || hello.nonce != nonce) {
        error = "server hello identity mismatch";
        return false;
    }
    return true;
}

bool UdsClient::ConnectAndConfigure(
    const std::string& socketPath, uint64_t sessionId, const std::string& nonce, uint32_t childPid, int timeoutMs,
    const ipc::ToolConfig& config, std::string& ready, std::string& error)
{
    sessionId_ = sessionId;
    sendSequence_ = 1;
    receiveSequence_ = 1;
    if (!ConnectWithRetry(socketPath, timeoutMs, static_cast<pid_t>(childPid), error)) {
        return false;
    }
    ipc::HelloPayload hello{};
    hello.pid = static_cast<uint32_t>(getpid());
    hello.uid = static_cast<uint32_t>(getuid());
    hello.nonce = nonce;
    if (!Send(ipc::MessageType::CLIENT_HELLO, ipc::EncodeHello(hello), error) ||
        !CheckServerIdentity(childPid, nonce, error)) {
        return false;
    }
    const auto encodedConfig = ipc::EncodeToolConfig(config);
    if (encodedConfig.empty()) {
        error = "cannot encode tool configuration";
        return false;
    }
    if (!Send(ipc::MessageType::CONFIGURE, encodedConfig, error)) {
        return false;
    }
    ipc::Frame response{};
    if (Receive(response, error) != ipc::IoStatus::OK) {
        return false;
    }
    if (response.type == ipc::MessageType::ERROR) {
        std::string message;
        std::string decodeError;
        if (!ipc::DecodeText(response.payload, message, decodeError)) {
            error = "injected library returned a malformed initialization error: " + decodeError;
            return false;
        }
        error = "injected library initialization failed: " + message;
        return false;
    }
    if (response.type != ipc::MessageType::READY) {
        error = "expected READY frame";
        return false;
    }
    if (!ipc::DecodeText(response.payload, ready, error)) {
        return false;
    }
    return ipc::SetSocketTimeouts(fd_, 500, error);
}

ipc::IoStatus UdsClient::Receive(ipc::Frame& frame, std::string& error)
{
    ipc::IoStatus status = ipc::ReceiveFrame(fd_, frame, error);
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
