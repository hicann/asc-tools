#include "report/report_writer.h"
#include "report/collection_file_validator.h"
#include "report/rep_directory_packer.h"
#include "report/rep_decoder.h"
#include "import/imported_profile_results.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unistd.h>

#define CHECK(condition)                                         \
    do {                                                         \
        if (!(condition)) {                                      \
            std::cerr << __LINE__ << ": " << #condition << '\n'; \
            return 1;                                            \
        }                                                        \
    } while (false)

std::vector<boost::property_tree::ptree> ReadSummary(const boost::filesystem::path& directory)
{
    std::ifstream input((directory / "summary.jsonl").string());
    std::vector<boost::property_tree::ptree> records;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        boost::property_tree::ptree record;
        boost::property_tree::read_json(stream, record);
        records.push_back(std::move(record));
    }
    return records;
}

int main()
{
    char directory[] = "/tmp/npu-summary-test-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    npucompute::ReportConfig config;
    config.outputDirectory = directory;
    config.frequencyMhz = 1000;
    aclptiProfilingDataResult result;
    aclptiPmuDataRow task{};
    task.coreType = ACLPTI_CORE_TYPE_AIV;
    task.totalCycles = 2000;
    task.values = {{0x422, 8}, {0x423, 4}};
    result.taskPmuLogs.emplace(aclptiBlockKey{0, 0, ACLPTI_CORE_TYPE_AIV, 0}, task);
    task.totalCycles = 9999;
    result.pmuLogs.emplace(aclptiBlockKey{0, 0, ACLPTI_CORE_TYPE_AIV, 0}, task);
    CHECK(npucompute::WritePmuReport(result, {"Memory"}, config) == ACLPTI_SUCCESS);
    std::ifstream input(std::string(directory) + "/summary.jsonl");
    CHECK(input.is_open());
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    CHECK(text.find("\"category\":\"Memory\"") == 1);
    CHECK(text.find("\"aiv_total_cycles\":2000") != std::string::npos);
    CHECK(text.find("9999") == std::string::npos);
    CHECK(text.find("block_id") == std::string::npos);
    CHECK(text.find("\"aic_time(us)\":null") != std::string::npos);
    CHECK(text.find("\"category\":\"OpInfoSummary\"") > text.find("\"category\":\"Memory\""));
    CHECK(text.find("aic_flops") == std::string::npos);
    CHECK(text.back() == '\n');
    std::string error;
    CHECK(npucompute::cli::ValidateCollectionFile(
        std::string(directory) + "/summary.jsonl", npucompute::cli::NpuRepFileType::Jsonl, &error));
    auto records = ReadSummary(directory);
    CHECK(records.size() == 2);
    CHECK(std::abs(records[0].get<double>("aiv_main_mem_read_bw(GB/s)") - 0.476837) < 1e-6);
    CHECK(records[1].get<std::string>("Op Name") == "null");
    CHECK(records[1].get<std::string>("aicore_parallel_utilization") == "null");
    CHECK(records[1].size() == 16);

    std::vector<uint8_t> encoded;
    CHECK(npucompute::cli::PackDirectoryToRep(directory, &encoded, &error));
    npucompute::cli::DecodedRep decoded;
    CHECK(npucompute::cli::DecodeRep(encoded, &decoded, &error));
    std::vector<npucompute::cli::ImportedProfileEntry> imported;
    for (const auto& entry : decoded.entries) {
        imported.push_back({entry.file_name, entry.file_type, entry.payload, {}});
    }
    const auto importDirectory = boost::filesystem::path(directory) / "imported";
    boost::filesystem::create_directory(importDirectory);
    CHECK(npucompute::cli::UnpackImportedProfileResults(imported, importDirectory, &error));
    CHECK(ReadSummary(importDirectory)[0].get<double>("aiv_total_cycles") == 2000);

    config.outputDirectory = (boost::filesystem::path(directory) / "all").string();
    npucompute::KernelMetadata metadata;
    metadata.name = "kernel\"\\\n\t中文";
    metadata.blockDim = 64;
    metadata.deviceId = 0;
    metadata.ratedAivFrequencyMhz = 1800;
    const std::vector<std::string> sections = {"PipeUtilization", "Memory", "L2Cache", "MemoryUB", "MemoryL0"};
    result.taskPmuLogs.begin()->second.values[0x501] = 1000;
    CHECK(npucompute::WritePmuReport(result, sections, config, metadata) == ACLPTI_SUCCESS);
    records = ReadSummary(config.outputDirectory);
    CHECK(records.size() == 6);
    for (std::size_t index = 0; index < sections.size(); ++index) {
        CHECK(records[index].get<std::string>("category") == sections[index]);
        CHECK(records[index].get<double>("aiv_total_cycles") == 2000);
    }
    CHECK(records.back().get<std::string>("category") == "OpInfoSummary");
    CHECK(records.back().get<std::string>("Op Name") == metadata.name);
    CHECK(records.back().get<uint64_t>("Block Dim") == 64);
    CHECK(records.back().get<double>("Rated Freq") == 1800);

    for (const auto& names : std::vector<std::pair<std::string, std::string>>{
             {"_Z9vectorAddPf", "vectorAdd(float*)"},
             {"vectorAdd(float*)", "vectorAdd(float*)"},
             {"_Zinvalid", "_Zinvalid"},
             {"", ""}}) {
        metadata.name = names.first;
        CHECK(npucompute::WriteSummaryJsonl(result, sections, config, metadata) == ACLPTI_SUCCESS);
        CHECK(ReadSummary(config.outputDirectory).back().get<std::string>("Op Name") == names.second);
    }

    config.pmuDataLevel = npucompute::PmuDataLevel::Task;
    CHECK(npucompute::WritePmuReport(result, sections, config, metadata) == ACLPTI_SUCCESS);
    bool foundCollection = false;
    for (const auto& entry : boost::filesystem::directory_iterator(config.outputDirectory)) {
        if (boost::filesystem::is_directory(entry.path())) {
            CHECK(ReadSummary(entry.path())[0].get<double>("aiv_total_cycles") == 2000);
            CHECK(boost::filesystem::exists(entry.path() / "Memory.csv"));
            foundCollection = true;
        }
    }
    CHECK(foundCollection);

    config.outputDirectory = (boost::filesystem::path(directory) / "missing").string();
    aclptiProfilingDataResult missing;
    missing.pmuLogs = result.pmuLogs;
    CHECK(npucompute::WritePmuReport(missing, {"Memory"}, config) == ACLPTI_SUCCESS);
    records = ReadSummary(config.outputDirectory);
    CHECK(records.size() == 2);
    CHECK(records[0].get<std::string>("aiv_total_cycles") == "null");
    CHECK(!boost::filesystem::exists(boost::filesystem::path(config.outputDirectory) / "Memory.csv"));

    config.outputDirectory = (boost::filesystem::path(directory) / "average").string();
    result.taskPmuLogs.begin()->second.values[0x501] = 1000;
    auto secondTask = result.taskPmuLogs.begin()->second;
    secondTask.blockId = 1;
    secondTask.totalCycles = 4000;
    secondTask.values[0x501] = 3200;
    result.taskPmuLogs.emplace(aclptiBlockKey{1, 0, ACLPTI_CORE_TYPE_AIV, 0}, secondTask);
    result.pmuLogs = result.taskPmuLogs;
    CHECK(npucompute::WritePmuReport(result, {"PipeUtilization"}, config) == ACLPTI_SUCCESS);
    records = ReadSummary(config.outputDirectory);
    CHECK(records[0].get<double>("aiv_total_cycles") == 3000);
    CHECK(std::abs(records[0].get<double>("aiv_vec_ratio") - 0.65) < 1e-6);
    CHECK(std::abs(records[1].get<double>("aicore_parallel_utilization") - 0.65) < 1e-6);
    CHECK(std::abs(records[1].get<double>("aicore_parallel_balance") - 2.0 / 3.0) < 1e-6);

    config.outputDirectory = (boost::filesystem::path(directory) / "mixed").string();
    aclptiPmuDataRow cube{};
    cube.coreType = ACLPTI_CORE_TYPE_AIC;
    cube.totalCycles = 1000;
    cube.values = {{0x422, 16}, {0x423, 8}};
    result.taskPmuLogs.emplace(aclptiBlockKey{}, cube);
    result.taskLogs[1] = {
        {0, 0, 1, 0, 100, 0, 0, ACLPTI_CORE_TYPE_AIC, 0}, {0, 1, 1, 0, 10100, 0, 0, ACLPTI_CORE_TYPE_AIC, 0}};
    config.aicFrequencyMhz = 1200;
    config.aivFrequencyMhz = 1000;
    CHECK(npucompute::WritePmuReport(result, {"Memory"}, config) == ACLPTI_SUCCESS);
    records = ReadSummary(config.outputDirectory);
    CHECK(records[1].get<std::string>("Op Type") == "mix");
    CHECK(records[1].get<double>("Task Duration(us)") == 10);
    CHECK(records[1].get<std::string>("Current Freq") == "null");
    CHECK(std::abs(records[1].get<double>("aicore_gm_read_bw(GB/s)") - 0.286102) < 1e-6);
    CHECK(std::abs(records[1].get<double>("aicore_gm_bw_usage_rate(%)") - 0.026822) < 1e-6);

    config.outputDirectory = (boost::filesystem::path(directory) / "zero").string();
    aclptiProfilingDataResult zero;
    cube.totalCycles = 0;
    cube.values = {{810, 0}};
    zero.taskPmuLogs.emplace(aclptiBlockKey{}, cube);
    zero.pmuLogs = zero.taskPmuLogs;
    CHECK(npucompute::WritePmuReport(zero, {"PipeUtilization"}, config) == ACLPTI_SUCCESS);
    records = ReadSummary(config.outputDirectory);
    CHECK(records[0].get<std::string>("aic_cube_ratio") == "null");
    CHECK(records[0].get<double>("aic_total_cycles") == 0);
    CHECK(records[1].get<std::string>("aicore_parallel_balance") == "null");

    const auto bad = boost::filesystem::path(directory) / "bad";
    boost::filesystem::create_directories(bad / "summary.jsonl");
    config.outputDirectory = bad.string();
    CHECK(npucompute::WriteSummaryJsonl(result, {"Memory"}, config, {}) != ACLPTI_SUCCESS);
    for (const auto& entry : boost::filesystem::directory_iterator(bad)) {
        CHECK(entry.path().filename() == "summary.jsonl");
    }
    const auto malformed = boost::filesystem::path(directory) / "summary.jsonl";
    for (const auto* invalid :
         {"{\"category\":\"Memory\"}\n", "not json\n",
          "{\"category\":\"OpInfoSummary\"}\n{\"category\":\"Memory\"}\n"}) {
        std::ofstream output(malformed.string());
        output << invalid;
        output.close();
        CHECK(!npucompute::cli::ValidateCollectionFile(malformed, npucompute::cli::NpuRepFileType::Jsonl, &error));
    }
    boost::filesystem::remove_all(directory);
    return 0;
}
