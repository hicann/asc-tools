// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi/dbi_pipeline.h"
#include "../../../common/tests/plog_capture.h"

#include <cstdlib>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include <gtest/gtest.h>

namespace aclsan {
namespace {

void WriteFile(const boost::filesystem::path& path, const std::string& content)
{
    boost::filesystem::create_directories(path.parent_path());
    std::ofstream output(path.string(), std::ios::binary | std::ios::trunc);
    output << content;
}

std::string ReadFile(const boost::filesystem::path& path)
{
    std::ifstream input(path.string(), std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void InstallFakeTool(const boost::filesystem::path& path)
{
    WriteFile(path, R"SH(#!/bin/sh
name=$(basename "$0")
printf '%s' "$name" >> "$DBI_FAKE_LOG"
for arg in "$@"; do printf ' <%s>' "$arg" >> "$DBI_FAKE_LOG"; done
printf '\n' >> "$DBI_FAKE_LOG"
if [ "$name" = llvm-objdump ]; then
  case "$2" in
    *group.o)
      for symbol in $(sed -n 's/.*void \(__sanitizer_report_[A-Za-z0-9_]*\)(.*/\1/p' "$(dirname "$2")"/src/*.cpp); do
        if [ "$symbol" != "$DBI_FAKE_DROP_SYMBOL" ]; then
          printf '00000000 w F .text.probe 00000010 %s\n' "$symbol"
        fi
      done
      ;;
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
if [ "$name" = bisheng-tune ]; then
  printf 'first line\n\n'
  printf '%01200d\n' 0
  printf 'last line\n'
  printf 'warning one\nwarning two\n' >&2
fi
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
    PlogCapture capture;
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_integration";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-3510";
    request.traceArgumentOffset = 24;
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache").string();
    request.keepTemp = true;

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_TRUE(result.success) << result.stage << ": " << result.diagnostic;
    EXPECT_EQ(result.patchedPath, request.outputKernel);
    EXPECT_TRUE(boost::filesystem::is_regular_file(request.outputKernel));

    const std::string logs = capture.Text();
    EXPECT_NE(logs.find("DBI stage=bisheng-tune output=stdout line=1 part=1 text=first line"), std::string::npos);
    EXPECT_NE(logs.find("DBI stage=bisheng-tune output=stdout line=2 part=1 text=\n"), std::string::npos);
    EXPECT_NE(logs.find("DBI stage=bisheng-tune output=stdout line=3 part=3 text="), std::string::npos);
    EXPECT_NE(logs.find("DBI stage=bisheng-tune output=stdout line=4 part=1 text=last line"), std::string::npos);
    EXPECT_NE(logs.find("DBI stage=bisheng-tune output=stderr line=2 part=1 text=warning two"), std::string::npos);
    std::istringstream records(logs);
    std::string record;
    while (std::getline(records, record)) {
        EXPECT_LT(record.size(), 1024U);
        EXPECT_NE(record.find("DBI stage="), std::string::npos) << record;
    }

    request.outputKernel = (root / "second-patched.o").string();
    request.workDirectory = (root / "second-work").string();
    const DbiResult cachedResult = RunDbiPipeline(request);
    EXPECT_TRUE(cachedResult.success) << cachedResult.stage << ": " << cachedResult.diagnostic;
    EXPECT_EQ(cachedResult.patchedPath, request.outputKernel);
    EXPECT_TRUE(boost::filesystem::is_regular_file(request.outputKernel));

    const std::string commands = ReadFile(root / "commands.log");
    EXPECT_NE(commands.find("bisheng <-xcce>"), std::string::npos) << commands;
    EXPECT_NE(
        commands.find("<" + (root / "toolchain/x86_64-linux/asc/impl/basic_api").string() + ">"), std::string::npos)
        << commands;
    EXPECT_NE(commands.find("ld.lld <-r>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("generated-mte2.cpp"), std::string::npos) << commands;
    EXPECT_NE(commands.find("generated-scalar.cpp"), std::string::npos) << commands;
    EXPECT_NE(commands.find("llvm-objdump <--syms>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<-execute-probe>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("bisheng-tune <--action=instru-probe>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<--tune-argsize=24>"), std::string::npos) << commands;
    EXPECT_NE(commands.find("<--dbi-config="), std::string::npos) << commands;
    EXPECT_EQ(CountOccurrences(commands, "bisheng <-xcce>"), 2U) << commands;
    EXPECT_EQ(CountOccurrences(commands, "ld.lld <-r>"), 1U) << commands;
    EXPECT_EQ(CountOccurrences(commands, "bisheng-tune <--action=instru-probe>"), 2U) << commands;
    unsetenv("DBI_FAKE_LOG");
    boost::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, FailedProbeLinkDoesNotPublishPartialCacheArtifact)
{
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_failed_cache";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);
    ASSERT_EQ(setenv("DBI_FAKE_FAIL", "ld.lld", 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "first-patched.o").string();
    request.arch = "dav-3510";
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
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
    boost::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, DoesNotAcceptStalePatchedOutputWhenTuneCreatesNothing)
{
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_stale_output";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "patched.o", "stale\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);
    ASSERT_EQ(setenv("DBI_FAKE_SKIP_OUTPUT", "bisheng-tune", 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-3510";
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache").string();

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.stage, "bisheng-tune");
    EXPECT_EQ(ReadFile(request.outputKernel), "stale\n");

    unsetenv("DBI_FAKE_SKIP_OUTPUT");
    unsetenv("DBI_FAKE_LOG");
    boost::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, RebuildsNonemptyCorruptAggregateCacheAsOneUnit)
{
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_corrupt_aggregate";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "first-patched.o").string();
    request.arch = "dav-3510";
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.workDirectory = (root / "first-work").string();
    request.cacheDirectory = (root / "cache").string();

    const DbiResult first = RunDbiPipeline(request);
    ASSERT_TRUE(first.success) << first.stage << ": " << first.diagnostic;
    struct stat cacheStatus {};
    ASSERT_EQ(stat((root / "cache").c_str(), &cacheStatus), 0);
    EXPECT_EQ(cacheStatus.st_mode & 0777, 0700);
    boost::filesystem::path artifactDirectory;
    for (const auto& entry : boost::filesystem::directory_iterator(root / "cache/aggregates")) {
        if (entry.is_directory()) {
            artifactDirectory = entry.path();
            break;
        }
    }
    ASSERT_FALSE(artifactDirectory.empty());
    struct stat artifactStatus {};
    ASSERT_EQ(stat(artifactDirectory.c_str(), &artifactStatus), 0);
    EXPECT_EQ(artifactStatus.st_mode & 0777, 0700);
    ASSERT_TRUE(boost::filesystem::is_regular_file(artifactDirectory / "manifest"));
    WriteFile(artifactDirectory / "ctrl.bin", "nonempty-corruption\n");

    request.outputKernel = (root / "second-patched.o").string();
    request.workDirectory = (root / "second-work").string();
    const DbiResult second = RunDbiPipeline(request);
    EXPECT_TRUE(second.success) << second.stage << ": " << second.diagnostic;
    EXPECT_EQ(CountOccurrences(ReadFile(root / "commands.log"), "ld.lld <-r>"), 2U);
    EXPECT_EQ(CountOccurrences(ReadFile(root / "commands.log"), "bisheng <-xcce>"), 2U);

    unsetenv("DBI_FAKE_LOG");
    boost::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, RejectsSymlinkCacheRoot)
{
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_symlink_cache";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");
    boost::filesystem::create_directories(root / "redirected-cache");
    boost::filesystem::create_directory_symlink(root / "redirected-cache", root / "cache-link");

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-3510";
    request.probeGroups = {ProbeGroup::Mte2};
    request.toolchainRoot = (root / "toolchain").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache-link").string();

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.stage, "cache-directory");
    EXPECT_TRUE(boost::filesystem::is_empty(root / "redirected-cache"));
    boost::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, RejectsUnsupportedGeneratedProbeArchitecture)
{
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_unsupported_arch";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-unknown";
    request.probeGroups = {ProbeGroup::Sync};
    request.toolchainRoot = (root / "toolchain").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache").string();

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.stage, "architecture");
    EXPECT_NE(result.diagnostic.find("dav-unknown"), std::string::npos);
    boost::filesystem::remove_all(root);
}

TEST(DbiIntegrationTest, RejectsGeneratedGroupWithMissingWeakSymbol)
{
    const auto root = boost::filesystem::temp_directory_path() / "dbi_pipeline_missing_generated_symbol";
    boost::filesystem::remove_all(root);
    const auto toolBin = root / "toolchain/tools/bisheng_compiler/bin";
    for (const char* name : {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"}) {
        InstallFakeTool(toolBin / name);
    }
    WriteFile(root / "input.o", "kernel\n");
    WriteFile(root / "commands.log", "");
    ASSERT_EQ(setenv("DBI_FAKE_LOG", (root / "commands.log").c_str(), 1), 0);
    ASSERT_EQ(setenv("DBI_FAKE_DROP_SYMBOL", "__sanitizer_report_copy_ubuf_to_cbuf", 1), 0);

    DbiRequest request{};
    request.inputKernel = (root / "input.o").string();
    request.outputKernel = (root / "patched.o").string();
    request.arch = "dav-3510";
    request.probeGroups = {ProbeGroup::Mte3};
    request.toolchainRoot = (root / "toolchain").string();
    request.workDirectory = (root / "work").string();
    request.cacheDirectory = (root / "cache").string();
    request.keepTemp = true;

    const DbiResult result = RunDbiPipeline(request);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.stage, "validate-probe");
    bool retainedStaging = false;
    for (const auto& entry : boost::filesystem::directory_iterator(root / "cache/groups")) {
        retainedStaging = retainedStaging || entry.path().filename().string().compare(0, 7, ".build-") == 0;
    }
    EXPECT_TRUE(retainedStaging);

    unsetenv("DBI_FAKE_DROP_SYMBOL");
    unsetenv("DBI_FAKE_LOG");
    boost::filesystem::remove_all(root);
}

} // namespace
} // namespace aclsan
