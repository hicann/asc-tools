// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#include "binary_instrumenter.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aclsan {
namespace {

class EnvironmentRestore {
public:
    explicit EnvironmentRestore(std::vector<std::string> names)
    {
        variables_.reserve(names.size());
        for (auto& name : names) {
            const char* value = std::getenv(name.c_str());
            variables_.emplace_back(
                std::move(name), value == nullptr ? std::nullopt : std::optional<std::string>(value));
        }
    }

    ~EnvironmentRestore()
    {
        for (const auto& [name, value] : variables_) {
            if (value.has_value()) {
                (void)setenv(name.c_str(), value->c_str(), 1);
            } else {
                (void)unsetenv(name.c_str());
            }
        }
    }

    EnvironmentRestore(const EnvironmentRestore&) = delete;
    EnvironmentRestore& operator=(const EnvironmentRestore&) = delete;

private:
    std::vector<std::pair<std::string, std::optional<std::string>>> variables_;
};

struct TestState {
    std::string inputContents;
    std::vector<std::string> workDirectories;
    int runnerCalls = 0;
    bool patchSucceeds = true;
    BinaryInstrumentationConfig config;
};

int32_t CaptureInstrumentedBinary(const void* data, size_t length, void* userdata)
{
    auto& captured = *static_cast<std::vector<uint8_t>*>(userdata);
    const auto* bytes = static_cast<const uint8_t*>(data);
    captured.assign(bytes, bytes + length);
    return 73;
}

TEST(DefaultBinaryInstrumentationConfigTest, ReadsRuntimeCompilationEnvironment)
{
    EnvironmentRestore environment(
        {"NPU_CHECK_DBI_ARCH", "NPU_CHECK_DBI_ARG_SIZE", "NPU_CHECK_DBI_WORK_DIR", "NPU_CHECK_DBI_CACHE_DIR",
         "NPU_CHECK_DBI_STRICT", "NPU_CHECK_DBI_KEEP_TEMP", "NPU_CHECK_DBI_COMPILER_ARG_COUNT",
         "NPU_CHECK_DBI_COMPILER_ARG_0", "NPU_CHECK_DBI_COMPILER_ARG_1"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_ARCH", "dav-3510", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_ARG_SIZE", "24", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_WORK_DIR", "/tmp/npu-check-work", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_CACHE_DIR", "/tmp/npu-check-cache", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_STRICT", "1", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_KEEP_TEMP", "1", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "2", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_0", "-g", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_1", "-DVALUE=a b", 1), 0);

    const BinaryInstrumentationConfig config = DefaultBinaryInstrumentationConfig();

    EXPECT_EQ(config.arch, "dav-3510");
    EXPECT_EQ(config.argSize, 24U);
    EXPECT_EQ(config.workDirectory, "/tmp/npu-check-work");
    EXPECT_EQ(config.cacheDirectory, "/tmp/npu-check-cache");
    EXPECT_TRUE(config.strict);
    EXPECT_TRUE(config.keepTemp);
    EXPECT_EQ(config.compilerArgs, (std::vector<std::string>{"-g", "-DVALUE=a b"}));
}

TEST(DefaultBinaryInstrumentationConfigTest, SelectsProbeGroupsFromEnvironmentOrActiveMask)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_PROBE_SET"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_PROBE_SET", "mte2,sync,register", 1), 0);

    EXPECT_EQ(
        DefaultBinaryInstrumentationConfig().probeGroups,
        (std::vector<ProbeGroup>{ProbeGroup::Mte2, ProbeGroup::Sync, ProbeGroup::Register}));
    EXPECT_EQ(
        DefaultBinaryInstrumentationConfig(PROBE_GROUP_MTE1 | PROBE_GROUP_FIXPIPE | PROBE_GROUP_REGISTER).probeGroups,
        (std::vector<ProbeGroup>{ProbeGroup::Mte1, ProbeGroup::Fixpipe, ProbeGroup::Register}));
}

TEST(DefaultBinaryInstrumentationConfigTest, DoesNotEnableProbeGroupsWithoutExplicitSelection)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_PROBE_SET"});
    ASSERT_EQ(unsetenv("NPU_CHECK_DBI_PROBE_SET"), 0);

    EXPECT_TRUE(DefaultBinaryInstrumentationConfig().probeGroups.empty());
}

TEST(DefaultBinaryInstrumentationConfigTest, TreatsOnlyExactOneAsTrue)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_STRICT", "NPU_CHECK_DBI_KEEP_TEMP"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_STRICT", "true", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_KEEP_TEMP", "01", 1), 0);

    const BinaryInstrumentationConfig config = DefaultBinaryInstrumentationConfig();

    EXPECT_FALSE(config.strict);
    EXPECT_FALSE(config.keepTemp);
}

TEST(DefaultBinaryInstrumentationConfigTest, RejectsAllCompilerArgumentsWhenOneIsMissing)
{
    EnvironmentRestore environment(
        {"NPU_CHECK_DBI_COMPILER_ARG_COUNT", "NPU_CHECK_DBI_COMPILER_ARG_0", "NPU_CHECK_DBI_COMPILER_ARG_1"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "2", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_0", "", 1), 0);
    ASSERT_EQ(unsetenv("NPU_CHECK_DBI_COMPILER_ARG_1"), 0);

    EXPECT_TRUE(DefaultBinaryInstrumentationConfig().compilerArgs.empty());
}

TEST(DefaultBinaryInstrumentationConfigTest, IgnoresMalformedOrOversizedCompilerArgumentCount)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_COMPILER_ARG_COUNT", "NPU_CHECK_DBI_COMPILER_ARG_0"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_0", "-g", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "not-a-count", 1), 0);
    EXPECT_TRUE(DefaultBinaryInstrumentationConfig().compilerArgs.empty());

    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "129", 1), 0);
    EXPECT_TRUE(DefaultBinaryInstrumentationConfig().compilerArgs.empty());
}

TEST(DefaultBinaryInstrumentationConfigTest, AcceptsExactlyMaximumCompilerArgumentCount)
{
    constexpr std::size_t kCompilerArgCount = 128;
    std::vector<std::string> names{"NPU_CHECK_DBI_COMPILER_ARG_COUNT"};
    std::vector<std::string> expected;
    names.reserve(kCompilerArgCount + 1);
    expected.reserve(kCompilerArgCount);
    for (std::size_t index = 0; index < kCompilerArgCount; ++index) {
        names.push_back("NPU_CHECK_DBI_COMPILER_ARG_" + std::to_string(index));
        expected.push_back(index == 0 ? "" : "-DVALUE=" + std::to_string(index));
    }
    EnvironmentRestore environment(names);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "128", 1), 0);
    for (std::size_t index = 0; index < kCompilerArgCount; ++index) {
        ASSERT_EQ(setenv(names[index + 1].c_str(), expected[index].c_str(), 1), 0);
    }

    EXPECT_EQ(DefaultBinaryInstrumentationConfig().compilerArgs, expected);
}

DbiResult FakePatch(const DbiRequest& request, void* userdata)
{
    auto& state = *static_cast<TestState*>(userdata);
    ++state.runnerCalls;
    state.workDirectories.push_back(request.workDirectory);
    std::ifstream input(request.inputKernel, std::ios::binary);
    state.inputContents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (!state.patchSucceeds) {
        return {false, {}, "fake", "failed"};
    }
    std::filesystem::create_directories(std::filesystem::path(request.outputKernel).parent_path());
    std::ofstream output(request.outputKernel, std::ios::binary | std::ios::trunc);
    output << "patched";
    return {output.good(), request.outputKernel, output.good() ? "complete" : "write", {}};
}

DbiResult ThrowingPatch(const DbiRequest& request, void* userdata)
{
    auto& state = *static_cast<TestState*>(userdata);
    ++state.runnerCalls;
    state.workDirectories.push_back(request.workDirectory);
    std::filesystem::create_directories(request.workDirectory);
    throw std::runtime_error("patch failed unexpectedly");
}

class BinaryInstrumenterTest : public testing::Test {
protected:
    void SetUp() override
    {
        char pattern[] = "/tmp/npu_check_binary_instrumenter_XXXXXX";
        char* created = mkdtemp(pattern);
        ASSERT_NE(created, nullptr);
        directory_ = created;
        state_.config.arch = "dav-c220-vec";
        state_.config.argSize = 64;
        state_.config.probeGroups = {ProbeGroup::Mte2};
        state_.config.workDirectory = directory_;
        state_.config.cacheDirectory = directory_ + "/cache";
        state_.config.keepTemp = true;
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    TestState state_;
    std::string directory_;
};

TEST_F(BinaryInstrumenterTest, ReturnsPatchedBytes)
{
    const std::string original = "kernel-data";

    const BinaryInstrumentationResult result =
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Instrumented);
    EXPECT_EQ(state_.runnerCalls, 1);
    EXPECT_EQ(state_.inputContents, original);
    EXPECT_EQ(std::string(result.binary.begin(), result.binary.end()), "patched");
}

TEST_F(BinaryInstrumenterTest, SkipsIncompleteConfiguration)
{
    state_.config.arch.clear();
    const std::string original = "kernel-data";

    const BinaryInstrumentationResult result =
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Skipped);
    EXPECT_EQ(state_.runnerCalls, 0);
}

TEST_F(BinaryInstrumenterTest, ReportsPipelineFailure)
{
    state_.patchSucceeds = false;
    const std::string original = "kernel-data";

    const BinaryInstrumentationResult result =
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Failed);
    EXPECT_EQ(result.stage, "fake");
    EXPECT_EQ(result.diagnostic, "failed");
    EXPECT_TRUE(result.binary.empty());
}

TEST_F(BinaryInstrumenterTest, KeepTempRetainsRequestDirectoryAndPatchedOutput)
{
    const std::string original = "kernel-data";

    ASSERT_EQ(
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_).status,
        BinaryInstrumentationStatus::Instrumented);

    ASSERT_EQ(state_.workDirectories.size(), 1U);
    EXPECT_TRUE(std::filesystem::is_directory(state_.workDirectories.front()));
    EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(state_.workDirectories.front()) / "patched.o"));
}

TEST_F(BinaryInstrumenterTest, CleanupRemovesRequestDirectoryButPreservesExternalCache)
{
    state_.config.keepTemp = false;
    const std::string original = "kernel-data";
    const std::filesystem::path cacheMarker = std::filesystem::path(state_.config.cacheDirectory) / "cache-marker";
    std::filesystem::create_directories(cacheMarker.parent_path());
    std::ofstream(cacheMarker) << "cached";

    ASSERT_EQ(
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_).status,
        BinaryInstrumentationStatus::Instrumented);

    ASSERT_EQ(state_.workDirectories.size(), 1U);
    EXPECT_FALSE(std::filesystem::exists(state_.workDirectories.front()));
    EXPECT_TRUE(std::filesystem::is_regular_file(cacheMarker));
}

TEST_F(BinaryInstrumenterTest, ExceptionReturnsFailureAndCleansTemporaryDirectory)
{
    state_.config.keepTemp = false;
    const std::string original = "kernel-data";

    const BinaryInstrumentationResult result =
        InstrumentBinary(state_.config, original.data(), original.size(), &ThrowingPatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Failed);
    EXPECT_EQ(result.stage, "exception");
    ASSERT_EQ(state_.workDirectories.size(), 1U);
    EXPECT_FALSE(std::filesystem::exists(state_.workDirectories.front()));
}

TEST_F(BinaryInstrumenterTest, RuntimeFacadeConsumesPatchedBytesAcrossAnAbiStableBoundary)
{
    EnvironmentRestore environment(
        {"NPU_CHECK_DBI_ARCH", "NPU_CHECK_DBI_ARG_SIZE", "NPU_CHECK_DBI_PROBE_SET", "NPU_CHECK_DBI_STRICT"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_ARCH", "dav-c220-vec", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_ARG_SIZE", "64", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_STRICT", "1", 1), 0);
    const std::string original = "kernel-data";
    std::vector<uint8_t> consumed;

    const RuntimeBinaryInstrumentationResult result = InstrumentRuntimeBinary(
        original.data(), original.size(), PROBE_GROUP_MTE2, &CaptureInstrumentedBinary, &consumed, &FakePatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Instrumented);
    EXPECT_EQ(result.strict, 1U);
    EXPECT_EQ(result.consumerStatus, 73);
    EXPECT_EQ(std::string(consumed.begin(), consumed.end()), "patched");
}

} // namespace
} // namespace aclsan
