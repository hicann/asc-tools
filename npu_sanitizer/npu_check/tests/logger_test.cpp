// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace npu::sanitizer::logging {
namespace {

std::filesystem::path TemporaryLogPath(const char* suffix)
{
    return std::filesystem::temp_directory_path() /
           ("npu_check_logger_" + std::to_string(getpid()) + "_" + suffix + ".log");
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

TEST(LoggerTest, WritesAllLevelsAndSynchronouslyNotifiesErrors)
{
    const auto path = TemporaryLogPath("levels");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    std::string observedError;
    {
        Logger logger;
        std::string error;
        ASSERT_TRUE(logger.Open(path.string(), LogLevel::DEBUG, error)) << error;
        logger.SetErrorSink([&observedError](const std::string& message) { observedError = message; });
        logger.Debug("cbdata count=1");
        logger.Info("memory alloc address=0x1000");
        logger.Warning("callback payload is incomplete");
        logger.Error("UDS flow failed");

        EXPECT_EQ(observedError, "UDS flow failed");
        EXPECT_EQ(logger.Path(), path.string());
    }

    const std::string content = ReadFile(path);
    const std::regex debugFormat(
        R"(\[DEBUG\]NPU_CHECK\(pid:[0-9]+\):[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}\.[0-9]{3} \[logger_test\.cpp:[0-9]+\] cbdata count=1)");
    EXPECT_TRUE(std::regex_search(content, debugFormat));
    EXPECT_NE(content.find("[INFO]NPU_CHECK(pid:"), std::string::npos);
    EXPECT_NE(content.find("] memory alloc address=0x1000"), std::string::npos);
    EXPECT_NE(content.find("[WARNING]NPU_CHECK(pid:"), std::string::npos);
    EXPECT_NE(content.find("] callback payload is incomplete"), std::string::npos);
    EXPECT_NE(content.find("[ERROR]NPU_CHECK(pid:"), std::string::npos);
    EXPECT_NE(content.find("] UDS flow failed"), std::string::npos);
    std::filesystem::remove(path, ignored);
}

TEST(LoggerTest, AppliesMinimumLogLevel)
{
    const auto path = TemporaryLogPath("filter");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    {
        Logger logger;
        std::string error;
        ASSERT_TRUE(logger.Open(path.string(), LogLevel::WARNING, error)) << error;
        logger.Debug("hidden debug");
        logger.Info("hidden info");
        logger.Warning("visible warning");
    }

    const std::string content = ReadFile(path);
    EXPECT_EQ(content.find("hidden debug"), std::string::npos);
    EXPECT_EQ(content.find("hidden info"), std::string::npos);
    EXPECT_NE(content.find("visible warning"), std::string::npos);
    std::filesystem::remove(path, ignored);
}

} // namespace
} // namespace npu::sanitizer::logging
