// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "options.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace npu::sanitizer::cli {
namespace {

struct ParseResult {
    bool ok = false;
    Options options{};
    std::string error;
};

ParseResult Parse(std::vector<std::string> arguments)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }

    ParseResult result{};
    result.ok = ParseOptions(static_cast<int>(argv.size()), argv.data(), result.options, result.error);
    return result;
}

class TemporaryFile {
public:
    TemporaryFile()
    {
        char pattern[] = "/tmp/npu_check_cli_test_XXXXXX";
        const int fd = mkstemp(pattern);
        EXPECT_NE(fd, -1);
        if (fd >= 0) {
            (void)close(fd);
            path_ = pattern;
        }
    }

    ~TemporaryFile()
    {
        if (!path_.empty()) {
            std::error_code error;
            std::filesystem::remove(path_, error);
        }
    }

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

TEST(OptionsTest, AcceptsHelpWithoutApplication)
{
    const auto result = Parse({"npu_check", "--help"});

    ASSERT_TRUE(result.ok);
    EXPECT_TRUE(result.options.showHelp);
    EXPECT_TRUE(result.options.application.empty());
}

TEST(OptionsTest, ParsesMemcheckInvocation)
{
    const auto result = Parse({
        "npu_check",   "--tool",
        "memcheck",    "--strict",
        "--keep-temp", "--log-file",
        "report.log",  "--work-dir",
        "work",        "--probe-cache-dir",
        "cache",       "--compile-option",
        "-g",          "--compile-option",
        "-O2",         "--handshake-timeout-ms",
        "250",         "--",
        "./sample",    "--size",
        "64",
    });

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.options.toolConfig.toolName, "memcheck");
    EXPECT_TRUE(result.options.toolConfig.strict);
    EXPECT_TRUE(result.options.toolConfig.keepTemp);
    EXPECT_EQ(result.options.handshakeTimeoutMs, 250);
    EXPECT_TRUE(std::filesystem::path(result.options.toolConfig.logFile).is_absolute());
    EXPECT_TRUE(std::filesystem::path(result.options.toolConfig.workDir).is_absolute());
    EXPECT_TRUE(std::filesystem::path(result.options.toolConfig.probeCacheDir).is_absolute());
    EXPECT_EQ(result.options.toolConfig.compileOptions, (std::vector<std::string>{"-g", "-O2"}));
    EXPECT_EQ(result.options.application, (std::vector<std::string>{"./sample", "--size", "64"}));
}

TEST(OptionsTest, AppliesLastBooleanOption)
{
    const auto result = Parse({
        "npu_check",
        "--tool",
        "memcheck",
        "--strict",
        "--no-strict",
        "--keep-temp",
        "--no-keep-temp",
        "--",
        "./sample",
    });

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_FALSE(result.options.toolConfig.strict);
    EXPECT_FALSE(result.options.toolConfig.keepTemp);
}

TEST(OptionsTest, RejectsInvalidInvocation)
{
    const auto missingApplication = Parse({"npu_check", "--tool", "memcheck"});
    EXPECT_FALSE(missingApplication.ok);
    EXPECT_EQ(missingApplication.error, "expected -- followed by an application command");

    const auto unsupportedTool = Parse({"npu_check", "--tool", "trace", "--", "./sample"});
    EXPECT_FALSE(unsupportedTool.ok);
    EXPECT_EQ(unsupportedTool.error, "unsupported tool 'trace'; current implementation supports memcheck");

    const auto invalidTimeout =
        Parse({"npu_check", "--tool", "memcheck", "--handshake-timeout-ms", "99", "--", "./sample"});
    EXPECT_FALSE(invalidTimeout.ok);
    EXPECT_EQ(invalidTimeout.error, "--handshake-timeout-ms must be in [100, 120000]");

    const auto unknownOption = Parse({"npu_check", "--tool", "memcheck", "--unknown", "--", "./sample"});
    EXPECT_FALSE(unknownOption.ok);
    EXPECT_EQ(unknownOption.error, "unknown option: --unknown");
}

TEST(OptionsTest, RejectsTooManyCompileOptions)
{
    std::vector<std::string> arguments{"npu_check", "--tool", "memcheck"};
    for (size_t index = 0; index <= ipc::kMaxCompileOptions; ++index) {
        arguments.push_back("--compile-option");
        arguments.push_back("-DTEST=" + std::to_string(index));
    }
    arguments.push_back("--");
    arguments.push_back("./sample");

    const auto result = Parse(std::move(arguments));

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, "too many --compile-option values");
}

TEST(OptionsTest, ResolvesExplicitRegularFile)
{
    TemporaryFile library;
    ASSERT_FALSE(library.Path().empty());

    std::string resolved;
    std::string error;
    ASSERT_TRUE(ResolveLibraryPath(library.Path().string(), resolved, error)) << error;

    EXPECT_EQ(resolved, std::filesystem::canonical(library.Path()).string());
}

TEST(OptionsTest, RejectsMissingLibrary)
{
    std::string resolved;
    std::string error;

    EXPECT_FALSE(ResolveLibraryPath("/tmp/npu_check_missing_library.so", resolved, error));
    EXPECT_EQ(error, "cannot locate libnpu_check.so; use --library or NPU_CHECK_LIBRARY_PATH");
}

TEST(OptionsTest, DocumentsRequiredArguments)
{
    const std::string usage = Usage();

    EXPECT_NE(usage.find("Usage: npu_check --tool memcheck"), std::string::npos);
    EXPECT_NE(usage.find("--handshake-timeout-ms N"), std::string::npos);
}

} // namespace
} // namespace npu::sanitizer::cli
