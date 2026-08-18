#include "uds_transport.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>

namespace npu::sanitizer::ipc {
namespace {

std::string ErrnoMessage(const char* operation) { return std::string(operation) + ": " + std::strerror(errno); }

} // namespace

IoStatus SendFrame(int fd, const Frame& frame, std::string& error)
{
    auto bytes = EncodeFrame(frame);
    if (bytes.empty()) {
        error = "cannot encode frame";
        return IoStatus::PROTOCOL_ERROR;
    }
    while (true) {
        const ssize_t sent = send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            error = "send timeout";
            return IoStatus::TIMEOUT;
        }
        if (sent < 0) {
            error = ErrnoMessage("send");
            return IoStatus::SYSTEM_ERROR;
        }
        if (static_cast<size_t>(sent) != bytes.size()) {
            error = "short SOCK_SEQPACKET send";
            return IoStatus::SYSTEM_ERROR;
        }
        return IoStatus::OK;
    }
}

IoStatus ReceiveFrame(int fd, Frame& frame, std::string& error)
{
    std::array<uint8_t, kMaxFrameSize> buffer{};
    iovec iov{};
    iov.iov_base = buffer.data();
    iov.iov_len = buffer.size();
    msghdr message{};
    message.msg_iov = &iov;
    message.msg_iovlen = 1;

    ssize_t received = 0;
    while (true) {
        received = recvmsg(fd, &message, MSG_CMSG_CLOEXEC);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    if (received == 0) {
        return IoStatus::CLOSED;
    }
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        error = "receive timeout";
        return IoStatus::TIMEOUT;
    }
    if (received < 0) {
        error = ErrnoMessage("recvmsg");
        return IoStatus::SYSTEM_ERROR;
    }
    if ((message.msg_flags & MSG_TRUNC) != 0) {
        error = "truncated frame";
        return IoStatus::PROTOCOL_ERROR;
    }
    if (!DecodeFrame(buffer.data(), static_cast<size_t>(received), frame, error)) {
        return IoStatus::PROTOCOL_ERROR;
    }
    return IoStatus::OK;
}

bool SetSocketTimeouts(int fd, int timeoutMs, std::string& error)
{
    if (timeoutMs <= 0) {
        error = "timeout must be positive";
        return false;
    }
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>(timeoutMs % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        error = ErrnoMessage("setsockopt");
        return false;
    }
    return true;
}

bool WaitReadable(int fd, int timeoutMs, std::string& error)
{
    pollfd descriptor{};
    descriptor.fd = fd;
    descriptor.events = POLLIN;
    int rc = 0;
    do {
        rc = poll(&descriptor, 1, timeoutMs);
    } while (rc < 0 && errno == EINTR);
    if (rc == 0) {
        error = "poll timeout";
        return false;
    }
    if (rc < 0) {
        error = ErrnoMessage("poll");
        return false;
    }
    if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
        error = "socket poll error";
        return false;
    }
    if ((descriptor.revents & (POLLIN | POLLHUP)) == 0) {
        error = "socket did not become readable";
        return false;
    }
    return true;
}

} // namespace npu::sanitizer::ipc
