// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "wire_protocol.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace npu::sanitizer::ipc {
namespace {

TEST(WireProtocolTest, RoundTripsHelloAndToolConfiguration)
{
    HelloPayload hello{};
    hello.pid = 123;
    hello.uid = 456;
    HelloPayload decodedHello{};
    std::string error;
    // Hello 固定 8 字节，不再携带 nonce。
    EXPECT_EQ(EncodeHello(hello).size(), 8U);
    ASSERT_TRUE(DecodeHello(EncodeHello(hello), decodedHello, error)) << error;
    EXPECT_EQ(decodedHello.pid, hello.pid);
    EXPECT_EQ(decodedHello.uid, hello.uid);

    // 尾随字节必须被拒绝：解码结束位置要恰好等于 payload_size。
    auto trailing = EncodeHello(hello);
    trailing.push_back(0);
    HelloPayload ignored{};
    EXPECT_FALSE(DecodeHello(trailing, ignored, error));

    ToolConfig config{};
    config.toolName = "memcheck";
    config.strict = true;
    config.keepTemp = false;
    config.logFile = "/tmp/report.log";
    config.workDir = "/tmp/work";
    config.probeCacheDir = "/tmp/cache";
    config.compileOptions = {"-g", "-O2"};
    ToolConfig decodedConfig{};
    ASSERT_TRUE(DecodeToolConfig(EncodeToolConfig(config), decodedConfig, error)) << error;
    EXPECT_EQ(decodedConfig.toolName, config.toolName);
    EXPECT_EQ(decodedConfig.strict, config.strict);
    EXPECT_EQ(decodedConfig.keepTemp, config.keepTemp);
    EXPECT_EQ(decodedConfig.compileOptions, config.compileOptions);
}

TEST(WireProtocolTest, RoundTripsFrame)
{
    ToolConfig config{};
    config.toolName = "memcheck";
    config.compileOptions = {"-g"};
    Frame frame{};
    frame.type = MessageType::CONFIGURE;
    frame.flags = 3;
    frame.sessionId = 0x1020304050607080ULL;
    frame.sequence = 9;
    frame.payload = EncodeToolConfig(config);

    const auto bytes = EncodeFrame(frame);
    Frame decoded{};
    std::string error;
    ASSERT_TRUE(DecodeFrame(bytes.data(), bytes.size(), decoded, error)) << error;
    EXPECT_EQ(decoded.type, frame.type);
    EXPECT_EQ(decoded.flags, frame.flags);
    EXPECT_EQ(decoded.sessionId, frame.sessionId);
    EXPECT_EQ(decoded.sequence, frame.sequence);
    EXPECT_EQ(decoded.payload, frame.payload);
}

TEST(WireProtocolTest, RejectsMalformedFramesAndText)
{
    Frame frame{};
    frame.type = MessageType::CONFIGURE;
    frame.sessionId = 1;
    frame.sequence = 1;
    const auto bytes = EncodeFrame(frame);
    Frame decoded{};
    std::string error;

    EXPECT_FALSE(DecodeFrame(bytes.data(), kWireHeaderSize - 1, decoded, error));
    auto corrupt = bytes;
    corrupt[0] = 0;
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));
    corrupt = bytes;
    corrupt[6] = 0;
    corrupt[7] = 1;
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));
    corrupt = bytes;
    corrupt.push_back(0);
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));

    std::string text;
    ASSERT_TRUE(DecodeText(EncodeText("hello"), text, error)) << error;
    EXPECT_EQ(text, "hello");
    const std::vector<uint8_t> invalidText{'a', 0, 'b'};
    EXPECT_FALSE(DecodeText(invalidText, text, error));
}

// 帧头偏移常量，与 EncodeFrame 的布局一一对应。
constexpr size_t kOffsetType = 8;
constexpr size_t kOffsetFlags = 10;
constexpr size_t kOffsetLength = 12;
constexpr size_t kOffsetPayloadSize = 16;

void PutU16At(std::vector<uint8_t>& bytes, size_t offset, uint16_t value)
{
    bytes[offset] = static_cast<uint8_t>(value >> 8);
    bytes[offset + 1] = static_cast<uint8_t>(value & 0xffu);
}

void PutU32At(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<uint8_t>(value >> ((3 - index) * 8));
    }
}

std::vector<uint8_t> SampleFrame()
{
    Frame frame{};
    frame.type = MessageType::READY;
    frame.sessionId = 42;
    frame.sequence = 7;
    frame.payload = EncodeText("payload");
    return EncodeFrame(frame);
}

TEST(WireProtocolTest, HeaderIs36BytesAndCarriesLength)
{
    EXPECT_EQ(kWireHeaderSize, 36U);
    const auto bytes = SampleFrame();
    Frame decoded{};
    std::string error;
    ASSERT_TRUE(DecodeFrame(bytes.data(), bytes.size(), decoded, error)) << error;
    // length 必须等于整帧长度，也就是 36 + payload_size。
    const size_t payloadSize = bytes.size() - kWireHeaderSize;
    uint32_t length = 0;
    for (size_t index = 0; index < 4; ++index) {
        length = (length << 8) | bytes[kOffsetLength + index];
    }
    EXPECT_EQ(length, kWireHeaderSize + payloadSize);
}

TEST(WireProtocolTest, RejectsInconsistentLength)
{
    std::string error;
    Frame decoded{};

    // length 与 payload_size 对不上。
    auto corrupt = SampleFrame();
    PutU32At(corrupt, kOffsetLength, static_cast<uint32_t>(corrupt.size() + 1));
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));

    // length 与实际收到的数据报长度对不上（两者都改，绕过上一条校验）。
    corrupt = SampleFrame();
    const uint32_t shorter = static_cast<uint32_t>(corrupt.size() - 1);
    PutU32At(corrupt, kOffsetLength, shorter);
    PutU32At(corrupt, kOffsetPayloadSize, static_cast<uint32_t>(shorter - kWireHeaderSize));
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));
}

TEST(WireProtocolTest, RejectsReservedMustUnderstandFlags)
{
    std::string error;
    Frame decoded{};

    // bit 3-7 任一置位即协议错误。
    auto corrupt = SampleFrame();
    PutU16At(corrupt, kOffsetFlags, static_cast<uint16_t>(0x0008u));
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));
    corrupt = SampleFrame();
    PutU16At(corrupt, kOffsetFlags, static_cast<uint16_t>(0x0080u));
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));

    // bit 8-15 是 must-ignore 的保留位，必须被忽略而不是报错。
    corrupt = SampleFrame();
    PutU16At(corrupt, kOffsetFlags, static_cast<uint16_t>(kFlagHasErrors | 0x0100u));
    ASSERT_TRUE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error)) << error;
    EXPECT_NE(decoded.flags & kFlagHasErrors, 0);
}

TEST(WireProtocolTest, AppliesMustIgnoreRuleToUnknownTypes)
{
    std::string error;
    Frame decoded{};

    // 最高位为 0 的未知类型 → 协议错误。
    auto corrupt = SampleFrame();
    PutU16At(corrupt, kOffsetType, static_cast<uint16_t>(0x0055u));
    EXPECT_FALSE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error));

    // 最高位为 1 的未知类型 → 正常解出，交由上层跳过。
    corrupt = SampleFrame();
    PutU16At(corrupt, kOffsetType, static_cast<uint16_t>(0x80abu));
    ASSERT_TRUE(DecodeFrame(corrupt.data(), corrupt.size(), decoded, error)) << error;
    EXPECT_TRUE(IsMustIgnoreType(static_cast<uint16_t>(decoded.type)));

    // 我们自己定义的实时诊断类型也落在 must-ignore 区间。
    EXPECT_TRUE(IsMustIgnoreType(static_cast<uint16_t>(MessageType::DIAGNOSTIC_STREAM)));
    EXPECT_FALSE(IsMustIgnoreType(static_cast<uint16_t>(MessageType::READY)));
}

} // namespace
} // namespace npu::sanitizer::ipc
