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
    hello.nonce = "001122";
    HelloPayload decodedHello{};
    std::string error;
    ASSERT_TRUE(DecodeHello(EncodeHello(hello), decodedHello, error)) << error;
    EXPECT_EQ(decodedHello.pid, hello.pid);
    EXPECT_EQ(decodedHello.uid, hello.uid);
    EXPECT_EQ(decodedHello.nonce, hello.nonce);

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

} // namespace
} // namespace npu::sanitizer::ipc
