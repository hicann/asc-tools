#ifndef NPU_CHECK_WIRE_PROTOCOL_H
#define NPU_CHECK_WIRE_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace npu::sanitizer::ipc {

constexpr uint32_t kProtocolMagic = 0x4e53414eu;
constexpr uint16_t kProtocolMajor = 1;
constexpr uint16_t kProtocolMinor = 0;
constexpr size_t kWireHeaderSize = 32;
constexpr size_t kMaxPayloadSize = 64 * 1024;
constexpr size_t kMaxFrameSize = kWireHeaderSize + kMaxPayloadSize;
constexpr size_t kMaxCompileOptions = 128;

constexpr const char* kSocketPathEnv = "NPU_CHECK_UDS_PATH";
constexpr const char* kSessionIdEnv = "NPU_CHECK_SESSION_ID";
constexpr const char* kSessionNonceEnv = "NPU_CHECK_SESSION_NONCE";
constexpr const char* kCliPidEnv = "NPU_CHECK_CLI_PID";
constexpr const char* kHandshakeTimeoutEnv = "NPU_CHECK_HANDSHAKE_TIMEOUT_MS";

enum class MessageType : uint16_t {
    CLIENT_HELLO = 1,
    SERVER_HELLO = 2,
    CONFIGURE = 3,
    READY = 4,
    DIAGNOSTIC = 5,
    LOG = 6,
    SUMMARY = 7,
    SESSION_END = 8,
    ERROR = 9,
};

struct Frame {
    MessageType type = MessageType::ERROR;
    uint16_t flags = 0;
    uint64_t sessionId = 0;
    uint64_t sequence = 0;
    std::vector<uint8_t> payload;
};

struct HelloPayload {
    uint32_t pid = 0;
    uint32_t uid = 0;
    std::string nonce;
};

struct ToolConfig {
    std::string toolName;
    bool strict = true;
    bool keepTemp = true;
    std::string logFile;
    std::string workDir;
    std::string probeCacheDir;
    std::vector<std::string> compileOptions;
};

std::vector<uint8_t> EncodeFrame(const Frame& frame);
bool DecodeFrame(const uint8_t* data, size_t size, Frame& frame, std::string& error);

std::vector<uint8_t> EncodeHello(const HelloPayload& hello);
bool DecodeHello(const std::vector<uint8_t>& payload, HelloPayload& hello, std::string& error);

std::vector<uint8_t> EncodeToolConfig(const ToolConfig& config);
bool DecodeToolConfig(const std::vector<uint8_t>& payload, ToolConfig& config, std::string& error);

std::vector<uint8_t> EncodeText(const std::string& text);
bool DecodeText(const std::vector<uint8_t>& payload, std::string& text, std::string& error);

const char* MessageTypeName(MessageType type);

} // namespace npu::sanitizer::ipc

#endif
