/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "wire_protocol.h"

#include <limits>
#include <utility>

namespace npu::sanitizer::ipc {
namespace {

class Encoder {
public:
    void PutU8(uint8_t value) { data_.push_back(value); }

    void PutU16(uint16_t value)
    {
        data_.push_back(static_cast<uint8_t>(value >> 8u));
        data_.push_back(static_cast<uint8_t>(value));
    }

    void PutU32(uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8) {
            data_.push_back(static_cast<uint8_t>(value >> static_cast<unsigned>(shift)));
        }
    }

    void PutU64(uint64_t value)
    {
        for (int shift = 56; shift >= 0; shift -= 8) {
            data_.push_back(static_cast<uint8_t>(value >> static_cast<unsigned>(shift)));
        }
    }

    bool PutString(const std::string& value)
    {
        if (value.size() > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        PutU32(static_cast<uint32_t>(value.size()));
        data_.insert(data_.end(), value.begin(), value.end());
        return true;
    }

    std::vector<uint8_t> Take() { return std::move(data_); }

private:
    std::vector<uint8_t> data_;
};

class Decoder {
public:
    Decoder(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool GetU8(uint8_t& value)
    {
        if (!Need(1)) {
            return false;
        }
        value = data_[offset_++];
        return true;
    }

    bool GetU16(uint16_t& value)
    {
        if (!Need(2)) {
            return false;
        }
        value = static_cast<uint16_t>(
            (static_cast<uint16_t>(data_[offset_]) << 8u) | static_cast<uint16_t>(data_[offset_ + 1]));
        offset_ += 2;
        return true;
    }

    bool GetU32(uint32_t& value)
    {
        if (!Need(4)) {
            return false;
        }
        uint32_t result = 0;
        for (size_t i = 0; i < 4; ++i) {
            result = (result << 8u) | data_[offset_ + i];
        }
        offset_ += 4;
        value = result;
        return true;
    }

    bool GetU64(uint64_t& value)
    {
        if (!Need(8)) {
            return false;
        }
        uint64_t result = 0;
        for (size_t i = 0; i < 8; ++i) {
            result = (result << 8u) | data_[offset_ + i];
        }
        offset_ += 8;
        value = result;
        return true;
    }

    bool GetString(std::string& value, size_t maxSize)
    {
        uint32_t length = 0;
        if (!GetU32(length) || length > maxSize || !Need(length)) {
            return false;
        }
        value.assign(reinterpret_cast<const char*>(data_ + offset_), length);
        offset_ += length;
        return true;
    }

    bool Done() const { return offset_ == size_; }

private:
    bool Need(size_t bytes) const { return bytes <= size_ - offset_; }

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t offset_ = 0;
};

bool Fail(std::string& error, const char* message)
{
    error = message;
    return false;
}

} // namespace

std::vector<uint8_t> EncodeFrame(const Frame& frame)
{
    if (frame.payload.size() > kMaxPayloadSize) {
        return {};
    }
    Encoder encoder;
    encoder.PutU32(kProtocolMagic);
    encoder.PutU16(kProtocolMajor);
    encoder.PutU16(kProtocolMinor);
    encoder.PutU16(static_cast<uint16_t>(frame.type));
    encoder.PutU16(frame.flags);
    encoder.PutU32(static_cast<uint32_t>(frame.payload.size()));
    encoder.PutU64(frame.sessionId);
    encoder.PutU64(frame.sequence);
    auto output = encoder.Take();
    output.insert(output.end(), frame.payload.begin(), frame.payload.end());
    return output;
}

bool DecodeFrame(const uint8_t* data, size_t size, Frame& frame, std::string& error)
{
    if (data == nullptr || size < kWireHeaderSize) {
        return Fail(error, "short frame header");
    }
    Decoder decoder(data, kWireHeaderSize);
    uint32_t magic = 0;
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t type = 0;
    uint16_t flags = 0;
    uint32_t payloadSize = 0;
    uint64_t sessionId = 0;
    uint64_t sequence = 0;
    if (!decoder.GetU32(magic) || !decoder.GetU16(major) || !decoder.GetU16(minor) || !decoder.GetU16(type) ||
        !decoder.GetU16(flags) || !decoder.GetU32(payloadSize) || !decoder.GetU64(sessionId) ||
        !decoder.GetU64(sequence)) {
        return Fail(error, "malformed frame header");
    }
    if (magic != kProtocolMagic) {
        return Fail(error, "invalid protocol magic");
    }
    if (major != kProtocolMajor) {
        return Fail(error, "incompatible protocol major version");
    }
    if (minor > kProtocolMinor) {
        return Fail(error, "unsupported protocol minor version");
    }
    if (payloadSize > kMaxPayloadSize || size != kWireHeaderSize + payloadSize) {
        return Fail(error, "invalid frame payload size");
    }
    if (type < static_cast<uint16_t>(MessageType::CLIENT_HELLO) || type > static_cast<uint16_t>(MessageType::ERROR)) {
        return Fail(error, "unknown message type");
    }
    frame.type = static_cast<MessageType>(type);
    frame.flags = flags;
    frame.sessionId = sessionId;
    frame.sequence = sequence;
    frame.payload.assign(data + kWireHeaderSize, data + size);
    return true;
}

std::vector<uint8_t> EncodeHello(const HelloPayload& hello)
{
    Encoder encoder;
    encoder.PutU32(hello.pid);
    encoder.PutU32(hello.uid);
    if (!encoder.PutString(hello.nonce)) {
        return {};
    }
    return encoder.Take();
}

bool DecodeHello(const std::vector<uint8_t>& payload, HelloPayload& hello, std::string& error)
{
    Decoder decoder(payload.data(), payload.size());
    if (!decoder.GetU32(hello.pid) || !decoder.GetU32(hello.uid) || !decoder.GetString(hello.nonce, 128) ||
        !decoder.Done()) {
        return Fail(error, "malformed hello payload");
    }
    return true;
}

std::vector<uint8_t> EncodeToolConfig(const ToolConfig& config)
{
    if (config.compileOptions.size() > kMaxCompileOptions) {
        return {};
    }
    Encoder encoder;
    encoder.PutU8(config.strict ? 1 : 0);
    encoder.PutU8(config.keepTemp ? 1 : 0);
    encoder.PutU16(0);
    if (!encoder.PutString(config.toolName) || !encoder.PutString(config.logFile) ||
        !encoder.PutString(config.workDir) || !encoder.PutString(config.probeCacheDir)) {
        return {};
    }
    encoder.PutU32(static_cast<uint32_t>(config.compileOptions.size()));
    for (const auto& option : config.compileOptions) {
        if (!encoder.PutString(option)) {
            return {};
        }
    }
    auto payload = encoder.Take();
    return payload.size() <= kMaxPayloadSize ? payload : std::vector<uint8_t>{};
}

bool DecodeToolConfig(const std::vector<uint8_t>& payload, ToolConfig& config, std::string& error)
{
    Decoder decoder(payload.data(), payload.size());
    uint8_t strict = 0;
    uint8_t keepTemp = 0;
    uint16_t reserved = 0;
    uint32_t optionCount = 0;
    if (!decoder.GetU8(strict) || !decoder.GetU8(keepTemp) || !decoder.GetU16(reserved) || strict > 1 || keepTemp > 1 ||
        reserved != 0 || !decoder.GetString(config.toolName, 32) || !decoder.GetString(config.logFile, 4096) ||
        !decoder.GetString(config.workDir, 4096) || !decoder.GetString(config.probeCacheDir, 4096) ||
        !decoder.GetU32(optionCount) || optionCount > kMaxCompileOptions) {
        return Fail(error, "malformed tool configuration");
    }
    config.strict = strict != 0;
    config.keepTemp = keepTemp != 0;
    config.compileOptions.clear();
    config.compileOptions.reserve(optionCount);
    for (uint32_t i = 0; i < optionCount; ++i) {
        std::string option;
        if (!decoder.GetString(option, 4096)) {
            return Fail(error, "malformed compile option");
        }
        config.compileOptions.push_back(std::move(option));
    }
    if (!decoder.Done()) {
        return Fail(error, "trailing configuration data");
    }
    return true;
}

std::vector<uint8_t> EncodeText(const std::string& text)
{
    if (text.size() > kMaxPayloadSize) {
        return {};
    }
    return std::vector<uint8_t>(text.begin(), text.end());
}

bool DecodeText(const std::vector<uint8_t>& payload, std::string& text, std::string& error)
{
    for (uint8_t value : payload) {
        if (value == 0) {
            return Fail(error, "text payload contains NUL");
        }
    }
    text.assign(payload.begin(), payload.end());
    return true;
}

const char* MessageTypeName(MessageType type)
{
    switch (type) {
        case MessageType::CLIENT_HELLO:
            return "CLIENT_HELLO";
        case MessageType::SERVER_HELLO:
            return "SERVER_HELLO";
        case MessageType::CONFIGURE:
            return "CONFIGURE";
        case MessageType::READY:
            return "READY";
        case MessageType::DIAGNOSTIC:
            return "DIAGNOSTIC";
        case MessageType::LOG:
            return "LOG";
        case MessageType::SUMMARY:
            return "SUMMARY";
        case MessageType::SESSION_END:
            return "SESSION_END";
        case MessageType::ERROR:
            return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace npu::sanitizer::ipc
