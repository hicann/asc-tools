/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "staging_directory.h"

#include <cstdio>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <regex>
#include <string>

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

using npu_compute::compute_launcher::StagingDirectory;

class TestDirectory {
public:
    TestDirectory()
    {
        std::string pathTemplate = "/tmp/npu-compute-staging-directory-test-XXXXXX";
        pathTemplate.push_back('\0');
        char* created = ::mkdtemp(pathTemplate.data());
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TestDirectory()
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

bool MatchesDirectoryName(const boost::filesystem::path& path)
{
    const std::string pattern = "^npu-compute-[0-9]+-" + std::to_string(::getpid()) + "-[A-Za-z0-9]{6}$";
    return std::regex_match(path.filename().string(), std::regex(pattern));
}

int TestCreatesUniqueDirectoriesUnderExplicitRoot()
{
    TestDirectory root;
    CHECK(!root.Path().empty());
    StagingDirectory first;
    StagingDirectory second;
    std::string error;

    CHECK(StagingDirectory::Create(root.Path(), &first, &error));
    CHECK(error.empty());
    CHECK(StagingDirectory::Create(root.Path(), &second, &error));
    CHECK(error.empty());

    const boost::filesystem::path firstPath = first.Path();
    const boost::filesystem::path secondPath = second.Path();
    CHECK(firstPath.is_absolute());
    CHECK(secondPath.is_absolute());
    CHECK(firstPath.parent_path() == root.Path());
    CHECK(secondPath.parent_path() == root.Path());
    CHECK(firstPath != secondPath);
    CHECK(MatchesDirectoryName(firstPath));
    CHECK(MatchesDirectoryName(secondPath));
    CHECK(boost::filesystem::is_directory(firstPath));
    CHECK(boost::filesystem::is_directory(secondPath));
    return 0;
}

int TestIgnoresTmpdirForExplicitRoot()
{
    TestDirectory root;
    CHECK(!root.Path().empty());
    const ScopedTmpdir tmpdir((root.Path() / "missing-tmpdir").string());
    StagingDirectory result;
    std::string error;

    CHECK(StagingDirectory::Create(root.Path(), &result, &error));
    CHECK(error.empty());
    CHECK(boost::filesystem::path(result.Path()).parent_path() == root.Path());
    return 0;
}

int TestRejectsInvalidRootAndClearsResult()
{
    TestDirectory root;
    CHECK(!root.Path().empty());
    StagingDirectory result;
    std::string error;

    CHECK(StagingDirectory::Create(root.Path(), &result, &error));
    CHECK(!result.Path().empty());

    CHECK(!StagingDirectory::Create(root.Path() / "missing", &result, &error));
    CHECK(result.Path().empty());
    CHECK(error.find("collection data directory") != std::string::npos);

    CHECK(!StagingDirectory::Create("/dev/null", &result, &error));
    CHECK(result.Path().empty());
    CHECK(error.find("collection data directory") != std::string::npos);

    CHECK(!StagingDirectory::Create("relative-root", &result, &error));
    CHECK(result.Path().empty());
    CHECK(error.find("absolute") != std::string::npos);
    return 0;
}

int TestRejectsNullResult()
{
    TestDirectory root;
    CHECK(!root.Path().empty());
    std::string error;

    CHECK(!StagingDirectory::Create(root.Path(), nullptr, &error));
    CHECK(error.find("result is null") != std::string::npos);
    return 0;
}

int TestRemovesEmptyDirectory()
{
    TestDirectory root;
    CHECK(!root.Path().empty());
    StagingDirectory result;
    std::string error;

    CHECK(StagingDirectory::Create(root.Path(), &result, &error));
    const boost::filesystem::path created = result.Path();
    CHECK(boost::filesystem::is_directory(created));

    CHECK(result.RemoveIfEmpty(&error));
    CHECK(error.empty());
    CHECK(result.Path().empty());
    CHECK(!boost::filesystem::exists(created));
    return 0;
}

int TestPreservesNonEmptyDirectory()
{
    TestDirectory root;
    CHECK(!root.Path().empty());
    StagingDirectory result;
    std::string error;

    CHECK(StagingDirectory::Create(root.Path(), &result, &error));
    const boost::filesystem::path created = result.Path();
    const boost::filesystem::path data = created / "partial.csv";
    std::FILE* file = std::fopen(data.c_str(), "w");
    CHECK(file != nullptr);
    CHECK(std::fputs("name,value\npartial,1\n", file) >= 0);
    CHECK(std::fclose(file) == 0);

    CHECK(result.RemoveIfEmpty(&error));
    CHECK(error.empty());
    CHECK(result.Path() == created.string());
    CHECK(boost::filesystem::is_regular_file(data));
    return 0;
}

} // namespace

int main()
{
    if (TestCreatesUniqueDirectoriesUnderExplicitRoot() != 0 || TestIgnoresTmpdirForExplicitRoot() != 0 ||
        TestRejectsInvalidRootAndClearsResult() != 0 || TestRejectsNullResult() != 0 ||
        TestRemovesEmptyDirectory() != 0 || TestPreservesNonEmptyDirectory() != 0) {
        return 1;
    }
    return 0;
}
