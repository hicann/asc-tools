/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_host.h"

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
    std::ofstream output(path);
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
    npu_compute::DiagnosticSink diagnosticSink = [&diagnostics](std::string_view message) {
        diagnostics.emplace_back(message);
    };
    npu_compute::HostInfoCollectionOptions options;
    options.cpuTopologyRoot = cpuRoot;

    npu_compute::HostInfo host;
    CHECK(npu_compute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, options));
    CHECK(host.cpuPhysicalCount == 3);
    CHECK(host.cpuLogicalCount > 0);
    CHECK(host.memoryTotalSizeMb > 0);
    CHECK(host.diskTotalSizeGb > 0);
    CHECK(Contains(diagnostics, "cpu10"));
    CHECK(Contains(diagnostics, "cpu11"));

    diagnostics.clear();
    npu_compute::HostInfoCollectionOptions missingCpuOptions;
    missingCpuOptions.cpuTopologyRoot = temporary.Path() / "missing-cpu";
    host = {};
    CHECK(npu_compute::CollectHostInfo(outputDirectory, &host, &diagnosticSink, missingCpuOptions));
    CHECK(host.cpuPhysicalCount == 0);
    CHECK(host.cpuLogicalCount > 0);
    CHECK(host.memoryTotalSizeMb > 0);
    CHECK(host.diskTotalSizeGb > 0);
    CHECK(Contains(diagnostics, "online"));

    diagnostics.clear();
    host = {};
    CHECK(npu_compute::CollectHostInfo(temporary.Path() / "missing-output", &host, &diagnosticSink, options));
    CHECK(host.diskTotalSizeGb == 0);
    CHECK(Contains(diagnostics, "statvfs"));

    diagnostics.clear();
    CHECK(!npu_compute::CollectHostInfo(outputDirectory, nullptr, &diagnosticSink, options));
    CHECK(Contains(diagnostics, "result is null"));
    return 0;
}
