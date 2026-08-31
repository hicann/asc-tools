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
        std::string pathTemplate = "/tmp/npu-compute-launcher-test-XXXXXX";
        pathTemplate.push_back('\0');
        char* created = ::mkdtemp(pathTemplate.data());
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TestDirectory()
    {
        if (!path_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }
    }

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

class ScopedCurrentDirectory {
public:
    explicit ScopedCurrentDirectory(const std::filesystem::path& path)
    {
        std::error_code error;
        original_ = std::filesystem::current_path(error);
        if (error) {
            return;
        }
        std::filesystem::current_path(path, error);
        active_ = !error;
    }

    ~ScopedCurrentDirectory()
    {
        if (active_) {
            std::error_code error;
            std::filesystem::current_path(original_, error);
        }
    }

    bool IsActive() const { return active_; }

private:
    std::filesystem::path original_;
    bool active_ = false;
};

class ScopedTmpdir {
public:
    explicit ScopedTmpdir(const std::string& value)
    {
        const char* original = std::getenv("TMPDIR");
        if (original != nullptr) {
            original_ = original;
            hadOriginal_ = true;
        }
        ::setenv("TMPDIR", value.c_str(), 1);
    }

    ~ScopedTmpdir()
    {
        if (hadOriginal_) {
            ::setenv("TMPDIR", original_.c_str(), 1);
        } else {
            ::unsetenv("TMPDIR");
        }
    }

private:
    std::string original_;
    bool hadOriginal_ = false;
};

CliConfig ShellConfig(const std::string& command)
{
    CliConfig config;
    config.sections = {"PipeUtilization"};
    config.program = "/bin/sh";
    config.program_arguments = {"-c", command};
    return config;
}

CliConfig ShellConfig(const std::string& command, const std::filesystem::path& output)
{
    CliConfig config = ShellConfig(command);
    config.export_path = output.string();
    return config;
}

std::string ValidCollectionCommand()
{
    return R"(test "$NPU_COMPUTE_CSV_OUTPUT_DIR" = "$NPU_COMPUTE_OUTPUT" && printf '{}\n{}\n{}\n{}\n{}\n' > "$NPU_COMPUTE_OUTPUT/HardwareInfo.jsonl" && printf 'name,value\npipe,1\n' > "$NPU_COMPUTE_OUTPUT/PipeUtilization.csv")";
}

bool IsCollectionDirectoryIn(const std::string& value, const std::filesystem::path& parent)
{
    const std::filesystem::path path(value);
    return path.is_absolute() && path.parent_path() == parent && std::filesystem::is_directory(path);
}

int CheckReportContents(const std::filesystem::path& report)
{
    std::vector<ImportedProfileEntry> results;
    std::string error;
    CHECK(ReadImportedProfileResults(report, &results, &error));
    CHECK(results.size() == 2U);
    CHECK(results[0].name == "HardwareInfo.jsonl");
    CHECK(results[1].name == "PipeUtilization.csv");
    return 0;
}

int TestWithoutExportPublishesReportInCurrentDirectory()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const CliConfig config = ShellConfig(ValidCollectionCommand());
    std::string collectionDataDirectory;
    std::string report;
    std::string error = "old error";

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 0);
    CHECK(error.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(std::filesystem::path(report).parent_path() == workDirectory.Path());
    CHECK(std::filesystem::path(report).extension() == ".npu-rep");
    CHECK(std::filesystem::is_regular_file(report));
    CHECK(CheckReportContents(report) == 0);
    return 0;
}

int TestExplicitReportKeepsDataInCurrentDirectory()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path output = workDirectory.Path() / "result.npu-rep";
    const CliConfig config = ShellConfig(ValidCollectionCommand(), output);
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 0);
    CHECK(error.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(report == output.string());
    CHECK(std::filesystem::is_regular_file(output));
    CHECK(CheckReportContents(output) == 0);
    return 0;
}

int TestExportDirectoryDoesNotMoveCollectionData()
{
    TestDirectory workDirectory;
    TestDirectory reportDirectory;
    CHECK(!workDirectory.Path().empty());
    CHECK(!reportDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const CliConfig config = ShellConfig(ValidCollectionCommand(), reportDirectory.Path());
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 0);
    CHECK(error.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(std::filesystem::path(report).parent_path() == reportDirectory.Path());
    CHECK(std::filesystem::is_regular_file(report));
    return 0;
}

int TestInvalidTmpdirDoesNotAffectCollection()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const ScopedTmpdir tmpdir((workDirectory.Path() / "missing-tmpdir").string());
    const CliConfig config = ShellConfig(ValidCollectionCommand(), workDirectory.Path() / "result.npu-rep");
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 0);
    CHECK(error.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(std::filesystem::is_regular_file(report));
    return 0;
}

int TestInvalidExportIsRejectedBeforeAppLaunch()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path marker = workDirectory.Path() / "app-ran";
    const std::filesystem::path output = workDirectory.Path() / "missing" / "result.npu-rep";
    const CliConfig config = ShellConfig("touch app-ran", output);
    std::string collectionDataDirectory = "old directory";
    std::string report = "old report";
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 4);
    CHECK(collectionDataDirectory.empty());
    CHECK(report.empty());
    CHECK(!std::filesystem::exists(marker));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("resolve report target failed") != std::string::npos);
    return 0;
}

int TestFailedAppRemovesEmptyDataAndDoesNotPublishReport()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path output = workDirectory.Path() / "result.npu-rep";
    const CliConfig config = ShellConfig("exit 23", output);
    std::string collectionDataDirectory;
    std::string report = "old report";
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 23);
    CHECK(report.empty());
    CHECK(collectionDataDirectory.empty());
    CHECK(std::filesystem::is_empty(workDirectory.Path()));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("status 23") != std::string::npos);
    return 0;
}

int TestMissingHardwareInfoRemovesEmptyDataAndDoesNotPublishReport()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path output = workDirectory.Path() / "result.npu-rep";
    const CliConfig config = ShellConfig("true", output);
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 3);
    CHECK(report.empty());
    CHECK(collectionDataDirectory.empty());
    CHECK(std::filesystem::is_empty(workDirectory.Path()));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("HardwareInfo.jsonl is missing") != std::string::npos);
    return 0;
}

int TestMissingHardwareInfoKeepsDataAndDoesNotPublishReport()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path output = workDirectory.Path() / "result.npu-rep";
    const CliConfig config =
        ShellConfig(R"(printf 'name,value\npipe,1\n' > "$NPU_COMPUTE_OUTPUT/PipeUtilization.csv")", output);
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 3);
    CHECK(report.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("HardwareInfo.jsonl is missing") != std::string::npos);
    return 0;
}

int TestPackingFailureKeepsDataAndDoesNotPublishReport()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path output = workDirectory.Path() / "result.npu-rep";
    const std::string command =
        ValidCollectionCommand() + R"( && printf 'unsupported\n' > "$NPU_COMPUTE_OUTPUT/notes.txt")";
    const CliConfig config = ShellConfig(command, output);
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 4);
    CHECK(report.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("pack") != std::string::npos);
    CHECK(error.find("notes.txt") != std::string::npos);
    return 0;
}

int TestPublishingFailureKeepsCollectionDataDirectory()
{
    TestDirectory workDirectory;
    CHECK(!workDirectory.Path().empty());
    const ScopedCurrentDirectory currentDirectory(workDirectory.Path());
    CHECK(currentDirectory.IsActive());
    const std::filesystem::path output =
        std::filesystem::path("/proc") / ("npu-compute-launcher-" + std::to_string(::getpid()) + ".npu-rep");
    const CliConfig config = ShellConfig(ValidCollectionCommand(), output);
    std::string collectionDataDirectory;
    std::string report;
    std::string error;

    CHECK(LaunchTarget(config, &collectionDataDirectory, &report, &error) == 4);
    CHECK(report.empty());
    CHECK(IsCollectionDirectoryIn(collectionDataDirectory, workDirectory.Path()));
    CHECK(!std::filesystem::exists(output));
    CHECK(error.find("publish") != std::string::npos);
    return 0;
}

} // namespace

int main()
{
    if (TestWithoutExportPublishesReportInCurrentDirectory() != 0 ||
        TestExplicitReportKeepsDataInCurrentDirectory() != 0 || TestExportDirectoryDoesNotMoveCollectionData() != 0 ||
        TestInvalidTmpdirDoesNotAffectCollection() != 0 || TestInvalidExportIsRejectedBeforeAppLaunch() != 0 ||
        TestFailedAppRemovesEmptyDataAndDoesNotPublishReport() != 0 ||
        TestMissingHardwareInfoRemovesEmptyDataAndDoesNotPublishReport() != 0 ||
        TestMissingHardwareInfoKeepsDataAndDoesNotPublishReport() != 0 ||
        TestPackingFailureKeepsDataAndDoesNotPublishReport() != 0 ||
        TestPublishingFailureKeepsCollectionDataDirectory() != 0) {
        return 1;
    }
    return 0;
}
