/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "report/rep_encoder.h"
#include "report/rep_report_writer.h"
#include "rep_test_decoder.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>

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

using npucompute::cli::EncodeRep;
using npucompute::cli::NpuRepFileType;
using npucompute::cli::PublishRepReport;
using npucompute::cli::PublishRepReportWithOperations;
using npucompute::cli::RepEntry;
using npucompute::cli::ReportFileOperations;
using npucompute::cli::ReportTarget;
using npucompute::cli::test::DecodedRep;
using npucompute::cli::test::DecodeRep;

class TempDirectory {
public:
    TempDirectory()
    {
        std::string path_template =
            (boost::filesystem::temp_directory_path() / "npu-compute-report-writer-test-XXXXXX").string();
        path_template.push_back('\0');
        char* created = ::mkdtemp(path_template.data());
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

std::vector<uint8_t> Bytes(std::string_view content) { return {content.begin(), content.end()}; }

bool BuildNestedRep(std::vector<uint8_t>* encoded)
{
    std::string error;
    std::vector<uint8_t> child;
    if (!EncodeRep({{"PipeUtilization.csv", NpuRepFileType::Csv, Bytes("block_id\n0\n")}}, &child, &error)) {
        return false;
    }
    return EncodeRep({{"device_0.npu.rep", NpuRepFileType::NpuRep, child}}, encoded, &error);
}

bool WriteFile(const boost::filesystem::path& path, std::string_view content)
{
    std::ofstream output(path.string(), std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

bool ReadFile(const boost::filesystem::path& path, std::vector<uint8_t>* content)
{
    std::ifstream input(path.string(), std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    content->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

bool HasTemporaryFile(const boost::filesystem::path& directory)
{
    boost::system::error_code error;
    for (const auto& entry : boost::filesystem::directory_iterator(directory, error)) {
        if (entry.path().filename().string().find(".tmp.") != std::string::npos) {
            return true;
        }
    }
    return false;
}

int TestPublishesCompleteRep()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    const ReportTarget target{temporary.Path() / "result.npu-rep"};
    std::string error;

    CHECK(PublishRepReport(encoded, target, &error));
    CHECK(error.empty());
    std::vector<uint8_t> actual;
    CHECK(ReadFile(target.path, &actual));
    CHECK(actual == encoded);
    DecodedRep top;
    CHECK(DecodeRep(actual, &top, &error));
    CHECK(top.entries.size() == 1U);
    DecodedRep child;
    CHECK(DecodeRep(top.entries[0].payload, &child, &error));
    CHECK(child.entries.size() == 1U);
    CHECK(child.entries[0].file_name == "PipeUtilization.csv");
    CHECK(!HasTemporaryFile(temporary.Path()));
    return 0;
}

int TestExistingTargetIsNotOverwritten()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const boost::filesystem::path path = temporary.Path() / "result.npu-rep";
    const std::string original = "original";
    CHECK(WriteFile(path, original));
    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    std::string error;

    CHECK(!PublishRepReport(encoded, ReportTarget{path}, &error));
    std::vector<uint8_t> actual;
    CHECK(ReadFile(path, &actual));
    CHECK(actual == Bytes(original));
    CHECK(!HasTemporaryFile(temporary.Path()));

    return 0;
}

int TestRejectsInvalidRepAndUnwritableDirectory()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    std::string error;
    const ReportTarget invalid_target{temporary.Path() / "invalid.npu-rep"};
    CHECK(!PublishRepReport({1U, 2U, 3U}, invalid_target, &error));
    CHECK(!boost::filesystem::exists(invalid_target.path));
    CHECK(!HasTemporaryFile(temporary.Path()));

    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    std::vector<uint8_t> invalid_length = encoded;
    invalid_length[28U] ^= 0x01U;
    const ReportTarget invalid_length_target{temporary.Path() / "invalid-length.npu-rep"};
    CHECK(!PublishRepReport(invalid_length, invalid_length_target, &error));
    CHECK(!boost::filesystem::exists(invalid_length_target.path));

    std::vector<uint8_t> invalid_offset = encoded;
    invalid_offset[36U + 152U] = 0U;
    invalid_offset[36U + 153U] = 0U;
    const ReportTarget invalid_offset_target{temporary.Path() / "invalid-offset.npu-rep"};
    CHECK(!PublishRepReport(invalid_offset, invalid_offset_target, &error));
    CHECK(!boost::filesystem::exists(invalid_offset_target.path));
    CHECK(!HasTemporaryFile(temporary.Path()));

    const ReportTarget unwritable{
        boost::filesystem::path("/proc") / ("npu-compute-report-" + std::to_string(::getpid()) + ".npu-rep")};
    CHECK(!PublishRepReport(encoded, unwritable, &error));
    CHECK(!error.empty());
    CHECK(!boost::filesystem::exists(unwritable.path));
    return 0;
}

struct OperationState {
    bool short_write = false;
    bool fail_sync = false;
    bool fail_close = false;
    bool fail_rename = false;
    std::size_t write_calls = 0;
    std::size_t sync_calls = 0;
    std::size_t close_calls = 0;
    std::size_t rename_calls = 0;
};

ssize_t InjectedWrite(int descriptor, const void* data, std::size_t size, void* context)
{
    auto* state = static_cast<OperationState*>(context);
    ++state->write_calls;
    const std::size_t request = state->short_write ? std::min<std::size_t>(size, 3U) : size;
    return ::write(descriptor, data, request);
}

int InjectedSync(int descriptor, void* context)
{
    auto* state = static_cast<OperationState*>(context);
    ++state->sync_calls;
    if (state->fail_sync && state->sync_calls == 1U) {
        errno = EIO;
        return -1;
    }
    return ::fsync(descriptor);
}

int InjectedClose(int descriptor, void* context)
{
    auto* state = static_cast<OperationState*>(context);
    ++state->close_calls;
    const int result = ::close(descriptor);
    if (state->fail_close && state->close_calls == 1U) {
        errno = EIO;
        return -1;
    }
    return result;
}

int InjectedRename(const char* source, const char* target, void* context)
{
    auto* state = static_cast<OperationState*>(context);
    ++state->rename_calls;
    if (state->fail_rename) {
        errno = EIO;
        return -1;
    }
    return ::rename(source, target);
}

ReportFileOperations Operations(OperationState* state)
{
    ReportFileOperations operations;
    operations.write = &InjectedWrite;
    operations.sync = &InjectedSync;
    operations.close = &InjectedClose;
    operations.rename = &InjectedRename;
    operations.context = state;
    return operations;
}

int TestShortWritesAreRetried()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    OperationState state;
    state.short_write = true;
    std::string error;
    const ReportTarget target{temporary.Path() / "short.npu-rep"};

    CHECK(PublishRepReportWithOperations(encoded, target, Operations(&state), &error));
    CHECK(state.write_calls > 1U);
    std::vector<uint8_t> actual;
    CHECK(ReadFile(target.path, &actual));
    CHECK(actual == encoded);
    CHECK(!HasTemporaryFile(temporary.Path()));
    return 0;
}

int TestInjectedFailuresLeaveNoPartialReport()
{
    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));

    for (int failure = 0; failure < 2; ++failure) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        OperationState state;
        state.fail_sync = failure == 0;
        state.fail_close = failure == 1;
        const ReportTarget target{temporary.Path() / "failed.npu-rep"};
        std::string error;
        CHECK(!PublishRepReportWithOperations(encoded, target, Operations(&state), &error));
        CHECK(!error.empty());
        CHECK(!boost::filesystem::exists(target.path));
        CHECK(!HasTemporaryFile(temporary.Path()));
    }

    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const boost::filesystem::path path = temporary.Path() / "existing.npu-rep";
    const std::string original = "original";
    CHECK(WriteFile(path, original));
    OperationState state;
    state.fail_rename = true;
    std::string error;
    CHECK(!PublishRepReportWithOperations(encoded, ReportTarget{path}, Operations(&state), &error));
    std::vector<uint8_t> actual;
    CHECK(ReadFile(path, &actual));
    CHECK(actual == Bytes(original));
    CHECK(!HasTemporaryFile(temporary.Path()));
    return 0;
}

int TestPublishConflictsPreserveTargets()
{
    std::vector<uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    for (int kind = 0; kind < 4; ++kind) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        const boost::filesystem::path target = temporary.Path() / "result.npu-rep";
        if (kind == 0 || kind == 1) {
            CHECK(boost::filesystem::create_directory(target));
            if (kind == 1) {
                CHECK(WriteFile(target / "keep.txt", "keep"));
            }
        } else if (kind == 2) {
            CHECK(WriteFile(target, "keep"));
        } else {
            CHECK(::symlink("missing-target", target.c_str()) == 0);
        }
        struct stat before {};
        CHECK(::lstat(target.c_str(), &before) == 0);
        std::string error;
        CHECK(!PublishRepReport(encoded, ReportTarget{target}, &error));
        CHECK(!error.empty());
        CHECK(!HasTemporaryFile(temporary.Path()));
        std::vector<uint8_t> actual;
        struct stat after {};
        CHECK(::lstat(target.c_str(), &after) == 0);
        CHECK(before.st_dev == after.st_dev && before.st_ino == after.st_ino);
        CHECK(before.st_mode == after.st_mode);
        if (kind == 0) {
            CHECK(boost::filesystem::is_empty(target));
        } else if (kind == 1) {
            CHECK(ReadFile(target / "keep.txt", &actual));
            CHECK(actual == Bytes("keep"));
        } else if (kind == 2) {
            CHECK(ReadFile(target, &actual));
            CHECK(actual == Bytes("keep"));
        } else {
            CHECK(boost::filesystem::read_symlink(target) == "missing-target");
        }
    }
    return 0;
}

} // namespace

int main()
{
    if (TestPublishConflictsPreserveTargets() != 0) {
        return 1;
    }
    if (TestPublishesCompleteRep() != 0 || TestExistingTargetIsNotOverwritten() != 0 ||
        TestRejectsInvalidRepAndUnwritableDirectory() != 0 || TestShortWritesAreRetried() != 0 ||
        TestInjectedFailuresLeaveNoPartialReport() != 0) {
        return 1;
    }
    return 0;
}
