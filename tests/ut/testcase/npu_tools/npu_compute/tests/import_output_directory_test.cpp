/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "import_output_directory.h"

#include <unistd.h>

#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <optional>
#include <regex>
#include <string>

namespace {

int Check(bool condition, const char* expression, int line)
{
    if (condition) {
        return 0;
    }
    std::fprintf(stderr, "line %d: check failed: %s\n", line, expression);
    return 1;
}

#define CHECK(expression)                                      \
    do {                                                       \
        if (Check((expression), #expression, __LINE__) != 0) { \
            return 1;                                          \
        }                                                      \
    } while (false)

class TempDirectory {
public:
    TempDirectory()
    {
        std::string pathTemplate =
            (boost::filesystem::temp_directory_path() / "npu-compute-import-output-test-XXXXXX").string();
        pathTemplate.push_back('\0');
        char* created = ::mkdtemp(pathTemplate.data());
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TempDirectory()
    {
        if (!path_.empty()) {
            boost::system::error_code error;
            boost::filesystem::remove_all(path_, error);
        }
    }

    const boost::filesystem::path& Path() const { return path_; }

private:
    boost::filesystem::path path_;
};

class CurrentDirectory {
public:
    explicit CurrentDirectory(const boost::filesystem::path& path)
    {
        boost::system::error_code error;
        previous_ = boost::filesystem::current_path(error);
        if (!error) {
            boost::filesystem::current_path(path, error);
            active_ = !error;
        }
    }

    ~CurrentDirectory()
    {
        if (active_) {
            boost::system::error_code error;
            boost::filesystem::current_path(previous_, error);
        }
    }

    bool Active() const { return active_; }

private:
    boost::filesystem::path previous_;
    bool active_ = false;
};

bool MatchesFinalDirectoryName(const boost::filesystem::path& path)
{
    const std::string pattern = "^npu-compute-import-[0-9]+-" + std::to_string(::getpid()) + "-[A-Za-z0-9]{6}$";
    return std::regex_match(path.filename().string(), std::regex(pattern));
}

bool MatchesTemporaryDirectoryName(const boost::filesystem::path& path)
{
    const std::string pattern = "^\\.npu-compute-import-[0-9]+-" + std::to_string(::getpid()) + "-tmp-[A-Za-z0-9]{6}$";
    return std::regex_match(path.filename().string(), std::regex(pattern));
}

int TestDefaultOutputDirectory()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CurrentDirectory currentDirectory(temporary.Path());
    CHECK(currentDirectory.Active());

    npu_compute::compute_launcher::ImportOutputDirectory directory;
    std::string error;
    CHECK(npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report_demo.npu-rep", std::nullopt, &directory, &error));
    CHECK(error.empty());
    CHECK(directory.FinalPath().parent_path() == temporary.Path());
    CHECK(directory.TemporaryPath().parent_path() == temporary.Path());
    CHECK(MatchesFinalDirectoryName(directory.FinalPath()));
    CHECK(MatchesTemporaryDirectoryName(directory.TemporaryPath()));
    CHECK(boost::filesystem::is_directory(directory.TemporaryPath()));
    CHECK(!boost::filesystem::exists(directory.FinalPath()));
    return 0;
}

bool WriteFile(const boost::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path.string(), std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

std::string ReadFile(const boost::filesystem::path& path)
{
    std::ifstream input(path.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

int TestExistingOutputRootAndPublish()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CurrentDirectory currentDirectory(temporary.Path());
    CHECK(currentDirectory.Active());
    const boost::filesystem::path outputRoot = temporary.Path() / "custom-output";
    CHECK(boost::filesystem::create_directory(outputRoot));
    CHECK(WriteFile(outputRoot / "keep.txt", "keep"));

    npu_compute::compute_launcher::ImportOutputDirectory directory;
    std::string error;
    CHECK(npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report_demo.npu-rep", std::optional<std::string>("custom-output"), &directory, &error));
    CHECK(error.empty());
    CHECK(directory.FinalPath().parent_path() == outputRoot);
    CHECK(directory.TemporaryPath().parent_path() == outputRoot);
    CHECK(MatchesFinalDirectoryName(directory.FinalPath()));
    CHECK(MatchesTemporaryDirectoryName(directory.TemporaryPath()));
    CHECK(WriteFile(directory.TemporaryPath() / "marker.csv", "name,value\npipe,1\n"));
    CHECK(directory.Publish(&error));
    CHECK(error.empty());
    CHECK(directory.TemporaryPath().empty());
    CHECK(boost::filesystem::is_directory(directory.FinalPath()));
    CHECK(ReadFile(directory.FinalPath() / "marker.csv") == "name,value\npipe,1\n");
    CHECK(ReadFile(outputRoot / "keep.txt") == "keep");
    return 0;
}

int TestCreatesUniqueDirectoriesUnderOneRoot()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const boost::filesystem::path outputRoot = temporary.Path() / "output";
    CHECK(boost::filesystem::create_directory(outputRoot));

    npu_compute::compute_launcher::ImportOutputDirectory first;
    npu_compute::compute_launcher::ImportOutputDirectory second;
    std::string error;
    CHECK(npu_compute::compute_launcher::ImportOutputDirectory::Create(
        temporary.Path() / "input.npu-rep", std::optional<std::string>(outputRoot.string()), &first, &error));
    CHECK(error.empty());
    CHECK(npu_compute::compute_launcher::ImportOutputDirectory::Create(
        temporary.Path() / "input.npu-rep", std::optional<std::string>(outputRoot.string()), &second, &error));
    CHECK(error.empty());
    CHECK(first.FinalPath().parent_path() == outputRoot);
    CHECK(second.FinalPath().parent_path() == outputRoot);
    CHECK(first.FinalPath() != second.FinalPath());
    CHECK(first.TemporaryPath() != second.TemporaryPath());
    return 0;
}

int TestTemporaryCleanupAndPublishFailure()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const boost::filesystem::path outputRoot = temporary.Path() / "output";
    CHECK(boost::filesystem::create_directory(outputRoot));
    boost::filesystem::path abandoned;
    {
        npu_compute::compute_launcher::ImportOutputDirectory directory;
        std::string error;
        CHECK(npu_compute::compute_launcher::ImportOutputDirectory::Create(
            temporary.Path() / "input.npu-rep", std::optional<std::string>(outputRoot.string()), &directory, &error));
        abandoned = directory.TemporaryPath();
        CHECK(boost::filesystem::is_directory(abandoned));
    }
    CHECK(!boost::filesystem::exists(abandoned));

    npu_compute::compute_launcher::ImportOutputDirectory directory;
    std::string error;
    CHECK(npu_compute::compute_launcher::ImportOutputDirectory::Create(
        temporary.Path() / "input.npu-rep", std::optional<std::string>(outputRoot.string()), &directory, &error));
    const boost::filesystem::path temporaryPath = directory.TemporaryPath();
    const boost::filesystem::path finalPath = directory.FinalPath();
    CHECK(boost::filesystem::create_directory(finalPath));
    CHECK(WriteFile(finalPath / "keep.txt", "keep"));
    CHECK(!directory.Publish(&error));
    CHECK(error.find("publish") != std::string::npos);
    CHECK(boost::filesystem::is_directory(temporaryPath));
    CHECK(ReadFile(finalPath / "keep.txt") == "keep");
    return 0;
}

int TestInvalidTargets()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CurrentDirectory currentDirectory(temporary.Path());
    CHECK(currentDirectory.Active());
    npu_compute::compute_launcher::ImportOutputDirectory directory;
    std::string error;

    CHECK(!npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report.unknown", std::nullopt, &directory, &error));
    CHECK(error.find("must end") != std::string::npos);
    CHECK(!npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report.npu-rep", std::optional<std::string>("missing-output-root"), &directory, &error));
    CHECK(error.find("does not exist") != std::string::npos);
    const boost::filesystem::path regularFile = temporary.Path() / "regular-file";
    CHECK(WriteFile(regularFile, "keep"));
    CHECK(!npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report.npu-rep", std::optional<std::string>(regularFile.string()), &directory, &error));
    CHECK(error.find("not a directory") != std::string::npos);
    CHECK(ReadFile(regularFile) == "keep");
    CHECK(!npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report.npu-rep", std::optional<std::string>("/proc"), &directory, &error));
    CHECK(error.find("create import temporary directory") != std::string::npos);
    CHECK(!npu_compute::compute_launcher::ImportOutputDirectory::Create(
        "/input/report.npu-rep", std::nullopt, nullptr, &error));
    CHECK(error.find("null") != std::string::npos);
    return 0;
}

} // namespace

int main()
{
    if (TestExistingOutputRootAndPublish() != 0 || TestDefaultOutputDirectory() != 0 ||
        TestCreatesUniqueDirectoriesUnderOneRoot() != 0 || TestTemporaryCleanupAndPublishFailure() != 0 ||
        TestInvalidTargets() != 0) {
        return 1;
    }
    return 0;
}
