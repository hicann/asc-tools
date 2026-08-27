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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <unistd.h>

#define CHECK(condition)                                                                      \
    do {                                                                                      \
        if (!(condition)) {                                                                   \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                         \
        }                                                                                     \
    } while (false)

namespace {

aclptiPmuDataRow::CoreData Core(
    aclptiCoreType type, uint8_t id, double cycles, std::initializer_list<std::pair<uint32_t, double>> values)
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

std::vector<std::filesystem::path> NestedCsvFiles(const std::filesystem::path& root, const std::string& filename)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().filename() == filename && entry.path().parent_path() != root) {
            paths.push_back(entry.path());
        }
    }
    return paths;
}

template <typename Function>
bool CaptureStderr(Function function, std::string* output)
{
    FILE* capture = std::tmpfile();
    if (capture == nullptr) {
        return false;
    }

    const int savedStderr = dup(STDERR_FILENO);
    if (savedStderr < 0) {
        std::fclose(capture);
        return false;
    }

    bool success = std::fflush(stderr) == 0 && dup2(fileno(capture), STDERR_FILENO) >= 0;
    if (success) {
        function();
        success = std::fflush(stderr) == 0;
    }
    success = dup2(savedStderr, STDERR_FILENO) >= 0 && success;
    close(savedStderr);

    if (success) {
        std::rewind(capture);
        char buffer[256];
        std::size_t count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), capture)) != 0) {
            output->append(buffer, count);
        }
        success = std::ferror(capture) == 0;
    }

    success = std::fclose(capture) == 0 && success;
    return success;
}

} // namespace

int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    aclptiPmuDataResult result;
    result.status = ACLPTI_SUCCESS;
    aclptiPmuDataRow aicRow{};
    aicRow.blockId = 2;
    aicRow.subBlockId = 3;
    aicRow.coreType = ACLPTI_CORE_TYPE_AIC;
    aicRow.coreId = 3;
    aicRow.coreData.push_back(Core(
        ACLPTI_CORE_TYPE_AIC, 3, 1000.0,
        {{0x424, 10.0}, {0x425, 2.0}, {0x426, 1.0}, {0x427, 5.0}, {0x428, 1.0}, {0x429, 1.0}}));
    aclptiPmuDataRow aivRow{};
    aivRow.blockId = 2;
    aivRow.subBlockId = 3;
    aivRow.coreType = ACLPTI_CORE_TYPE_AIV;
    aivRow.coreId = 3;
    aivRow.coreData.push_back(Core(
        ACLPTI_CORE_TYPE_AIV, 3, 2000.0,
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
    result.pmuLogs.emplace(aclptiBlockKey{2, 3, ACLPTI_CORE_TYPE_AIC, 3}, aicRow);
    result.pmuLogs.emplace(aclptiBlockKey{2, 3, ACLPTI_CORE_TYPE_AIV, 3}, aivRow);

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
    CHECK(l2.find("2,cube3,") != std::string::npos);
    CHECK(l2.find("2,vector3,") != std::string::npos);
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

    const auto splitFrequencyDirectory = std::filesystem::temp_directory_path() /
                                         ("npu_compute_csv_split_frequency_test_" +
                                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    config.outputDirectory = splitFrequencyDirectory.string();
    config.aicFrequencyMhz = 500.0;
    config.aivFrequencyMhz = 2000.0;
    CHECK(npu_compute::PmuCsvWriter::Write(result, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    const std::string splitFrequencyL2 = ReadFile(splitFrequencyDirectory / "L2Cache.csv");
    CHECK(splitFrequencyL2.find("2,cube3,2,1000,NA,NA") != std::string::npos);
    CHECK(splitFrequencyL2.find("2,vector3,NA,NA,1,2000") != std::string::npos);
    std::filesystem::remove_all(splitFrequencyDirectory);
    config.outputDirectory = directory.string();
    config.aicFrequencyMhz = 0.0;
    config.aivFrequencyMhz = 0.0;

    aclptiPmuDataResult secondResult = result;
    secondResult.pmuLogs.clear();
    aclptiPmuDataRow secondRow = aicRow;
    secondRow.blockId = 8;
    secondRow.subBlockId = 9;
    secondRow.coreId = 9;
    secondRow.coreData.clear();
    secondRow.coreData.push_back(Core(
        ACLPTI_CORE_TYPE_AIC, 9, 1000.0,
        {{0x424, 10.0}, {0x425, 2.0}, {0x426, 1.0}, {0x427, 5.0}, {0x428, 1.0}, {0x429, 1.0}}));
    secondResult.pmuLogs.emplace(aclptiBlockKey{8, 9, ACLPTI_CORE_TYPE_AIC, 9}, secondRow);
    CHECK(npu_compute::PmuCsvWriter::Write(secondResult, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    CHECK(ReadFile(directory / "L2Cache.csv") == l2);
    const std::vector<std::filesystem::path> nestedL2Files = NestedCsvFiles(directory, "L2Cache.csv");
    CHECK(nestedL2Files.size() == 1);
    CHECK(ReadFile(nestedL2Files[0]).find("8,cube9,") != std::string::npos);

    std::filesystem::remove_all(directory);

    const auto sparseDirectory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_sparse_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    aclptiPmuDataResult sparseResult;
    aclptiPmuDataRow sparseRow{};
    sparseRow.blockId = 4;
    sparseRow.subBlockId = 5;
    sparseRow.coreType = ACLPTI_CORE_TYPE_AIC;
    sparseRow.coreId = 5;
    sparseRow.coreData.push_back(Core(ACLPTI_CORE_TYPE_AIC, 5, 1000.0, {{0x424, 10.0}, {0x425, 0.0}}));
    sparseResult.pmuLogs.emplace(aclptiBlockKey{4, 5, ACLPTI_CORE_TYPE_AIC, 5}, sparseRow);
    config.outputDirectory = sparseDirectory.string();
    const char* previousDebug = std::getenv("NPU_COMPUTE_DEBUG");
    const std::string previousDebugValue = previousDebug == nullptr ? "" : previousDebug;
    const bool hadPreviousDebug = previousDebug != nullptr;
    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    std::string sparseLog;
    aclptiResult sparseWriteStatus = ACLPTI_ERROR_INTERNAL;
    CHECK(CaptureStderr(
        [&] { sparseWriteStatus = npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config); }, &sparseLog));
    CHECK(sparseWriteStatus == ACLPTI_SUCCESS);
    CHECK(sparseLog.find("CSV section data availability: section=L2Cache rows=1") != std::string::npos);
    CHECK(sparseLog.find("missingFields=") != std::string::npos);
    CHECK(sparseLog.find("missingReasons=") != std::string::npos);
    CHECK(sparseLog.find("missingColumnReasons=") != std::string::npos);
    CHECK(sparseLog.find("CoreTypeNotApplicable") != std::string::npos);
    CHECK(sparseLog.find("EventMissing") != std::string::npos);
    CHECK(sparseLog.find("aiv_time(us)@CoreTypeNotApplicable:1") != std::string::npos);
    CHECK(sparseLog.find("aic_read_close_victim@EventMissing:1") != std::string::npos);
    CHECK(sparseLog.find("aic_read_close_victim:1") != std::string::npos);
    CHECK(sparseLog.find("aiv_time(us):1") != std::string::npos);
    const std::string sparseL2 = ReadFile(sparseDirectory / "L2Cache.csv");
    CHECK(sparseL2.find("4,cube5,1,1000,NA,NA") != std::string::npos);
    CHECK(sparseL2.find("10,0,NA,NA,NA,NA") != std::string::npos);
    std::filesystem::remove_all(sparseDirectory);

    const auto mirrorPrimaryDirectory = std::filesystem::temp_directory_path() /
                                        ("npu_compute_csv_mirror_primary_test_" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto mirrorDirectory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_mirror_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    config.outputDirectory = mirrorPrimaryDirectory.string();
    config.mirrorOutputDirectory = mirrorDirectory.string();
    CHECK(npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    CHECK(std::filesystem::exists(mirrorPrimaryDirectory / "L2Cache.csv"));
    CHECK(std::filesystem::exists(mirrorDirectory / "L2Cache.csv"));
    CHECK(ReadFile(mirrorDirectory / "L2Cache.csv") == ReadFile(mirrorPrimaryDirectory / "L2Cache.csv"));
    std::filesystem::remove_all(mirrorPrimaryDirectory);
    std::filesystem::remove_all(mirrorDirectory);
    config.mirrorOutputDirectory.clear();

    const auto mirrorFailurePrimaryDirectory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_mirror_failure_primary_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto blockedMirrorDirectory = std::filesystem::temp_directory_path() /
                                        ("npu_compute_csv_blocked_mirror_test_" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream blockedOutput(blockedMirrorDirectory);
        blockedOutput << "not a directory\n";
    }
    config.outputDirectory = mirrorFailurePrimaryDirectory.string();
    config.mirrorOutputDirectory = blockedMirrorDirectory.string();
    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    std::string mirrorFailureLog;
    aclptiResult mirrorFailureStatus = ACLPTI_ERROR_INTERNAL;
    CHECK(CaptureStderr(
        [&] { mirrorFailureStatus = npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config); },
        &mirrorFailureLog));
    CHECK(mirrorFailureStatus == ACLPTI_SUCCESS);
    CHECK(std::filesystem::exists(mirrorFailurePrimaryDirectory / "L2Cache.csv"));
    CHECK(mirrorFailureLog.find("CSV mirror") != std::string::npos);
    std::filesystem::remove_all(mirrorFailurePrimaryDirectory);
    std::filesystem::remove(blockedMirrorDirectory);
    config.mirrorOutputDirectory.clear();

    const auto blockedDirectory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_blocked_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream blockedOutput(blockedDirectory);
        blockedOutput << "not a directory\n";
    }
    config.outputDirectory = blockedDirectory.string();
    std::string filesystemLog;
    aclptiResult filesystemStatus = ACLPTI_SUCCESS;
    CHECK(CaptureStderr(
        [&] { filesystemStatus = npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config); },
        &filesystemLog));
    CHECK(filesystemStatus == ACLPTI_ERROR_CSV_WRITE);
    CHECK(filesystemLog.find("CSV write rejected: output path is not a directory") != std::string::npos);
    CHECK(filesystemLog.find("path=" + blockedDirectory.string()) != std::string::npos);
    CHECK(filesystemLog.find("reason=") != std::string::npos);
    std::filesystem::remove(blockedDirectory);

    config.outputDirectory = "relative_csv_output";
    std::string relativeLog;
    aclptiResult relativeStatus = ACLPTI_SUCCESS;
    CHECK(CaptureStderr(
        [&] { relativeStatus = npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config); }, &relativeLog));
    CHECK(relativeStatus == ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(relativeLog.find("CSV write rejected: output path must be absolute") != std::string::npos);
    CHECK(relativeLog.find("path=relative_csv_output") != std::string::npos);
    if (hadPreviousDebug) {
        CHECK(setenv("NPU_COMPUTE_DEBUG", previousDebugValue.c_str(), 1) == 0);
    } else {
        CHECK(unsetenv("NPU_COMPUTE_DEBUG") == 0);
    }

    const auto emptyDirectory =
        std::filesystem::temp_directory_path() /
        ("npu_compute_csv_empty_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(std::filesystem::create_directories(emptyDirectory));
    const std::filesystem::path existingCsv = emptyDirectory / "L2Cache.csv";
    {
        std::ofstream existingOutput(existingCsv);
        existingOutput << "sentinel\n";
    }
    aclptiPmuDataResult emptyResult;
    config.outputDirectory = emptyDirectory.string();
    CHECK(npu_compute::PmuCsvWriter::Write(emptyResult, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    CHECK(ReadFile(existingCsv) == "sentinel\n");

    aclptiPmuDataResult failedEmptyResult;
    failedEmptyResult.status = ACLPTI_ERROR_DECODE;
    failedEmptyResult.errorStats.failedRecordCount = 1;
    CHECK(npu_compute::PmuCsvWriter::Write(failedEmptyResult, {"L2Cache"}, config) == ACLPTI_SUCCESS);
    CHECK(ReadFile(existingCsv) == "sentinel\n");

    std::filesystem::remove_all(emptyDirectory);

    CHECK(npu_compute::PmuCsvConfig{}.outputDirectory.empty());
    config.outputDirectory.clear();
    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    std::string emptyOutputLog;
    aclptiResult emptyOutputStatus = ACLPTI_SUCCESS;
    CHECK(CaptureStderr(
        [&] { emptyOutputStatus = npu_compute::PmuCsvWriter::Write(sparseResult, {"L2Cache"}, config); },
        &emptyOutputLog));
    CHECK(emptyOutputStatus == ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(emptyOutputLog.find("CSV write rejected: output path is empty") != std::string::npos);
    if (hadPreviousDebug) {
        CHECK(setenv("NPU_COMPUTE_DEBUG", previousDebugValue.c_str(), 1) == 0);
    } else {
        CHECK(unsetenv("NPU_COMPUTE_DEBUG") == 0);
    }
    return 0;
}
