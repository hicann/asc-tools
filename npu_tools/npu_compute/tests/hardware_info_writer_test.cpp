/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_writer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return false;                                                                 \
        }                                                                                 \
    } while (false)

constexpr std::string_view kFinalFileName = "HardwareInfo.jsonl";
constexpr std::string_view kLockFileName = ".hardware_info.lock";
constexpr std::string_view kTemporaryPrefix = "HardwareInfo.jsonl.tmp.";

class TempDirectory {
public:
    TempDirectory()
    {
        std::string pathTemplate = (std::filesystem::temp_directory_path() / "npu-compute-writer-test-XXXXXX").string();
        pathTemplate.push_back('\0');
        char* created = ::mkdtemp(pathTemplate.data());
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TempDirectory()
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

bool ReadFile(const std::filesystem::path& path, std::string* value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    value->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

bool HasTemporaryFile(const std::filesystem::path& directory)
{
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.path().filename().string().compare(0, kTemporaryPrefix.size(), kTemporaryPrefix) == 0) {
            return true;
        }
    }
    return false;
}

bool TestPublishesCompleteFileAndKeepsStableLock()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::string content = "{\"category\":\"Host Info\",\"cpu physical count\":2}\n"
                                "{\"category\":\"Device Info\",\"npu count\":1}\n";
    std::string error = "old error";

    CHECK(
        npu_compute::PublishHardwareInfoJsonl(temporary.Path(), content, &error) ==
        npu_compute::PublishResult::Published);
    CHECK(error.empty());
    const std::filesystem::path finalPath = temporary.Path() / kFinalFileName;
    CHECK(std::filesystem::is_regular_file(finalPath));
    CHECK(std::filesystem::is_regular_file(temporary.Path() / kLockFileName));
    std::string actual;
    CHECK(ReadFile(finalPath, &actual));
    CHECK(actual == content);
    CHECK(!HasTemporaryFile(temporary.Path()));
    return true;
}

bool TestExistingFileIsNotOverwritten()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::string original = "original\n";
    std::string error;
    CHECK(
        npu_compute::PublishHardwareInfoJsonl(temporary.Path(), original, &error) ==
        npu_compute::PublishResult::Published);
    CHECK(
        npu_compute::PublishHardwareInfoJsonl(temporary.Path(), "replacement\n", &error) ==
        npu_compute::PublishResult::AlreadyPublished);
    CHECK(error.empty());
    std::string actual;
    CHECK(ReadFile(temporary.Path() / kFinalFileName, &actual));
    CHECK(actual == original);
    CHECK(!HasTemporaryFile(temporary.Path()));
    return true;
}

bool TestFailuresDoNotCreateFinalFile()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    std::string error;
    const std::filesystem::path missingDirectory = temporary.Path() / "missing";
    CHECK(
        npu_compute::PublishHardwareInfoJsonl(missingDirectory, "data", &error) == npu_compute::PublishResult::Failed);
    CHECK(!error.empty());
    CHECK(!std::filesystem::exists(missingDirectory / kFinalFileName));

    error.clear();
    const std::filesystem::path invalidFinalDirectory = temporary.Path() / "invalid-final";
    CHECK(std::filesystem::create_directory(invalidFinalDirectory));
    CHECK(std::filesystem::create_directory(invalidFinalDirectory / kFinalFileName));
    CHECK(
        npu_compute::PublishHardwareInfoJsonl(invalidFinalDirectory, "data", &error) ==
        npu_compute::PublishResult::Failed);
    CHECK(!error.empty());
    CHECK(std::filesystem::is_directory(invalidFinalDirectory / kFinalFileName));
    CHECK(!HasTemporaryFile(invalidFinalDirectory));

    error.clear();
    const std::filesystem::path blockedTemporaryDirectory = temporary.Path() / "blocked-temp";
    CHECK(std::filesystem::create_directory(blockedTemporaryDirectory));
    const std::filesystem::path blockedTemporaryPath =
        blockedTemporaryDirectory / (std::string(kTemporaryPrefix) + std::to_string(::getpid()));
    CHECK(std::filesystem::create_directory(blockedTemporaryPath));
    CHECK(
        npu_compute::PublishHardwareInfoJsonl(blockedTemporaryDirectory, "data", &error) ==
        npu_compute::PublishResult::Failed);
    CHECK(!error.empty());
    CHECK(!std::filesystem::exists(blockedTemporaryDirectory / kFinalFileName));
    CHECK(std::filesystem::is_directory(blockedTemporaryPath));
    return true;
}

bool TestTwoProcessesPublishOnlyOneCompleteFile()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::string firstContent(1024U * 1024U, 'A');
    const std::string secondContent(1024U * 1024U, 'B');

    const pid_t first = ::fork();
    CHECK(first >= 0);
    if (first == 0) {
        const auto result = npu_compute::PublishHardwareInfoJsonl(temporary.Path(), firstContent, nullptr);
        ::_exit(
            result == npu_compute::PublishResult::Published        ? 10 :
            result == npu_compute::PublishResult::AlreadyPublished ? 11 :
                                                                     12);
    }

    const pid_t second = ::fork();
    CHECK(second >= 0);
    if (second == 0) {
        const auto result = npu_compute::PublishHardwareInfoJsonl(temporary.Path(), secondContent, nullptr);
        ::_exit(
            result == npu_compute::PublishResult::Published        ? 10 :
            result == npu_compute::PublishResult::AlreadyPublished ? 11 :
                                                                     12);
    }

    int firstStatus = 0;
    int secondStatus = 0;
    CHECK(::waitpid(first, &firstStatus, 0) == first);
    CHECK(::waitpid(second, &secondStatus, 0) == second);
    CHECK(WIFEXITED(firstStatus));
    CHECK(WIFEXITED(secondStatus));
    const int firstCode = WEXITSTATUS(firstStatus);
    const int secondCode = WEXITSTATUS(secondStatus);
    CHECK((firstCode == 10 && secondCode == 11) || (firstCode == 11 && secondCode == 10));

    std::string actual;
    CHECK(ReadFile(temporary.Path() / kFinalFileName, &actual));
    CHECK(actual == firstContent || actual == secondContent);
    CHECK(!HasTemporaryFile(temporary.Path()));
    return true;
}

} // namespace

int main()
{
    if (!TestPublishesCompleteFileAndKeepsStableLock() || !TestExistingFileIsNotOverwritten() ||
        !TestFailuresDoNotCreateFinalFile() || !TestTwoProcessesPublishOnlyOneCompleteFile()) {
        return 1;
    }
    return 0;
}
