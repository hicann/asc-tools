/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rep_encoder.h"
#include "rep_report_writer.h"
#include "rep_test_decoder.h"

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

using npu_compute::compute_launcher::EncodeRep;
using npu_compute::compute_launcher::NpuRepFileType;
using npu_compute::compute_launcher::PublishRepReport;
using npu_compute::compute_launcher::PublishRepReportWithOperations;
using npu_compute::compute_launcher::RepEntry;
using npu_compute::compute_launcher::ReportFileOperations;
using npu_compute::compute_launcher::ReportTarget;
using npu_compute::compute_launcher::test::DecodedRep;
using npu_compute::compute_launcher::test::DecodeRep;

class TempDirectory {
public:
    TempDirectory()
    {
        std::string path_template =
            (std::filesystem::temp_directory_path() / "npu-compute-report-writer-test-XXXXXX").string();
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

std::vector<std::uint8_t> Bytes(std::string_view content) { return {content.begin(), content.end()}; }

bool BuildNestedRep(std::vector<std::uint8_t>* encoded)
{
    std::string error;
    std::vector<std::uint8_t> child;
    if (!EncodeRep({{"PipeUtilization.csv", NpuRepFileType::Csv, Bytes("block_id\n0\n")}}, &child, &error)) {
        return false;
    }
    return EncodeRep({{"device_0.npu.rep", NpuRepFileType::NpuRep, child}}, encoded, &error);
}

bool WriteFile(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>* content)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    content->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

bool HasTemporaryFile(const std::filesystem::path& directory)
{
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
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
    std::vector<std::uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    const ReportTarget target{temporary.Path() / "result.npu-rep"};
    std::string error;

    CHECK(PublishRepReport(encoded, target, &error));
    CHECK(error.empty());
    std::vector<std::uint8_t> actual;
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
    const std::filesystem::path path = temporary.Path() / "result.npu-rep";
    const std::string original = "original";
    CHECK(WriteFile(path, original));
    std::vector<std::uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    std::string error;

    CHECK(!PublishRepReport(encoded, ReportTarget{path}, &error));
    std::vector<std::uint8_t> actual;
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
    CHECK(!std::filesystem::exists(invalid_target.path));
    CHECK(!HasTemporaryFile(temporary.Path()));

    std::vector<std::uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    std::vector<std::uint8_t> invalid_length = encoded;
    invalid_length[28U] ^= 0x01U;
    const ReportTarget invalid_length_target{temporary.Path() / "invalid-length.npu-rep"};
    CHECK(!PublishRepReport(invalid_length, invalid_length_target, &error));
    CHECK(!std::filesystem::exists(invalid_length_target.path));

    std::vector<std::uint8_t> invalid_offset = encoded;
    invalid_offset[36U + 152U] = 0U;
    invalid_offset[36U + 153U] = 0U;
    const ReportTarget invalid_offset_target{temporary.Path() / "invalid-offset.npu-rep"};
    CHECK(!PublishRepReport(invalid_offset, invalid_offset_target, &error));
    CHECK(!std::filesystem::exists(invalid_offset_target.path));
    CHECK(!HasTemporaryFile(temporary.Path()));

    const ReportTarget unwritable{
        std::filesystem::path("/proc") / ("npu-compute-report-" + std::to_string(::getpid()) + ".npu-rep")};
    CHECK(!PublishRepReport(encoded, unwritable, &error));
    CHECK(!error.empty());
    CHECK(!std::filesystem::exists(unwritable.path));
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
    std::vector<std::uint8_t> encoded;
    CHECK(BuildNestedRep(&encoded));
    OperationState state;
    state.short_write = true;
    std::string error;
    const ReportTarget target{temporary.Path() / "short.npu-rep"};

    CHECK(PublishRepReportWithOperations(encoded, target, Operations(&state), &error));
    CHECK(state.write_calls > 1U);
    std::vector<std::uint8_t> actual;
    CHECK(ReadFile(target.path, &actual));
    CHECK(actual == encoded);
    CHECK(!HasTemporaryFile(temporary.Path()));
    return 0;
}

int TestInjectedFailuresLeaveNoPartialReport()
{
    std::vector<std::uint8_t> encoded;
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
        CHECK(!std::filesystem::exists(target.path));
        CHECK(!HasTemporaryFile(temporary.Path()));
    }

    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path path = temporary.Path() / "existing.npu-rep";
    const std::string original = "original";
    CHECK(WriteFile(path, original));
    OperationState state;
    state.fail_rename = true;
    std::string error;
    CHECK(!PublishRepReportWithOperations(encoded, ReportTarget{path}, Operations(&state), &error));
    std::vector<std::uint8_t> actual;
    CHECK(ReadFile(path, &actual));
    CHECK(actual == Bytes(original));
    CHECK(!HasTemporaryFile(temporary.Path()));
    return 0;
}

} // namespace

int main()
{
    if (TestPublishesCompleteRep() != 0 || TestExistingTargetIsNotOverwritten() != 0 ||
        TestRejectsInvalidRepAndUnwritableDirectory() != 0 || TestShortWritesAreRetried() != 0 ||
        TestInjectedFailuresLeaveNoPartialReport() != 0) {
        return 1;
    }
    return 0;
}
