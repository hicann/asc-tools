// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#include "acl_hook.h"

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

using GenericAclFunction = int (*)(void);

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

struct InjectionState {
    bool failDataRestore = false;
    std::vector<std::string> setNames;
    GenericAclFunction dataOrigin = nullptr;
    GenericAclFunction dataCurrent = nullptr;
    GenericAclFunction dataPrevious = nullptr;
};

InjectionState* g_injectionState = nullptr;

extern "C" aclError aclrtApiInjectionGetFunc(const char* name, GenericAclFunction* origin, GenericAclFunction* current)
{
    if (g_injectionState == nullptr || name == nullptr || origin == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    const std::string api(name);
    if (api == "aclrtBinaryLoadFromData") {
        *origin = g_injectionState->dataOrigin;
        if (current != nullptr)
            *current = g_injectionState->dataCurrent;
    } else {
        return ACL_ERROR_INVALID_PARAM;
    }
    return ACL_SUCCESS;
}

extern "C" aclError aclrtApiInjectionSetFunc(const char* name, GenericAclFunction function)
{
    if (g_injectionState == nullptr || name == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    g_injectionState->setNames.emplace_back(name);
    if (g_injectionState->setNames.back() != "aclrtBinaryLoadFromData") {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (g_injectionState->failDataRestore && function == g_injectionState->dataPrevious) {
        return ACL_ERROR_FAILURE;
    }
    g_injectionState->dataCurrent = function;
    return ACL_SUCCESS;
}

struct TestState {
    std::vector<uint8_t> loadedData;
    std::string inputContents;
    std::vector<std::string> workDirectories;
    int originalCalls = 0;
    int runnerCalls = 0;
    bool patchSucceeds = true;
    bool recurse = false;
    AclHookConfig config;
};

TestState* g_state = nullptr;

TEST(DefaultHookConfigTest, ReadsRuntimeCompilationEnvironment)
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

    const AclHookConfig config = DefaultHookConfig();

    EXPECT_EQ(config.arch, "dav-3510");
    EXPECT_EQ(config.argSize, 24U);
    EXPECT_EQ(config.workDirectory, "/tmp/npu-check-work");
    EXPECT_EQ(config.cacheDirectory, "/tmp/npu-check-cache");
    EXPECT_TRUE(config.strict);
    EXPECT_TRUE(config.keepTemp);
    EXPECT_EQ(config.compilerArgs, (std::vector<std::string>{"-g", "-DVALUE=a b"}));
}

TEST(DefaultHookConfigTest, DoesNotEnableProbeGroupsWithoutExplicitSelection)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_PROBE_SET"});
    ASSERT_EQ(unsetenv("NPU_CHECK_DBI_PROBE_SET"), 0);

    EXPECT_TRUE(DefaultHookConfig().probeGroups.empty());
}

TEST(DefaultHookConfigTest, SelectsProbeGroupsFromEnvironmentOrActiveMask)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_PROBE_SET"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_PROBE_SET", "mte2,sync", 1), 0);

    EXPECT_EQ(DefaultHookConfig().probeGroups, (std::vector<ProbeGroup>{ProbeGroup::Mte2, ProbeGroup::Sync}));
    EXPECT_EQ(
        DefaultHookConfig(PROBE_GROUP_MTE1 | PROBE_GROUP_FIXPIPE).probeGroups,
        (std::vector<ProbeGroup>{ProbeGroup::Mte1, ProbeGroup::Fixpipe}));
}

TEST(DefaultHookConfigTest, TreatsOnlyExactOneAsTrue)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_STRICT", "NPU_CHECK_DBI_KEEP_TEMP"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_STRICT", "true", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_KEEP_TEMP", "01", 1), 0);

    const AclHookConfig config = DefaultHookConfig();

    EXPECT_FALSE(config.strict);
    EXPECT_FALSE(config.keepTemp);
}

TEST(DefaultHookConfigTest, RejectsAllCompilerArgumentsWhenOneIsMissing)
{
    EnvironmentRestore environment(
        {"NPU_CHECK_DBI_COMPILER_ARG_COUNT", "NPU_CHECK_DBI_COMPILER_ARG_0", "NPU_CHECK_DBI_COMPILER_ARG_1"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "2", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_0", "", 1), 0);
    ASSERT_EQ(unsetenv("NPU_CHECK_DBI_COMPILER_ARG_1"), 0);

    const AclHookConfig config = DefaultHookConfig();

    EXPECT_TRUE(config.compilerArgs.empty());
}

TEST(DefaultHookConfigTest, IgnoresMalformedOrOversizedCompilerArgumentCount)
{
    EnvironmentRestore environment({"NPU_CHECK_DBI_COMPILER_ARG_COUNT", "NPU_CHECK_DBI_COMPILER_ARG_0"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_0", "-g", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "not-a-count", 1), 0);
    EXPECT_TRUE(DefaultHookConfig().compilerArgs.empty());

    ASSERT_EQ(setenv("NPU_CHECK_DBI_COMPILER_ARG_COUNT", "129", 1), 0);
    EXPECT_TRUE(DefaultHookConfig().compilerArgs.empty());
}

TEST(DefaultHookConfigTest, AcceptsExactlyMaximumCompilerArgumentCount)
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

    const AclHookConfig config = DefaultHookConfig();

    EXPECT_EQ(config.compilerArgs, expected);
}

aclError OriginalData(const void* data, size_t length, const aclrtBinaryLoadOptions*, aclrtBinHandle*)
{
    ++g_state->originalCalls;
    const auto* bytes = static_cast<const uint8_t*>(data);
    g_state->loadedData.assign(bytes, bytes + length);
    return ACL_SUCCESS;
}

aclError FailingOriginalData(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* handle)
{
    (void)OriginalData(data, length, options, handle);
    return ACL_ERROR_FAILURE;
}

aclError PreviousData(const void*, size_t, const aclrtBinaryLoadOptions*, aclrtBinHandle*) { return ACL_ERROR_FAILURE; }

DbiResult FakePatch(const DbiRequest& request, void* userdata)
{
    auto& state = *static_cast<TestState*>(userdata);
    ++state.runnerCalls;
    state.workDirectories.push_back(request.workDirectory);
    std::ifstream input(request.inputKernel, std::ios::binary);
    state.inputContents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (state.recurse) {
        const std::string recursiveInput = "recursive-data";
        (void)HandleBinaryLoadFromData(
            state.config, recursiveInput.data(), recursiveInput.size(), nullptr, nullptr, &OriginalData, &FakePatch,
            userdata);
    }
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
    state.inputContents = request.workDirectory;
    std::filesystem::create_directories(request.workDirectory);
    throw std::runtime_error("patch failed unexpectedly");
}

class AclHookTest : public testing::Test {
protected:
    void SetUp() override
    {
        char pattern[] = "/tmp/npu_check_acl_hook_XXXXXX";
        char* created = mkdtemp(pattern);
        ASSERT_NE(created, nullptr);
        directory_ = created;
        state_.config.arch = "dav-c220-vec";
        state_.config.argSize = 64;
        state_.config.probeGroups = {ProbeGroup::Mte2};
        state_.config.workDirectory = directory_;
        state_.config.cacheDirectory = directory_ + "/cache";
        state_.config.keepTemp = true;
        g_state = &state_;
    }

    void TearDown() override
    {
        g_state = nullptr;
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    TestState state_;
    std::string directory_;
};

TEST_F(AclHookTest, LoadsPatchedData)
{
    const std::string original = "kernel-data";
    bool loadedPatched = false;

    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &FakePatch, &state_,
            &loadedPatched),
        ACL_SUCCESS);
    EXPECT_TRUE(loadedPatched);
    EXPECT_EQ(state_.runnerCalls, 1);
    EXPECT_EQ(state_.inputContents, original);
    EXPECT_EQ(std::string(state_.loadedData.begin(), state_.loadedData.end()), "patched");
}

TEST_F(AclHookTest, KeepTempRetainsRequestDirectoryAndPatchedOutput)
{
    const std::string original = "kernel-data";

    ASSERT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &FakePatch, &state_),
        ACL_SUCCESS);
    ASSERT_EQ(state_.workDirectories.size(), 1U);
    EXPECT_TRUE(std::filesystem::is_directory(state_.workDirectories.front()));
    EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(state_.workDirectories.front()) / "patched.o"));
}

TEST_F(AclHookTest, CleanupRemovesRequestDirectoryButPreservesExternalCache)
{
    state_.config.keepTemp = false;
    const std::string original = "kernel-data";
    const std::filesystem::path cacheMarker = std::filesystem::path(state_.config.cacheDirectory) / "cache-marker";
    std::filesystem::create_directories(cacheMarker.parent_path());
    std::ofstream(cacheMarker) << "cached";

    ASSERT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &FakePatch, &state_),
        ACL_SUCCESS);
    ASSERT_EQ(state_.workDirectories.size(), 1U);
    EXPECT_FALSE(std::filesystem::exists(state_.workDirectories.front()));
    EXPECT_TRUE(std::filesystem::is_regular_file(cacheMarker));
}

TEST_F(AclHookTest, NonStrictFailureFallsBackToOriginalInput)
{
    state_.patchSucceeds = false;
    const std::string original = "kernel-data";
    bool loadedPatched = true;

    testing::internal::CaptureStderr();
    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &FakePatch, &state_,
            &loadedPatched),
        ACL_SUCCESS);
    const std::string diagnostic = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(state_.loadedData.begin(), state_.loadedData.end()), original);
    EXPECT_FALSE(loadedPatched);
    EXPECT_NE(diagnostic.find("npu_check: DBI patch failed at fake: failed"), std::string::npos);
}

TEST_F(AclHookTest, StrictFailureDoesNotLoadOriginalInput)
{
    state_.patchSucceeds = false;
    state_.config.strict = true;
    const std::string original = "kernel-data";
    bool loadedPatched = true;

    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &FakePatch, &state_,
            &loadedPatched),
        ACL_ERROR_FAILURE);
    EXPECT_FALSE(loadedPatched);
    EXPECT_EQ(state_.originalCalls, 0);
}

TEST_F(AclHookTest, PatchedLoaderFailureIsNotReportedAsPatchedLoad)
{
    const std::string original = "kernel-data";
    bool loadedPatched = true;

    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &FailingOriginalData, &FakePatch,
            &state_, &loadedPatched),
        ACL_ERROR_FAILURE);
    EXPECT_FALSE(loadedPatched);
}

TEST_F(AclHookTest, StrictExceptionDoesNotLoadOriginalDataAndCleansTemporaryDirectory)
{
    state_.config.strict = true;
    state_.config.keepTemp = false;
    const std::string original = "kernel-data";

    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &ThrowingPatch, &state_),
        ACL_ERROR_FAILURE);
    EXPECT_EQ(state_.originalCalls, 0);
    EXPECT_FALSE(state_.inputContents.empty());
    EXPECT_FALSE(std::filesystem::exists(state_.inputContents));
}

TEST_F(AclHookTest, NonStrictExceptionLoadsOriginalDataAndCleansTemporaryDirectory)
{
    state_.config.keepTemp = false;
    const std::string original = "kernel-data";

    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &ThrowingPatch, &state_),
        ACL_SUCCESS);
    EXPECT_EQ(state_.originalCalls, 1);
    EXPECT_EQ(std::string(state_.loadedData.begin(), state_.loadedData.end()), original);
    EXPECT_FALSE(state_.inputContents.empty());
    EXPECT_FALSE(std::filesystem::exists(state_.inputContents));
}

TEST_F(AclHookTest, RecursiveLoadBypassesPatching)
{
    state_.recurse = true;
    const std::string original = "kernel-data";

    EXPECT_EQ(
        HandleBinaryLoadFromData(
            state_.config, original.data(), original.size(), nullptr, nullptr, &OriginalData, &FakePatch, &state_),
        ACL_SUCCESS);
    EXPECT_EQ(state_.runnerCalls, 1);
    EXPECT_EQ(state_.originalCalls, 2);
}

TEST_F(AclHookTest, RestoresPreviousHooksInsteadOfBypassingThem)
{
    InjectionState injection{};
    injection.dataOrigin = reinterpret_cast<GenericAclFunction>(&OriginalData);
    injection.dataPrevious = reinterpret_cast<GenericAclFunction>(&PreviousData);
    injection.dataCurrent = injection.dataPrevious;
    g_injectionState = &injection;

    std::string error;
    ASSERT_TRUE(InstallAclHooks(state_.config, error)) << error;
    EXPECT_NE(injection.dataCurrent, injection.dataPrevious);

    UninstallAclHooks();
    EXPECT_EQ(injection.dataCurrent, injection.dataPrevious);
    g_injectionState = nullptr;
}

TEST_F(AclHookTest, RetriesRestorationAfterInjectionSpiFailure)
{
    InjectionState injection{};
    injection.dataOrigin = reinterpret_cast<GenericAclFunction>(&OriginalData);
    injection.dataPrevious = injection.dataOrigin;
    injection.dataCurrent = injection.dataPrevious;
    g_injectionState = &injection;

    std::string error;
    ASSERT_TRUE(InstallAclHooks(state_.config, error)) << error;
    injection.failDataRestore = true;
    testing::internal::CaptureStderr();
    UninstallAclHooks();
    EXPECT_NE(testing::internal::GetCapturedStderr().find("failed to restore"), std::string::npos);
    EXPECT_NE(injection.dataCurrent, injection.dataPrevious);

    injection.failDataRestore = false;
    UninstallAclHooks();
    EXPECT_EQ(injection.dataCurrent, injection.dataPrevious);
    g_injectionState = nullptr;
}

TEST_F(AclHookTest, RejectsIncompleteInstallOptionsBeforeUsingInjectionSpi)
{
    AclHookInstallOptions options{};
    options.arch = "";
    options.argSize = 64;
    options.probeGroupMask = PROBE_GROUP_MTE2;
    char error[256]{};

    EXPECT_EQ(NpuCheckInstallAclHooks(&options, error, sizeof(error)), ACL_ERROR_INVALID_PARAM);
    EXPECT_STREQ(error, "ACL DBI hook requires architecture, argument size, and a Probe group");

    options.arch = "dav-c220";
    options.probeGroupMask = 0;
    error[0] = '\0';
    EXPECT_EQ(NpuCheckInstallAclHooks(&options, error, sizeof(error)), ACL_ERROR_INVALID_PARAM);
    EXPECT_STREQ(error, "ACL DBI hook requires architecture, argument size, and a Probe group");
}

TEST_F(AclHookTest, RejectsMissingCompilerArgumentArray)
{
    AclHookInstallOptions options{};
    options.arch = "dav-c220";
    options.argSize = 64;
    options.probeGroupMask = PROBE_GROUP_MTE2;
    options.compilerArgCount = 1;
    char error[256]{};

    EXPECT_EQ(NpuCheckInstallAclHooks(&options, error, sizeof(error)), ACL_ERROR_INVALID_PARAM);
    EXPECT_STREQ(error, "compiler argument array is null");
}

} // namespace
} // namespace aclsan
