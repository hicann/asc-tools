/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "imported_profile_results.h"
#include "launcher.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

int Check(bool condition, const char* expression, int line)
{
    if (condition) {
        return 0;
    }
    std::fprintf(stderr, "line %d: check failed: %s\n", line, expression);
    return 1;
}

#define CHECK(expression)                               \
    do {                                                \
        if (Check((expression), #expression, __LINE__)) \
            return 1;                                   \
    } while (false)

using npu_compute::compute_launcher::CliConfig;
using npu_compute::compute_launcher::ImportedProfileEntry;
using npu_compute::compute_launcher::LaunchTarget;
using npu_compute::compute_launcher::ReadImportedProfileResults;

class TestDirectory {
public:
    TestDirectory()
    {
        std::string path_template =
            (std::filesystem::temp_directory_path() / "npu-compute-launcher-test-XXXXXX").string();
        path_template.push_back('\0');
        char* created = ::mkdtemp(path_template.data());
        if (created == nullptr) {
            return;
        }
        path_ = created;
        const char* original = std::getenv("TMPDIR");
        if (original != nullptr) {
            original_tmpdir_ = original;
            had_original_tmpdir_ = true;
        }
        ::setenv("TMPDIR", path_.c_str(), 1);
    }

    ~TestDirectory()
    {
        if (had_original_tmpdir_) {
            ::setenv("TMPDIR", original_tmpdir_.c_str(), 1);
        } else {
            ::unsetenv("TMPDIR");
        }
        if (!path_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }
    }

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
    std::string original_tmpdir_;
    bool had_original_tmpdir_ = false;
};

CliConfig ShellConfig(const std::string& command, const std::filesystem::path& output)
{
    CliConfig config;
    config.sections = {"PipeUtilization"};
    config.program = "/bin/sh";
    config.program_arguments = {"-c", command};
    config.export_path = output.string();
    return config;
}

std::string ValidCollectionCommand()
{
    return R"(test "$NPU_COMPUTE_CSV_OUTPUT_DIR" = "$NPU_COMPUTE_OUTPUT" && printf '{}\n{}\n{}\n{}\n{}\n' > "$NPU_COMPUTE_OUTPUT/HardwareInfo.jsonl" && printf 'name,value\npipe,1\n' > "$NPU_COMPUTE_OUTPUT/PipeUtilization.csv")";
}

int TestSuccessfulAppPublishesReport()
{
    TestDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path output = temporary.Path() / "result.npu-rep";
    const CliConfig config = ShellConfig(ValidCollectionCommand(), output);
    std::string staging;
    std::string report;
    std::string error = "old error";

    CHECK(LaunchTarget(config, &staging, &report, &error) == 0);
    CHECK(error.empty());
    CHECK(!staging.empty());
    CHECK(std::filesystem::is_directory(staging));
    CHECK(report == output.string());
    CHECK(std::filesystem::is_regular_file(output));

    std::vector<ImportedProfileEntry> results;
    CHECK(ReadImportedProfileResults(output, &results, &error));
    CHECK(results.size() == 2U);
    CHECK(results[0].name == "HardwareInfo.jsonl");
    CHECK(results[1].name == "PipeUtilization.csv");
    return 0;
}

int TestFailedAppDoesNotPublishReport()
{
    TestDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path output = temporary.Path() / "result.npu-rep";
    const CliConfig config = ShellConfig("exit 23", output);
    std::string staging;
    std::string report = "old report";
    std::string error;

    CHECK(LaunchTarget(config, &staging, &report, &error) == 23);
    CHECK(report.empty());
    CHECK(std::filesystem::is_directory(staging));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("status 23") != std::string::npos);
    return 0;
}

int TestPackingFailureDoesNotPublishReport()
{
    TestDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path output = temporary.Path() / "result.npu-rep";
    const std::string command =
        ValidCollectionCommand() + R"( && printf 'unsupported\n' > "$NPU_COMPUTE_OUTPUT/notes.txt")";
    const CliConfig config = ShellConfig(command, output);
    std::string staging;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &staging, &report, &error) == 4);
    CHECK(report.empty());
    CHECK(std::filesystem::is_directory(staging));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("pack") != std::string::npos);
    CHECK(error.find("notes.txt") != std::string::npos);
    return 0;
}

int TestPublishingFailureKeepsStagingDirectory()
{
    TestDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path output =
        std::filesystem::path("/proc") / ("npu-compute-launcher-" + std::to_string(::getpid()) + ".npu-rep");
    const CliConfig config = ShellConfig(ValidCollectionCommand(), output);
    std::string staging;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &staging, &report, &error) == 4);
    CHECK(report.empty());
    CHECK(std::filesystem::is_directory(staging));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("publish") != std::string::npos);
    return 0;
}

} // namespace

int main()
{
    if (TestSuccessfulAppPublishesReport() != 0 || TestFailedAppDoesNotPublishReport() != 0 ||
        TestPackingFailureDoesNotPublishReport() != 0 || TestPublishingFailureKeepsStagingDirectory() != 0) {
        return 1;
    }
    return 0;
}
