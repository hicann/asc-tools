#ifndef NPU_CHECK_COMMON_UDS_TRANSPORT_H
#define NPU_CHECK_COMMON_UDS_TRANSPORT_H

#include "wire_protocol.h"

#include <cstdint>
#include <string>

namespace npu::sanitizer::ipc {

enum class IoStatus : uint8_t {
    OK,
    CLOSED,
    TIMEOUT,
    SYSTEM_ERROR,
    PROTOCOL_ERROR,
};

IoStatus SendFrame(int fd, const Frame& frame, std::string& error);
IoStatus ReceiveFrame(int fd, Frame& frame, std::string& error);
bool SetSocketTimeouts(int fd, int timeoutMs, std::string& error);
bool WaitReadable(int fd, int timeoutMs, std::string& error);

} // namespace npu::sanitizer::ipc

#endif
