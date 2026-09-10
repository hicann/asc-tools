/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware/hardware_info_host.h"

#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <cstdlib>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

class TempDirectory {
public:
    TempDirectory()
    {
        std::string pathTemplate = (boost::filesystem::temp_directory_path() / "npu-compute-host-test-XXXXXX").string();
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

bool WriteFile(const boost::filesystem::path& path, std::string_view content)
{
    boost::system::error_code error;
    boost::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path.string());
    output << content;
    return output.good();
}

bool Contains(const std::vector<std::string>& diagnostics, std::string_view text)
{
    for (const std::string& diagnostic : diagnostics) {
        if (diagnostic.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    const boost::filesystem::path outputDirectory = temporary.Path() / "output";
    const boost::filesystem::path cpuRoot = temporary.Path() / "cpu";
    CHECK(boost::filesystem::create_directory(outputDirectory));
    CHECK(WriteFile(cpuRoot / "online", "0-3,8,10-11\n"));
    CHECK(WriteFile(cpuRoot / "cpu0/topology/physical_package_id", "0\n"));
    CHECK(WriteFile(cpuRoot / "cpu1/topology/physical_package_id", "0\n"));
    CHECK(WriteFile(cpuRoot / "cpu2/topology/physical_package_id", "1\n"));
    CHECK(WriteFile(cpuRoot / "cpu3/topology/physical_package_id", "1\n"));
    CHECK(WriteFile(cpuRoot / "cpu8/topology/physical_package_id", "2\n"));
    CHECK(WriteFile(cpuRoot / "cpu11/topology/physical_package_id", "invalid\n"));

    std::vector<std::string> diagnostics;
    npucompute::DiagnosticSink diagnosticSink = [&diagnostics](std::string_view message) {
        diagnostics.emplace_back(message);
    };
    npucompute::HostInfoCollectionOptions options;
    options.cpuTopologyRoot = cpuRoot;

    npucompute::HostInfo host;
    CHECK(npucompute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, options));
    CHECK(host.cpuPhysicalCount == 3);
    CHECK(host.cpuLogicalCount > 0);
    CHECK(host.memoryTotalSizeMb > 0);
    CHECK(host.diskTotalSizeGb > 0);
    CHECK(Contains(diagnostics, "cpu10"));
    CHECK(Contains(diagnostics, "cpu11"));

    diagnostics.clear();
    npucompute::HostInfoCollectionOptions missingCpuOptions;
    missingCpuOptions.cpuTopologyRoot = temporary.Path() / "missing-cpu";
    host = {};
    CHECK(npucompute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, missingCpuOptions));
    CHECK(host.cpuPhysicalCount == 0);
    CHECK(host.cpuLogicalCount > 0);
    CHECK(host.memoryTotalSizeMb > 0);
    CHECK(host.diskTotalSizeGb > 0);
    CHECK(Contains(diagnostics, "online"));

    diagnostics.clear();
    host = {};
    CHECK(npucompute::CollectHostInfo(temporary.Path() / "missing-output", &host, &diagnosticSink, options));
    CHECK(host.diskTotalSizeGb == 0);
    CHECK(Contains(diagnostics, "statvfs"));

    diagnostics.clear();
    CHECK(!npucompute::CollectHostInfo(outputDirectory, nullptr, &diagnosticSink, options));
    CHECK(Contains(diagnostics, "result is null"));

    const std::string invalidOnline[] = {
        "",    " \t\r\n", "1048577", "4294967295",          "4294967296",           "-1", "+1", "12x", "1 2",
        "3-1", "0-1-2",   "0,,1",    std::string(100, '9'), std::string("0\0x", 3),
    };
    for (const std::string& text : invalidOnline) {
        CHECK(WriteFile(cpuRoot / "online", text));
        diagnostics.clear();
        host.cpuPhysicalCount = 99;
        CHECK(npucompute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, options));
        CHECK(host.cpuPhysicalCount == 0);
        CHECK(Contains(diagnostics, "parse CPU online list failed"));
    }
    CHECK(WriteFile(cpuRoot / "cpu1048576/topology/physical_package_id", "1"));
    const std::string validOnline[] = {"1048576", "1048576-1048576", " 0000,0-0 \n"};
    for (const std::string& text : validOnline) {
        CHECK(WriteFile(cpuRoot / "online", text));
        diagnostics.clear();
        CHECK(npucompute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, options));
        CHECK(host.cpuPhysicalCount == 1);
        CHECK(diagnostics.empty());
    }
    CHECK(WriteFile(cpuRoot / "online", "0"));
    struct PackageCase {
        std::string text;
        bool valid;
    };
    const PackageCase packages[] = {
        {"0", true},
        {"0001", true},
        {" \t42\r\n", true},
        {"9223372036854775807", true},
        {"-0", true},
        {"-00", true},
        {"-1", false},
        {"-9223372036854775808", false},
        {"-9223372036854775809", false},
        {"9223372036854775808", false},
        {"18446744073709551616", false},
        {std::string(100, '9'), false},
        {"", false},
        {" \t\r\n", false},
        {"+1", false},
        {"-", false},
        {"--0", false},
        {"12x", false},
        {"1 2", false},
        {std::string("0\0x", 3), false},
    };
    for (const PackageCase& test : packages) {
        CHECK(WriteFile(cpuRoot / "cpu0/topology/physical_package_id", test.text));
        diagnostics.clear();
        host.cpuPhysicalCount = 99;
        CHECK(npucompute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, options));
        CHECK(host.cpuPhysicalCount == (test.valid ? 1U : 0U));
        CHECK(Contains(diagnostics, "parse CPU package ID failed") == !test.valid);
        CHECK(diagnostics.size() == (test.valid ? 0U : 1U));
    }
    return 0;
}
