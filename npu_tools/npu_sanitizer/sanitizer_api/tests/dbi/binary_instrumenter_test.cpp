// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#include "dbi/binary_instrumenter.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <elf.h>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

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

TEST(DefaultBinaryInstrumentationConfigTest, BuildsRuntimeConfigWithoutDbiEnvironment)
{
    EnvironmentRestore environment(
        {"NPU_CHECK_DBI_ARCH", "NPU_CHECK_DBI_TOOLCHAIN_ROOT", "NPU_CHECK_DBI_WORK_DIR", "NPU_CHECK_DBI_CACHE_DIR",
         "NPU_CHECK_DBI_STRICT", "NPU_CHECK_DBI_KEEP_TEMP", "NPU_CHECK_DBI_PROBE_SET"});
    ASSERT_EQ(setenv("NPU_CHECK_DBI_ARCH", "forged", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_TOOLCHAIN_ROOT", "/forged", 1), 0);
    ASSERT_EQ(setenv("NPU_CHECK_DBI_PROBE_SET", "sync", 1), 0);

    const auto cannRoot = boost::filesystem::temp_directory_path() / "dbi-runtime-config-test";
    const auto tools = cannRoot / "tools/bisheng_compiler/bin";
    const auto runtime = cannRoot / "x86_64-linux/lib64/libacl_rt.so";
    boost::filesystem::remove_all(cannRoot);
    boost::filesystem::create_directories(tools);
    boost::filesystem::create_directories(runtime.parent_path());
    std::ofstream(runtime).put('\n');
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        std::ofstream(tools / name).put('\n');
    }

    BinaryInstrumentationConfig config;
    std::string diagnostic;
    ASSERT_TRUE(
        BuildRuntimeInstrumentationConfig("Ascend950PR_9599", runtime.c_str(), PROBE_GROUP_MTE2, config, diagnostic))
        << diagnostic;
    EXPECT_EQ(config.arch, "dav-3510");
    EXPECT_EQ(config.toolchainRoot, cannRoot.string());
    EXPECT_EQ(config.probeGroups, (std::vector<ProbeGroup>{ProbeGroup::Mte2, ProbeGroup::Scalar}));
    EXPECT_TRUE(config.strict);
    EXPECT_FALSE(config.keepTemp);
    const std::string root = "/tmp/npu-check-" + std::to_string(static_cast<unsigned long long>(geteuid()));
    EXPECT_EQ(config.workDirectory, root + "/requests");
    EXPECT_EQ(config.cacheDirectory, root + "/cache");
    boost::filesystem::remove_all(cannRoot);
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
    boost::filesystem::create_directories(boost::filesystem::path(request.outputKernel).parent_path());
    std::ofstream output(request.outputKernel, std::ios::binary | std::ios::trunc);
    output << "patched";
    return {output.good(), request.outputKernel, output.good() ? "complete" : "write", {}};
}

std::vector<uint8_t> MakeKernelArgumentSizeElf(uint32_t argumentSize)
{
    constexpr char kSectionNames[] = "\0.shstrtab\0__CCE_KernelArgSize";
    const std::string sectionNames(kSectionNames, sizeof(kSectionNames));
    const size_t sectionHeadersOffset = sizeof(Elf64_Ehdr);
    const size_t sectionNamesOffset = sectionHeadersOffset + 3 * sizeof(Elf64_Shdr);
    const size_t argumentSizeOffset = sectionNamesOffset + sectionNames.size();
    std::vector<uint8_t> image(argumentSizeOffset + sizeof(argumentSize), 0);

    Elf64_Ehdr header{};
    std::memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_shoff = sectionHeadersOffset;
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = 3;
    header.e_shstrndx = 1;
    std::memcpy(image.data(), &header, sizeof(header));

    Elf64_Shdr sectionNamesHeader{};
    sectionNamesHeader.sh_name = 1;
    sectionNamesHeader.sh_type = SHT_STRTAB;
    sectionNamesHeader.sh_offset = sectionNamesOffset;
    sectionNamesHeader.sh_size = sectionNames.size();
    std::memcpy(
        image.data() + sectionHeadersOffset + sizeof(Elf64_Shdr), &sectionNamesHeader, sizeof(sectionNamesHeader));

    Elf64_Shdr argumentSizeHeader{};
    argumentSizeHeader.sh_name = 11;
    argumentSizeHeader.sh_type = SHT_NOTE;
    argumentSizeHeader.sh_offset = argumentSizeOffset;
    argumentSizeHeader.sh_size = sizeof(argumentSize);
    std::memcpy(
        image.data() + sectionHeadersOffset + 2 * sizeof(Elf64_Shdr), &argumentSizeHeader, sizeof(argumentSizeHeader));
    std::memcpy(image.data() + sectionNamesOffset, sectionNames.data(), sectionNames.size());
    std::memcpy(image.data() + argumentSizeOffset, &argumentSize, sizeof(argumentSize));
    return image;
}

DbiResult ThrowingPatch(const DbiRequest& request, void* userdata)
{
    auto& state = *static_cast<TestState*>(userdata);
    ++state.runnerCalls;
    state.workDirectories.push_back(request.workDirectory);
    boost::filesystem::create_directories(request.workDirectory);
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
        state_.config.arch = "dav-3510";
        state_.config.traceArgumentOffset = 8;
        state_.config.probeGroups = {ProbeGroup::Mte2};
        state_.config.workDirectory = directory_;
        state_.config.cacheDirectory = directory_ + "/cache";
        state_.config.keepTemp = true;
    }

    void TearDown() override
    {
        boost::system::error_code error;
        boost::filesystem::remove_all(directory_, error);
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

TEST_F(BinaryInstrumenterTest, InstrumentsWithoutKernelArgumentSizeMetadata)
{
    const std::string original = "kernel-without-argument-size-metadata";

    const BinaryInstrumentationResult result =
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Instrumented);
    EXPECT_EQ(state_.runnerCalls, 1);
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
    EXPECT_TRUE(boost::filesystem::is_directory(state_.workDirectories.front()));
    EXPECT_TRUE(
        boost::filesystem::is_regular_file(boost::filesystem::path(state_.workDirectories.front()) / "patched.o"));
}

TEST_F(BinaryInstrumenterTest, CleanupRemovesRequestDirectoryButPreservesExternalCache)
{
    state_.config.keepTemp = false;
    const std::string original = "kernel-data";
    const boost::filesystem::path cacheMarker = boost::filesystem::path(state_.config.cacheDirectory) / "cache-marker";
    boost::filesystem::create_directories(cacheMarker.parent_path());
    std::ofstream(cacheMarker) << "cached";

    ASSERT_EQ(
        InstrumentBinary(state_.config, original.data(), original.size(), &FakePatch, &state_).status,
        BinaryInstrumentationStatus::Instrumented);

    ASSERT_EQ(state_.workDirectories.size(), 1U);
    EXPECT_FALSE(boost::filesystem::exists(state_.workDirectories.front()));
    EXPECT_TRUE(boost::filesystem::is_regular_file(cacheMarker));
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
    EXPECT_FALSE(boost::filesystem::exists(state_.workDirectories.front()));
}

TEST_F(BinaryInstrumenterTest, RuntimeFacadeConsumesPatchedBytesAcrossAnAbiStableBoundary)
{
    const std::vector<uint8_t> original = MakeKernelArgumentSizeElf(0);
    std::vector<uint8_t> consumed;
    const auto cannRoot = boost::filesystem::path(directory_) / "fake-cann";
    const auto tools = cannRoot / "tools/bisheng_compiler/bin";
    const auto runtime = cannRoot / "x86_64-linux/lib64/libacl_rt.so";
    boost::filesystem::create_directories(tools);
    boost::filesystem::create_directories(runtime.parent_path());
    std::ofstream(runtime).put('\n');
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        std::ofstream(tools / name).put('\n');
    }

    const RuntimeBinaryInstrumentationResult result = InstrumentRuntimeBinary(
        original.data(), original.size(), PROBE_GROUP_MTE2, "Ascend950PR_9599", runtime.c_str(),
        &CaptureInstrumentedBinary, &consumed, &FakePatch, &state_);

    EXPECT_EQ(result.status, BinaryInstrumentationStatus::Instrumented);
    EXPECT_EQ(result.strict, 1U);
    EXPECT_EQ(result.consumerStatus, 73);
    EXPECT_EQ(result.traceArgumentOffset, 8U);
    EXPECT_EQ(std::string(consumed.begin(), consumed.end()), "patched");
}

} // namespace
} // namespace aclsan
