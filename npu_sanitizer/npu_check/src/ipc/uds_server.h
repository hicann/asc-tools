#ifndef NPU_CHECK_IPC_UDS_SERVER_H
#define NPU_CHECK_IPC_UDS_SERVER_H

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
    bool ExchangeHandshake(ToolConfig& config, std::string& error);
    bool SendSynchronous(MessageType type, const std::vector<uint8_t>& payload, std::string& error);
    void StartPublisher();
    void PublisherLoop();
    void CloseDescriptors();

    int listenFd_ = -1;
    int clientFd_ = -1;
    std::string socketPath_;
    std::string nonce_;
    uint64_t sessionId_ = 0;
    uint32_t expectedCliPid_ = 0;
    int handshakeTimeoutMs_ = 10000;
    uint64_t sendSequence_ = 1;

    mutable std::mutex queueMutex_;
    std::condition_variable queueReady_;
    std::deque<QueuedMessage> queue_;
    std::thread publisher_;
    bool publisherStarted_ = false;
    bool closing_ = false;
    bool transportComplete_ = true;
    uint64_t droppedMessages_ = 0;
    static constexpr size_t kMaxQueuedMessages = 1024;
};

} // namespace npu::sanitizer::ipc

#endif
