// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi_pipeline.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include <gtest/gtest.h>

namespace aclsan {
namespace {

void WriteFile(const std::filesystem::path& path, const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void InstallFakeTool(const std::filesystem::path& path)
{
    WriteFile(path, R"SH(#!/bin/sh
name=$(basename "$0")
printf '%s' "$name" >> "$DBI_FAKE_LOG"
for arg in "$@"; do printf ' <%s>' "$arg" >> "$DBI_FAKE_LOG"; done
printf '\n' >> "$DBI_FAKE_LOG"
if [ "$name" = llvm-objdump ]; then
  case "$2" in
    *probe.o) printf '00000000 w F .text.probe 00000010 __sanitizer_report_probe\n' ;;
    *) printf '00000000 g F .text.kernel 00000010 kernel_main\n' ;;
  esac
  exit 0
fi
output=
previous=
for arg in "$@"; do
  if [ "$previous" = -o ]; then output="$arg"; fi
  case "$arg" in -o=*) output=${arg#-o=} ;; esac
  previous="$arg"
done
if [ -n "$output" ] && [ "$name" != "$DBI_FAKE_SKIP_OUTPUT" ]; then printf 'fake-%s\n' "$name" > "$output"; fi
if [ "$name" = "$DBI_FAKE_FAIL" ]; then exit 7; fi
)SH");
    ASSERT_EQ(chmod(path.c_str(), 0755), 0);
}

std::size_t CountOccurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    for (std::size_t position = 0; (position = text.find(needle, position)) != std::string::npos;
         position += needle.size()) {
        ++count;
    }
    return count;
}

TEST(DbiIntegrationTest, CompilesLinksAndPatchesSelectedProbeSet)
{
    const auto root = std::filesystem::temp_directory_path() / "dbi_pipeline_integration";
    std::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "toolchain/x86_64-linux/asc/include/kernel_operator.h", "// marker\n");
    WriteFile(
        root / "toolchain/x86_64-linux/ascendc/include/highlevel_api/kernel_tiling/kernel_tiling.h", "// marker\n");
    WriteFile(root / "sources/probes/mte2.cpp", "// mte2 source\n");
    WriteFile(root / "sources/probes/scalar.cpp", "// scalar source\n");
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-c220";
    request.argSize = 128;
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.sourceRoot = (root / "sources").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache").string();
    request.keepTemp = true;
    request.extraCompilerArgs = {"-g"};

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_TRUE(result.success) << result.stage << ": " << result.diagnostic;
    EXPECT_EQ(result.patchedPath, request.outputKernel);
    EXPECT_TRUE(std::filesystem::is_regular_file(request.outputKernel));

    request.outputKernel = (root / "second-patched.o").string();
    request.workDirectory = (root / "second-work").string();
    const DbiResult cachedResult = RunDbiPipeline(request);
    EXPECT_TRUE(cachedResult.success) << cachedResult.stage << ": " << cachedResult.diagnostic;
    EXPECT_EQ(cachedResult.patchedPath, request.outputKernel);
    EXPECT_TRUE(std::filesystem::is_regular_file(request.outputKernel));

    const std::string commands = ReadFile(root / "commands.log");
    EXPECT_NE(commands.find("bisheng <-xcce>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<--cce-aicore-arch=dav-c220>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<-DTILING_KEY_VAR=0>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<-g>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<-I> <" + (root / "toolchain/x86_64-linux/asc").string() + ">"), std::string::npos)
        << commands;
    EXPECT_NE(commands.find("<-I> <" + (root / "toolchain/x86_64-linux/asc/include").string() + ">"), std::string::npos)
        << commands;
    EXPECT_NE(
        commands.find("<-I> <" + (root / "toolchain/x86_64-linux/asc/include/basic_api").string() + ">"),
        std::string::npos)
        << commands;
    EXPECT_NE(
        commands.find("<-I> <" + (root / "toolchain/x86_64-linux/ascendc/include/highlevel_api").string() + ">"),
        std::string::npos)
        << commands;
    EXPECT_NE(commands.find("<-I> <" + (root / "sources").string() + ">"), std::string::npos) << commands;
    EXPECT_NE(commands.find("ld.lld <-r>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("llvm-objdump <--syms>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<-execute-probe>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("bisheng-tune <--action=instru-probe>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<--dbi-config="), std::string::npos) << commands;
    EXPECT_EQ(CountOccurrences(commands, "bisheng <-xcce>"), 2U) << commands;
    EXPECT_EQ(CountOccurrences(commands, "ld.lld <-r>"), 1U) << commands;
    EXPECT_EQ(CountOccurrences(commands, "bisheng-tune <--action=instru-probe>"), 2U) << commands;
    unsetenv("DBI_FAKE_LOG");
    std::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, FailedProbeLinkDoesNotPublishPartialCacheArtifact)
{
    const auto root = std::filesystem::temp_directory_path() / "dbi_pipeline_failed_cache";
    std::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "toolchain/x86_64-linux/asc/include/kernel_operator.h", "// marker\n");
    WriteFile(
        root / "toolchain/x86_64-linux/ascendc/include/highlevel_api/kernel_tiling/kernel_tiling.h", "// marker\n");
    WriteFile(root / "sources/probes/mte2.cpp", "// mte2 source\n");
    WriteFile(root / "sources/probes/scalar.cpp", "// scalar source\n");
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);
    ASSERT_EQ(setenv("DBI_FAKE_FAIL", "ld.lld", 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "first-patched.o").string();
    request.arch = "dav-c220";
    request.argSize = 128;
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.sourceRoot = (root / "sources").string();
    request.workDirectory = (root / "first-work").string();
    request.cacheDirectory = (root / "cache").string();

    const DbiResult failed = RunDbiPipeline(request);
    EXPECT_FALSE(failed.success);
    EXPECT_EQ(failed.stage, "link-probe");

    ASSERT_EQ(unsetenv("DBI_FAKE_FAIL"), 0);
    request.outputKernel = (root / "second-patched.o").string();
    request.workDirectory = (root / "second-work").string();
    const DbiResult retried = RunDbiPipeline(request);
    EXPECT_TRUE(retried.success) << retried.stage << ": " << retried.diagnostic;

    const std::string commands = ReadFile(root / "commands.log");
    EXPECT_EQ(CountOccurrences(commands, "ld.lld <-r>"), 2U) << commands;
    unsetenv("DBI_FAKE_LOG");
    std::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, DoesNotAcceptStalePatchedOutputWhenTuneCreatesNothing)
{
    const auto root = std::filesystem::temp_directory_path() / "dbi_pipeline_stale_output";
    std::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "toolchain/x86_64-linux/asc/include/kernel_operator.h", "// marker\n");
    WriteFile(
        root / "toolchain/x86_64-linux/ascendc/include/highlevel_api/kernel_tiling/kernel_tiling.h", "// marker\n");
    WriteFile(root / "sources/probes/mte2.cpp", "// mte2 source\n");
    WriteFile(root / "sources/probes/scalar.cpp", "// scalar source\n");
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "patched.o", "stale\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);
    ASSERT_EQ(setenv("DBI_FAKE_SKIP_OUTPUT", "bisheng-tune", 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-c220";
    request.argSize = 128;
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.sourceRoot = (root / "sources").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache").string();

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.stage, "bisheng-tune");
    EXPECT_EQ(ReadFile(request.outputKernel), "stale\n");

    unsetenv("DBI_FAKE_SKIP_OUTPUT");
    unsetenv("DBI_FAKE_LOG");
    std::filesystem::remove_all(root);
}

} // namespace
} // namespace aclsan
