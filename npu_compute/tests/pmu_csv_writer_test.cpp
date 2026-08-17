/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "pmu_csv_writer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#define CHECK(condition)    \
    do {                    \
        if (!(condition)) { \
            return 1;       \
        }                   \
    } while (false)

namespace {

aclptiPmuDataRow::CoreData Core(
    aclptiCoreType type, std::uint8_t id, double cycles, std::initializer_list<std::pair<std::uint32_t, double>> values)
{
    aclptiPmuDataRow::CoreData core{};
    core.coreType = type;
    core.coreId = id;
    core.sampleCount = 1;
    core.totalCycles = cycles;
    for (const auto& [eventId, value] : values) {
        core.values.emplace(eventId, value);
        core.valueCounts.emplace(eventId, 1);
    }
    return core;
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::size_t FieldCount(const std::string& line)
{
    return line.empty() ? 0 : 1 + static_cast<std::size_t>(std::count(line.begin(), line.end(), ','));
}

bool HasMatchingColumnCounts(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::string header;
    std::string row;
    return std::getline(input, header) && std::getline(input, row) && FieldCount(header) == FieldCount(row);
}

} // namespace

int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    aclptiPmuDataResult result;
    result.status = ACLPTI_SUCCESS;
    aclptiPmuDataRow row{};
    row.blockId = 2;
    row.subBlockId = 3;
    row.coreData.push_back(Core(
        ACLPTI_CORE_TYPE_AIC, 0, 1000.0,
        {{0x424, 10.0}, {0x425, 2.0}, {0x426, 1.0}, {0x427, 5.0}, {0x428, 1.0}, {0x429, 1.0}}));
    row.coreData.push_back(Core(
        ACLPTI_CORE_TYPE_AIV, 0, 2000.0,
        {{0x424, 20.0},
         {0x425, 4.0},
         {0x426, 2.0},
         {0x427, 10.0},
         {0x428, 2.0},
         {0x429, 2.0},
         {0x34, 40.0},
         {0x35, 10.0},
         {0x422, 100.0},
         {0x57f, 10.0},
         {0x580, 10.0}}));
    result.pmuLogs.emplace(aclptiBlockKey{2, 3}, row);

    npu_compute::PmuCsvConfig config;
    config.outputDirectory = directory.string();
    config.frequencyMhz = 1000.0;
    config.socName = "950X";
    CHECK(
        npu_compute::PmuCsvWriter::Write(
            result, {"L2Cache", "Memory", "MemoryL0", "MemoryUB", "PipeUtilization"}, config) == ACLPTI_SUCCESS);

    const std::string l2 = ReadFile(directory / "L2Cache.csv");
    CHECK(l2.find("block_id,sub_block_id") == 0);
    CHECK(l2.find("aic_read_close_hit") != std::string::npos);
    CHECK(l2.find("aic_read_hit_rate(%)") != std::string::npos);
    CHECK(l2.find("2,3,") != std::string::npos);
    CHECK(std::filesystem::exists(directory / "Memory.csv"));
    CHECK(std::filesystem::exists(directory / "MemoryL0.csv"));
    CHECK(std::filesystem::exists(directory / "MemoryUB.csv"));
    CHECK(std::filesystem::exists(directory / "PipeUtilization.csv"));
    CHECK(ReadFile(directory / "PipeUtilization.csv").find(",0.25,") != std::string::npos);
    CHECK(ReadFile(directory / "MemoryUB.csv").find(",4.76837158203125,NA") != std::string::npos);
    CHECK(HasMatchingColumnCounts(directory / "L2Cache.csv"));
    CHECK(HasMatchingColumnCounts(directory / "Memory.csv"));
    CHECK(HasMatchingColumnCounts(directory / "MemoryL0.csv"));
    CHECK(HasMatchingColumnCounts(directory / "MemoryUB.csv"));
    CHECK(HasMatchingColumnCounts(directory / "PipeUtilization.csv"));

    std::filesystem::remove_all(directory);

    const auto sparseDirectory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_sparse_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    aclptiPmuDataResult sparseResult;
    aclptiPmuDataRow sparseRow{};
    sparseRow.blockId = 4;
    sparseRow.subBlockId = 5;
    sparseRow.coreData.push_back(Core(ACLPTI_CORE_TYPE_AIC, 0, 1000.0, {{0x424, 10.0}, {0x425, 0.0}}));
    sparseResult.pmuLogs.emplace(aclptiBlockKey{4, 5}, sparseRow);
    config.outputDirectory = sparseDirectory.string();
    CHECK(npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    const std::string sparseL2 = ReadFile(sparseDirectory / "L2Cache.csv");
    CHECK(sparseL2.find("4,5,1,1000,NA,NA") != std::string::npos);
    CHECK(sparseL2.find("10,0,NA,NA,NA,NA") != std::string::npos);
    std::filesystem::remove_all(sparseDirectory);

    const auto environmentDirectory = std::filesystem::temp_directory_path() /
                                      ("npu_compute_csv_environment_test_" +
                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const char* previousOutputDirectory = std::getenv("NPU_COMPUTE_CSV_OUTPUT_DIR");
    const std::string previousOutputDirectoryValue = previousOutputDirectory == nullptr ? "" : previousOutputDirectory;
    const bool hadPreviousOutputDirectory = previousOutputDirectory != nullptr;
    CHECK(setenv("NPU_COMPUTE_CSV_OUTPUT_DIR", environmentDirectory.c_str(), 1) == 0);
    config.outputDirectory.clear();
    CHECK(npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    CHECK(std::filesystem::exists(environmentDirectory / "L2Cache.csv"));
    if (hadPreviousOutputDirectory) {
        CHECK(setenv("NPU_COMPUTE_CSV_OUTPUT_DIR", previousOutputDirectoryValue.c_str(), 1) == 0);
    } else {
        CHECK(unsetenv("NPU_COMPUTE_CSV_OUTPUT_DIR") == 0);
    }
    std::filesystem::remove_all(environmentDirectory);
    return 0;
}
