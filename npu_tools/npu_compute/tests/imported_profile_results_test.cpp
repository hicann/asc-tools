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

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "rep_directory_packer.h"
#include "rep_encoder.h"

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

using npu_compute::compute_launcher::EncodeRep;
using npu_compute::compute_launcher::ImportedProfileEntry;
using npu_compute::compute_launcher::NpuRepFileType;
using npu_compute::compute_launcher::PackDirectoryToRep;
using npu_compute::compute_launcher::ReadImportedProfileResults;
using npu_compute::compute_launcher::UnpackImportedProfileResults;

class TempDirectory {
public:
    TempDirectory()
    {
        std::string path_template =
            (std::filesystem::temp_directory_path() / "npu-compute-import-test-XXXXXX").string();
        path_template.push_back('\0');
        char* created = ::mkdtemp(path_template.data());
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

std::vector<uint8_t> Bytes(std::string_view value) { return {value.begin(), value.end()}; }

bool WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    return output.good();
}

bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>* content)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    content->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

bool BuildNestedRep(std::vector<uint8_t>* encoded)
{
    std::string error;
    std::vector<uint8_t> child;
    if (!EncodeRep({{"Memory.csv", NpuRepFileType::Csv, Bytes("name,value\nmemory,2\n")}}, &child, &error)) {
        return false;
    }
    return EncodeRep(
        {{"HardwareInfo.jsonl", NpuRepFileType::Jsonl, Bytes("{\"category\":\"Host Info\"}\n")},
         {"device_0.npu.rep", NpuRepFileType::NpuRep, child}},
        encoded, &error);
}

const std::vector<uint8_t>& HardwareInfoBytes()
{
    static const std::vector<uint8_t> content = Bytes("{\"category\":\"Host Info\"}\n"
                                                      "{\"category\":\"Device Info\"}\n"
                                                      "{\"category\":\"CPU Information\"}\n"
                                                      "{\"category\":\"AI Core Information\"}\n"
                                                      "{\"category\":\"Memory Information\"}\n");
    return content;
}

bool BuildRecursiveFixture(const std::filesystem::path& fixture, std::vector<uint8_t>* encoded)
{
    if (!std::filesystem::create_directories(fixture / "device_0" / "details") ||
        !WriteFile(fixture / "HardwareInfo.jsonl", HardwareInfoBytes()) ||
        !WriteFile(fixture / "metadata.json", Bytes("{\"version\":1}\n")) ||
        !WriteFile(fixture / "profile.sqlite3", {0x53U, 0x51U, 0x4cU, 0x00U, 0xffU}) ||
        !WriteFile(fixture / "trace.pb", {0x08U, 0x96U, 0x01U, 0x00U}) ||
        !WriteFile(fixture / "device_0" / "Memory.csv", Bytes("name,value\nmemory,2\n")) ||
        !WriteFile(fixture / "device_0" / "details" / "L2Cache.csv", Bytes("name,value\nl2,3\n"))) {
        return false;
    }
    std::string error;
    return PackDirectoryToRep(fixture, encoded, &error);
}

std::vector<std::string> RelativeDirectoryEntries(const std::filesystem::path& root)
{
    std::vector<std::string> entries;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root)) {
        std::string relative = std::filesystem::relative(entry.path(), root).generic_string();
        if (entry.is_directory()) {
            relative += "/";
        }
        entries.push_back(std::move(relative));
    }
    std::sort(entries.begin(), entries.end());
    return entries;
}

bool SameDirectoryTrees(const std::filesystem::path& left, const std::filesystem::path& right)
{
    if (RelativeDirectoryEntries(left) != RelativeDirectoryEntries(right)) {
        return false;
    }
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(left)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::vector<uint8_t> leftContent;
        std::vector<uint8_t> rightContent;
        if (!ReadFile(entry.path(), &leftContent) ||
            !ReadFile(right / std::filesystem::relative(entry.path(), left), &rightContent) ||
            leftContent != rightContent) {
            return false;
        }
    }
    return true;
}

int TestReadsNestedProfileResults()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    const std::filesystem::path input = temporary.Path() / "input.npu-rep";
    CHECK(WriteFile(input, encoded));

    std::vector<ImportedProfileEntry> results;
    std::string error = "old error";
    CHECK(ReadImportedProfileResults(input, &results, &error));
    CHECK(error.empty());
    CHECK(results.size() == 2U);
    CHECK(results[0].name == "HardwareInfo.jsonl");
    CHECK(results[0].type == NpuRepFileType::Jsonl);
    CHECK(results[0].payload == Bytes("{\"category\":\"Host Info\"}\n"));
    CHECK(results[0].children.empty());
    CHECK(results[1].name == "device_0.npu.rep");
    CHECK(results[1].type == NpuRepFileType::NpuRep);
    CHECK(results[1].payload.empty());
    CHECK(results[1].children.size() == 1U);
    CHECK(results[1].children[0].name == "Memory.csv");
    return 0;
}

int TestUnpacksRecursiveProfileResults()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path fixture = temporary.Path() / "fixture";
    std::vector<uint8_t> encoded;
    CHECK(BuildRecursiveFixture(fixture, &encoded));
    const std::filesystem::path input = temporary.Path() / "input.npu-rep";
    CHECK(WriteFile(input, encoded));

    std::vector<ImportedProfileEntry> results;
    std::string error;
    CHECK(ReadImportedProfileResults(input, &results, &error));
    const std::filesystem::path output = temporary.Path() / "unpacked";
    CHECK(std::filesystem::create_directory(output));
    CHECK(UnpackImportedProfileResults(results, output, &error));
    CHECK(error.empty());

    std::vector<uint8_t> actual;
    CHECK(ReadFile(output / "HardwareInfo.jsonl", &actual));
    CHECK(actual == HardwareInfoBytes());
    CHECK(ReadFile(output / "metadata.json", &actual));
    CHECK(actual == Bytes("{\"version\":1}\n"));
    CHECK(ReadFile(output / "profile.sqlite3", &actual));
    CHECK(actual == std::vector<uint8_t>({0x53U, 0x51U, 0x4cU, 0x00U, 0xffU}));
    CHECK(ReadFile(output / "trace.pb", &actual));
    CHECK(actual == std::vector<uint8_t>({0x08U, 0x96U, 0x01U, 0x00U}));
    CHECK(ReadFile(output / "device_0" / "Memory.csv", &actual));
    CHECK(actual == Bytes("name,value\nmemory,2\n"));
    CHECK(ReadFile(output / "device_0" / "details" / "L2Cache.csv", &actual));
    CHECK(actual == Bytes("name,value\nl2,3\n"));
    CHECK(RelativeDirectoryEntries(output) == RelativeDirectoryEntries(fixture));
    CHECK(!std::filesystem::exists(output / ".hardware_info.lock"));
    return 0;
}

int TestUnpacksShortRepSuffix()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path output = temporary.Path() / "unpacked";
    CHECK(std::filesystem::create_directory(output));
    const std::vector<ImportedProfileEntry> results = {
        {"details.rep",
         NpuRepFileType::NpuRep,
         {},
         {{"L2Cache.csv", NpuRepFileType::Csv, Bytes("name,value\nl2,3\n"), {}}}}};
    std::string error;
    CHECK(UnpackImportedProfileResults(results, output, &error));
    std::vector<uint8_t> actual;
    CHECK(ReadFile(output / "details" / "L2Cache.csv", &actual));
    CHECK(actual == Bytes("name,value\nl2,3\n"));
    return 0;
}

int TestRejectsExistingOutputWithoutPartialWrites()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path fixture = temporary.Path() / "fixture";
    std::vector<uint8_t> encoded;
    CHECK(BuildRecursiveFixture(fixture, &encoded));
    const std::filesystem::path input = temporary.Path() / "input.npu-rep";
    CHECK(WriteFile(input, encoded));
    std::vector<ImportedProfileEntry> results;
    std::string error;
    CHECK(ReadImportedProfileResults(input, &results, &error));

    const std::filesystem::path output = temporary.Path() / "unpacked";
    CHECK(std::filesystem::create_directory(output));
    const std::vector<uint8_t> existing = Bytes("keep-existing");
    CHECK(WriteFile(output / "trace.pb", existing));
    CHECK(!UnpackImportedProfileResults(results, output, &error));
    CHECK(error.find("exists") != std::string::npos);
    CHECK(!std::filesystem::exists(output / "HardwareInfo.jsonl"));
    CHECK(!std::filesystem::exists(output / "device_0"));
    std::vector<uint8_t> actual;
    CHECK(ReadFile(output / "trace.pb", &actual));
    CHECK(actual == existing);
    return 0;
}

int TestRejectsUnsafeAndConflictingOutputModel()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path output = temporary.Path() / "unpacked";
    CHECK(std::filesystem::create_directory(output));
    std::string error;

    std::vector<ImportedProfileEntry> results = {{".hardware_info.lock", NpuRepFileType::Jsonl, Bytes("lock"), {}}};
    CHECK(!UnpackImportedProfileResults(results, output, &error));
    CHECK(error.find("lock") != std::string::npos);
    CHECK(std::filesystem::is_empty(output));

    results = {{"device_0", NpuRepFileType::Json, Bytes("{}"), {}}, {"device_0.rep", NpuRepFileType::NpuRep, {}, {}}};
    CHECK(!UnpackImportedProfileResults(results, output, &error));
    CHECK(error.find("conflict") != std::string::npos);
    CHECK(std::filesystem::is_empty(output));

    results = {{"device_0.bin", NpuRepFileType::NpuRep, {}, {}}};
    CHECK(!UnpackImportedProfileResults(results, output, &error));
    CHECK(error.find("must end") != std::string::npos);
    CHECK(std::filesystem::is_empty(output));
    return 0;
}

int TestRejectsInvalidInputAndChildRep()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    std::vector<ImportedProfileEntry> results;
    std::string error;
    CHECK(!ReadImportedProfileResults(temporary.Path() / "missing.npu-rep", &results, &error));
    CHECK(error.find("missing.npu-rep") != std::string::npos);
    CHECK(results.empty());
    CHECK(!ReadImportedProfileResults(temporary.Path(), &results, &error));
    CHECK(error.find("regular file") != std::string::npos);

    std::vector<uint8_t> child = Bytes("not-a-rep");
    std::vector<uint8_t> parent;
    CHECK(EncodeRep({{"broken.npu.rep", NpuRepFileType::NpuRep, child}}, &parent, &error));
    const std::filesystem::path input = temporary.Path() / "broken.npu-rep";
    CHECK(WriteFile(input, parent));
    CHECK(!ReadImportedProfileResults(input, &results, &error));
    CHECK(error.find("broken.npu.rep") != std::string::npos);
    CHECK(results.empty());
    return 0;
}

struct ProcessResult {
    int exit_code = -1;
    std::string standard_error;
};

bool RunCli(
    const std::filesystem::path& cli, const std::vector<std::string>& arguments,
    const std::filesystem::path& temporary_root, const std::filesystem::path& working_directory, ProcessResult* result)
{
    int pipe_descriptors[2];
    if (::pipe(pipe_descriptors) != 0) {
        return false;
    }
    const pid_t child = ::fork();
    if (child < 0) {
        ::close(pipe_descriptors[0]);
        ::close(pipe_descriptors[1]);
        return false;
    }
    if (child == 0) {
        ::close(pipe_descriptors[0]);
        ::dup2(pipe_descriptors[1], STDERR_FILENO);
        ::close(pipe_descriptors[1]);
        ::setenv("TMPDIR", temporary_root.c_str(), 1);
        ::setenv("ACL_API_INJECTION", "/npu-compute-import-must-not-load.so", 1);
        if (::chdir(working_directory.c_str()) != 0) {
            _exit(126);
        }
        std::vector<std::string> storage;
        storage.push_back(cli.string());
        storage.insert(storage.end(), arguments.begin(), arguments.end());
        std::vector<char*> argv;
        for (std::string& argument : storage) {
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);
        ::execv(cli.c_str(), argv.data());
        _exit(127);
    }

    ::close(pipe_descriptors[1]);
    char buffer[512];
    while (true) {
        const ssize_t count = ::read(pipe_descriptors[0], buffer, sizeof(buffer));
        if (count > 0) {
            result->standard_error.append(buffer, static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    ::close(pipe_descriptors[0]);
    int status = 0;
    if (::waitpid(child, &status, 0) != child || !WIFEXITED(status)) {
        return false;
    }
    result->exit_code = WEXITSTATUS(status);
    return true;
}

bool HasRuntimeCollectionOutput(const std::string& standardError)
{
    return standardError.find("data-directory=") != std::string::npos ||
           standardError.find("report=") != std::string::npos ||
           standardError.find("[prof_api_stub]") != std::string::npos ||
           standardError.find("[aclpti]") != std::string::npos;
}

bool ExtractUnpackedPath(const ProcessResult& result, std::filesystem::path* output)
{
    constexpr char kPrefix[] = "npu-compute: unpacked=";
    const std::size_t begin = result.standard_error.find(kPrefix);
    if (begin == std::string::npos || result.standard_error.find(kPrefix, begin + 1U) != std::string::npos) {
        return false;
    }
    const std::size_t pathBegin = begin + sizeof(kPrefix) - 1U;
    const std::size_t end = result.standard_error.find('\n', pathBegin);
    *output = result.standard_error.substr(pathBegin, end - pathBegin);
    return output->is_absolute();
}

bool HasTemporaryImportDirectory(const std::filesystem::path& outputRoot)
{
    constexpr char kPrefix[] = ".npu-compute-import-";
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(outputRoot)) {
        const std::string name = entry.path().filename().string();
        if (name.compare(0, sizeof(kPrefix) - 1U, kPrefix) == 0) {
            return true;
        }
    }
    return false;
}

int TestCliImportUnpacksResults(const std::filesystem::path& cli)
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path runtime_tmp = temporary.Path() / "tmp";
    const std::filesystem::path work = temporary.Path() / "work";
    const std::filesystem::path fixture = temporary.Path() / "fixture";
    CHECK(std::filesystem::create_directory(runtime_tmp));
    CHECK(std::filesystem::create_directory(work));
    std::vector<uint8_t> encoded;
    CHECK(BuildRecursiveFixture(fixture, &encoded));
    const std::filesystem::path input = temporary.Path() / "input.npu-rep";
    CHECK(WriteFile(input, encoded));

    ProcessResult defaultResult;
    CHECK(RunCli(cli, {"--import", input.string()}, runtime_tmp, work, &defaultResult));
    CHECK(defaultResult.exit_code == 0);
    std::filesystem::path defaultOutput;
    CHECK(ExtractUnpackedPath(defaultResult, &defaultOutput));
    CHECK(defaultOutput.parent_path() == work);
    CHECK(!HasRuntimeCollectionOutput(defaultResult.standard_error));
    CHECK(SameDirectoryTrees(fixture, defaultOutput));
    CHECK(std::filesystem::is_empty(runtime_tmp));

    const std::filesystem::path outputRoot = temporary.Path() / "exported";
    CHECK(std::filesystem::create_directory(outputRoot));
    CHECK(WriteFile(outputRoot / "keep.txt", Bytes("keep")));
    ProcessResult firstResult;
    CHECK(RunCli(cli, {"--import", input.string(), "--export", outputRoot.string()}, runtime_tmp, work, &firstResult));
    CHECK(firstResult.exit_code == 0);
    std::filesystem::path firstOutput;
    CHECK(ExtractUnpackedPath(firstResult, &firstOutput));
    CHECK(firstOutput.parent_path() == outputRoot);
    CHECK(!HasRuntimeCollectionOutput(firstResult.standard_error));
    CHECK(SameDirectoryTrees(fixture, firstOutput));
    CHECK(std::filesystem::is_empty(runtime_tmp));

    ProcessResult secondResult;
    CHECK(RunCli(cli, {"--import", input.string(), "--export", outputRoot.string()}, runtime_tmp, work, &secondResult));
    CHECK(secondResult.exit_code == 0);
    std::filesystem::path secondOutput;
    CHECK(ExtractUnpackedPath(secondResult, &secondOutput));
    CHECK(secondOutput.parent_path() == outputRoot);
    CHECK(firstOutput != secondOutput);
    CHECK(SameDirectoryTrees(fixture, secondOutput));
    std::vector<uint8_t> actual;
    CHECK(ReadFile(outputRoot / "keep.txt", &actual));
    CHECK(actual == Bytes("keep"));
    CHECK(!HasTemporaryImportDirectory(outputRoot));

    const std::filesystem::path invalid = temporary.Path() / "invalid.npu-rep";
    CHECK(WriteFile(invalid, Bytes("invalid")));
    ProcessResult invalidResult;
    CHECK(RunCli(cli, {"--import", invalid.string()}, runtime_tmp, work, &invalidResult));
    CHECK(invalidResult.exit_code == 4);
    CHECK(invalidResult.standard_error.find("invalid") != std::string::npos);
    CHECK(!HasTemporaryImportDirectory(work));

    const std::filesystem::path missingRoot = temporary.Path() / "missing-output-root";
    ProcessResult missingRootResult;
    CHECK(RunCli(
        cli, {"--import", input.string(), "--export", missingRoot.string()}, runtime_tmp, work, &missingRootResult));
    CHECK(missingRootResult.exit_code == 4);
    CHECK(missingRootResult.standard_error.find("does not exist") != std::string::npos);
    CHECK(!std::filesystem::exists(missingRoot));

    const std::filesystem::path regularFile = temporary.Path() / "regular-file";
    CHECK(WriteFile(regularFile, Bytes("keep")));
    ProcessResult regularFileResult;
    CHECK(RunCli(
        cli, {"--import", input.string(), "--export", regularFile.string()}, runtime_tmp, work, &regularFileResult));
    CHECK(regularFileResult.exit_code == 4);
    CHECK(regularFileResult.standard_error.find("not a directory") != std::string::npos);
    CHECK(ReadFile(regularFile, &actual));
    CHECK(actual == Bytes("keep"));

    ProcessResult writeFailure;
    CHECK(RunCli(cli, {"--import", input.string(), "--export", "/proc"}, runtime_tmp, work, &writeFailure));
    CHECK(writeFailure.exit_code == 4);
    CHECK(writeFailure.standard_error.find("create import temporary directory failed") != std::string::npos);
    CHECK(std::filesystem::is_empty(runtime_tmp));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <npu-compute>\n", argv[0]);
        return 2;
    }
    if (TestReadsNestedProfileResults() != 0 || TestUnpacksRecursiveProfileResults() != 0 ||
        TestUnpacksShortRepSuffix() != 0 || TestRejectsExistingOutputWithoutPartialWrites() != 0 ||
        TestRejectsUnsafeAndConflictingOutputModel() != 0 || TestRejectsInvalidInputAndChildRep() != 0 ||
        TestCliImportUnpacksResults(argv[1]) != 0) {
        return 1;
    }
    return 0;
}
