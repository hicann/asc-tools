/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "report_name.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
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

using npu_compute::compute_launcher::ReportNameSources;
using npu_compute::compute_launcher::ReportTarget;
using npu_compute::compute_launcher::ResolveReportTarget;
using npu_compute::compute_launcher::ResolveReportTargetWithSources;

class TempDirectory {
public:
    TempDirectory()
    {
        std::string path_template =
            (std::filesystem::temp_directory_path() / "npu-compute-report-name-test-XXXXXX").string();
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

struct FixedSources {
    std::uint64_t epoch_milliseconds = 1700000000123ULL;
    std::vector<std::array<std::uint8_t, 4>> random_values;
    std::size_t random_index = 0;
    bool fail_epoch = false;
    bool fail_random = false;
};

bool FixedEpoch(std::uint64_t* value, void* context, std::string* error)
{
    auto* sources = static_cast<FixedSources*>(context);
    if (sources->fail_epoch) {
        if (error != nullptr) {
            *error = "injected epoch failure";
        }
        return false;
    }
    *value = sources->epoch_milliseconds;
    return true;
}

bool FixedRandom(std::array<std::uint8_t, 4>* value, void* context, std::string* error)
{
    auto* sources = static_cast<FixedSources*>(context);
    if (sources->fail_random || sources->random_index >= sources->random_values.size()) {
        if (error != nullptr) {
            *error = "injected random failure";
        }
        return false;
    }
    *value = sources->random_values[sources->random_index++];
    return true;
}

ReportNameSources Sources(const std::filesystem::path& current_directory, FixedSources* fixed)
{
    ReportNameSources sources;
    sources.current_directory = current_directory;
    sources.epoch_milliseconds = &FixedEpoch;
    sources.random_bytes = &FixedRandom;
    sources.context = fixed;
    return sources;
}

bool WriteFile(const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "existing";
    return output.good();
}

int TestDefaultAndDirectoryTargets()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    FixedSources fixed;
    fixed.random_values = {{{0x01U, 0x23U, 0xabU, 0xffU}}, {{0xdeU, 0xadU, 0xbeU, 0xefU}}};
    const ReportNameSources sources = Sources(temporary.Path(), &fixed);
    ReportTarget target;
    std::string error = "old error";

    const bool resolved_default = ResolveReportTargetWithSources(std::nullopt, sources, &target, &error);
    if (!resolved_default) {
        std::fprintf(stderr, "default target error: %s\n", error.c_str());
    }
    CHECK(resolved_default);
    CHECK(error.empty());
    CHECK(target.path == temporary.Path() / "report_1700000000123_0123abff.npu-rep");

    const std::filesystem::path output_directory = temporary.Path() / "reports";
    CHECK(std::filesystem::create_directory(output_directory));
    CHECK(ResolveReportTargetWithSources(output_directory.string(), sources, &target, &error));
    CHECK(target.path == output_directory / "report_1700000000123_deadbeef.npu-rep");
    return 0;
}

int TestExplicitFileTarget()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    FixedSources fixed;
    const ReportNameSources sources = Sources(temporary.Path(), &fixed);
    ReportTarget target;
    std::string error;

    CHECK(ResolveReportTargetWithSources("custom.npu-rep", sources, &target, &error));
    CHECK(target.path == temporary.Path() / "custom.npu-rep");
    CHECK(fixed.random_index == 0U);

    CHECK(WriteFile(target.path));
    CHECK(!ResolveReportTargetWithSources("custom.npu-rep", sources, &target, &error));
    CHECK(error.find("exists") != std::string::npos);
    return 0;
}

int TestCollisionRetry()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    FixedSources fixed;
    fixed.random_values = {{{0x00U, 0x00U, 0x00U, 0x01U}}, {{0x00U, 0x00U, 0x00U, 0x02U}}};
    CHECK(WriteFile(temporary.Path() / "report_1700000000123_00000001.npu-rep"));
    const ReportNameSources sources = Sources(temporary.Path(), &fixed);
    ReportTarget target;
    std::string error;

    CHECK(ResolveReportTargetWithSources(std::nullopt, sources, &target, &error));
    CHECK(target.path == temporary.Path() / "report_1700000000123_00000002.npu-rep");
    CHECK(fixed.random_index == 2U);
    return 0;
}

int TestInvalidTargetsAndSources()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    FixedSources fixed;
    fixed.random_values = {{{0x01U, 0x02U, 0x03U, 0x04U}}};
    ReportNameSources sources = Sources(temporary.Path(), &fixed);
    ReportTarget target;
    std::string error;

    CHECK(!ResolveReportTargetWithSources("report.rep", sources, &target, &error));
    CHECK(!error.empty());
    CHECK(!ResolveReportTargetWithSources("missing/report.npu-rep", sources, &target, &error));
    CHECK(error.find("missing") != std::string::npos);
    CHECK(!ResolveReportTargetWithSources(std::string(), sources, &target, &error));

    fixed.fail_random = true;
    CHECK(!ResolveReportTargetWithSources(std::nullopt, sources, &target, &error));
    CHECK(error == "injected random failure");

    fixed.fail_random = false;
    fixed.fail_epoch = true;
    CHECK(!ResolveReportTargetWithSources(std::nullopt, sources, &target, &error));
    CHECK(error == "injected epoch failure");
    return 0;
}

int TestProductionEntryForExplicitTarget()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const std::filesystem::path explicit_path = temporary.Path() / "production.npu-rep";
    ReportTarget target;
    std::string error;
    CHECK(ResolveReportTarget(explicit_path.string(), &target, &error));
    CHECK(error.empty());
    CHECK(target.path == explicit_path);
    return 0;
}

} // namespace

int main()
{
    if (TestDefaultAndDirectoryTargets() != 0 || TestExplicitFileTarget() != 0 || TestCollisionRetry() != 0 ||
        TestInvalidTargetsAndSources() != 0 || TestProductionEntryForExplicitTarget() != 0) {
        return 1;
    }
    return 0;
}
