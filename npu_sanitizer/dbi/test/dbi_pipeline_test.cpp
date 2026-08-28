// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi_pipeline.h"
#include "ctrlbin_generator.h"
#include "tool_runner.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace aclsan {
namespace {

TEST(DbiPipelineTest, NormalizesProbeGroupsAndRejectsDuplicates)
{
    const auto groups = NormalizeProbeGroups({ProbeGroup::Sync, ProbeGroup::Mte2, ProbeGroup::Sync});
    ASSERT_EQ(groups.size(), 3U);
    EXPECT_EQ(groups[0], ProbeGroup::Mte2);
    EXPECT_EQ(groups[1], ProbeGroup::Scalar);
    EXPECT_EQ(groups[2], ProbeGroup::Sync);
}

TEST(DbiPipelineTest, MapsProbeGroupsToTranslationUnits)
{
    EXPECT_EQ(ProbeSourceName(ProbeGroup::Mte1), "mte1.cpp");
    EXPECT_EQ(ProbeSourceName(ProbeGroup::Mte2), "mte2.cpp");
    EXPECT_EQ(ProbeSourceName(ProbeGroup::Mte3), "mte3.cpp");
    EXPECT_EQ(ProbeSourceName(ProbeGroup::Fixpipe), "fixpipe.cpp");
    EXPECT_EQ(ProbeSourceName(ProbeGroup::Sync), "sync.cpp");
    EXPECT_EQ(ProbeSourceName(ProbeGroup::Scalar), "scalar.cpp");
}

TEST(DbiPipelineTest, ValidatesRequestFields)
{
    DbiRequest request{};
    request.inputKernel = "/tmp/input.o";
    request.outputKernel = "/tmp/output.o";
    request.arch = "dav-c220";
    request.argSize = 128;
    request.probeGroups = {ProbeGroup::Mte2};
    EXPECT_TRUE(ValidateRequest(request).empty());

    request.arch.clear();
    EXPECT_EQ(ValidateRequest(request), "architecture is empty");
}

TEST(DbiPipelineTest, ResolvesToolchainFromExplicitRoot)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "dbi_toolchain_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "tools/bisheng_compiler/bin");
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        std::ofstream(root / "tools/bisheng_compiler/bin" / name).put('\n');
    }

    const auto toolchain = ResolveToolchain(root.string(), {});
    EXPECT_EQ(toolchain.bisheng, (root / "tools/bisheng_compiler/bin/bisheng").string());
    EXPECT_EQ(toolchain.bishengTune, (root / "tools/bisheng_compiler/bin/bisheng-tune").string());
    EXPECT_EQ(toolchain.ldLld, (root / "tools/bisheng_compiler/bin/ld.lld").string());
    EXPECT_EQ(toolchain.llvmObjdump, (root / "tools/bisheng_compiler/bin/llvm-objdump").string());
    EXPECT_FALSE(toolchain.Complete());
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        ASSERT_EQ(chmod((root / "tools/bisheng_compiler/bin" / name).c_str(), 0755), 0);
    }
    EXPECT_TRUE(toolchain.Complete());
    std::filesystem::remove_all(root);
}

TEST(DbiPipelineTest, CacheKeyChangesWithProbeSetAndSourceDigest)
{
    const auto first = MakeCacheKey("dav-c220", {ProbeGroup::Mte2}, {"-O2"}, "source-a");
    const auto second = MakeCacheKey("dav-c220", {ProbeGroup::Mte3}, {"-O2"}, "source-a");
    const auto third = MakeCacheKey("dav-c220", {ProbeGroup::Mte2}, {"-O2"}, "source-b");
    EXPECT_NE(first, second);
    EXPECT_NE(first, third);
}

TEST(DbiPipelineTest, SourceDigestIncludesSharedTraceProtocolHeaders)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "dbi_source_digest_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "probes");
    std::ofstream(root / "probes/scalar.cpp") << "scalar";
    std::ofstream(root / "probes/mte2.cpp") << "probe";
    std::ofstream(root / "trace_record.h") << "record-v1";
    std::ofstream(root / "trace_buffer_abi.h") << "abi-v1";

    DbiRequest request{};
    request.sourceRoot = root.string();
    const auto first = ComputeProbeSourceDigest(request, {ProbeGroup::Mte2});
    std::ofstream(root / "trace_record.h", std::ios::trunc) << "record-v2";
    const auto second = ComputeProbeSourceDigest(request, {ProbeGroup::Mte2});
    std::ofstream(root / "trace_buffer_abi.h", std::ios::trunc) << "abi-v2";
    const auto third = ComputeProbeSourceDigest(request, {ProbeGroup::Mte2});

    EXPECT_NE(first, second);
    EXPECT_NE(second, third);
    std::filesystem::remove_all(root);
}

std::string ReadGeneratedFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

TEST(CtrlbinGeneratorTest, FiltersBindingsToSelectedProbeGroups)
{
    const auto symbols = BindingSymbols({ProbeGroup::Mte2});
    EXPECT_EQ(symbols.size(), 27U);
    for (const auto& symbol : symbols) {
        EXPECT_NE(symbol.find("sanitizer_report_"), std::string::npos);
    }

    const auto path = std::filesystem::temp_directory_path() / "mte2-only.ctrl.bin";
    std::filesystem::remove(path);
    std::string diagnostic;
    EXPECT_TRUE(GenerateCtrlBin(path.string(), {ProbeGroup::Mte2}, diagnostic)) << diagnostic;
    EXPECT_GT(std::filesystem::file_size(path), 0U);
    std::filesystem::remove(path);

    const auto mte1Symbols = BindingSymbols({ProbeGroup::Mte1});
    EXPECT_EQ(std::find(mte1Symbols.begin(), mte1Symbols.end(), "__sanitizer_report_set_padding"), mte1Symbols.end());

    const auto scalarSymbols = BindingSymbols({ProbeGroup::Scalar});
    ASSERT_EQ(scalarSymbols.size(), 8U);
    EXPECT_NE(
        std::find(scalarSymbols.begin(), scalarSymbols.end(), "__sanitizer_report_set_padding"), scalarSymbols.end());
    EXPECT_EQ(
        std::find(scalarSymbols.begin(), scalarSymbols.end(), "__sanitizer_report_set_l1_2d_b16"), scalarSymbols.end());
    EXPECT_NE(std::find(symbols.begin(), symbols.end(), "__sanitizer_report_set_l1_2d_b16"), symbols.end());
}

TEST(CtrlbinGeneratorTest, AllGroupsPreserveBindingCount) { EXPECT_EQ(BindingSymbols(AllProbeGroups()).size(), 83U); }

TEST(CtrlbinGeneratorTest, ConcurrentRequestsRemainIsolated)
{
    const auto root = std::filesystem::temp_directory_path() / "ctrlbin_generator_concurrency";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::string diagnostic;
    ASSERT_TRUE(GenerateCtrlBin((root / "mte2-reference.bin").string(), {ProbeGroup::Mte2}, diagnostic));
    ASSERT_TRUE(GenerateCtrlBin((root / "sync-reference.bin").string(), {ProbeGroup::Sync}, diagnostic));
    const std::string expectedMte2 = ReadGeneratedFile(root / "mte2-reference.bin");
    const std::string expectedSync = ReadGeneratedFile(root / "sync-reference.bin");

    constexpr std::size_t kThreadCount = 24;
    std::atomic<std::size_t> ready{0};
    std::atomic<bool> start{false};
    std::vector<uint8_t> generated(kThreadCount, 0);
    std::vector<std::thread> workers;
    for (std::size_t index = 0; index < kThreadCount; ++index) {
        workers.emplace_back([&, index] {
            ++ready;
            while (!start.load()) {
                std::this_thread::yield();
            }
            const bool mte2 = index % 2 == 0;
            std::string error;
            const auto path = root / ("result-" + std::to_string(index) + ".bin");
            if (GenerateCtrlBin(path.string(), {mte2 ? ProbeGroup::Mte2 : ProbeGroup::Sync}, error)) {
                generated[index] = ReadGeneratedFile(path) == (mte2 ? expectedMte2 : expectedSync) ? 1U : 0U;
            }
        });
    }
    while (ready.load() != kThreadCount) {
        std::this_thread::yield();
    }
    start = true;
    for (auto& worker : workers) {
        worker.join();
    }
    for (std::size_t index = 0; index < kThreadCount; ++index) {
        EXPECT_TRUE(generated[index]) << "thread " << index << " produced a mixed control file";
    }
    std::filesystem::remove_all(root);
}

TEST(ToolRunnerTest, CapturesStdoutStderrAndExitStatus)
{
    const auto result = RunTool({"/bin/sh", "-c", "printf stdout; printf stderr >&2; exit 7"});
    EXPECT_EQ(result.exitCode, 7);
    EXPECT_EQ(result.standardOutput, "stdout");
    EXPECT_EQ(result.standardError, "stderr");
}

TEST(ToolRunnerTest, ReportsExecFailure)
{
    const auto result = RunTool({"/definitely/missing/dbi-tool"});
    EXPECT_NE(result.exitCode, 0);
    EXPECT_FALSE(result.standardError.empty());
}

TEST(ToolRunnerTest, RemovesInjectionEnvironmentFromChild)
{
    ASSERT_EQ(setenv("ACL_API_INJECTION", "/tmp/libnpu_check.so", 1), 0);
    ASSERT_EQ(setenv("LD_PRELOAD", "/lib/x86_64-linux-gnu/libm.so.6", 1), 0);

    const auto result = RunTool({"/bin/sh", "-c", "test -z \"$ACL_API_INJECTION\" && test -z \"$LD_PRELOAD\""});

    EXPECT_EQ(unsetenv("ACL_API_INJECTION"), 0);
    EXPECT_EQ(unsetenv("LD_PRELOAD"), 0);
    EXPECT_EQ(result.exitCode, 0) << result.standardError;
}

} // namespace
} // namespace aclsan
