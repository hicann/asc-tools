/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "report/report_writer.h"
#include "pmu/pmu_metric_builder.h"
#include "common/debug_log.h"

#include <boost/filesystem.hpp>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cxxabi.h>
#include <fcntl.h>
#include <iomanip>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <unistd.h>

namespace npucompute {
namespace {

using Metric = std::optional<double>;

std::string DemangleName(const std::string& name)
{
    if (name.compare(0, 2, "_Z") != 0) {
        return name;
    }
    int status = 0;
    const std::unique_ptr<char, decltype(&std::free)> demangled(
        abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status), &std::free);
    return status == 0 && demangled ? std::string(demangled.get()) : name;
}

std::string Quote(std::string_view value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '"';
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') {
            output << '\\' << character;
        } else if (character < 0x20) {
            output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(character);
        } else {
            output << character;
        }
    }
    output << '"';
    return output.str();
}

std::string Number(Metric value, bool integer = false)
{
    if (!value || !std::isfinite(*value)) {
        return "null";
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(integer ? 0 : 6) << *value;
    return output.str();
}

struct Average {
    double sum = 0;
    std::size_t count = 0;
    void Add(Metric value)
    {
        if (value && std::isfinite(*value)) {
            sum += *value;
            ++count;
        }
    }
    Metric Value() const { return count == 0 ? std::nullopt : Metric(sum / count); }
};

using Fields = std::map<std::string, Metric>;

Fields Section(
    std::ostream& output, const std::string& section, const aclptiProfilingDataResult& result,
    const ReportConfig& config)
{
    const auto header = PmuMetricBuilder::Header(section);
    std::vector<Average> averages(header.size());
    std::vector<bool> integers(header.size(), false);
    const auto duration = PmuMetricBuilder::OperationDuration(result, PmuDataLevel::Task);
    for (const auto& [key, row] : result.taskPmuLogs) {
        const auto fields = PmuMetricBuilder::Build(section, key, row, config, duration);
        for (std::size_t index = 2; index < fields.size(); ++index) {
            averages[index].Add(fields[index].value);
            integers[index] = integers[index] || fields[index].integer;
        }
    }
    Fields values;
    output << "{\"category\":" << Quote(section);
    for (std::size_t index = 2; index < header.size(); ++index) {
        values.emplace(header[index], averages[index].Value());
        output << ',' << Quote(header[index]) << ':' << Number(averages[index].Value(), integers[index]);
    }
    output << "}\n";
    return values;
}

Metric Sum(Metric left, Metric right)
{
    return !left && !right ? std::nullopt : Metric(left.value_or(0) + right.value_or(0));
}

std::pair<Metric, Metric> ParallelMetrics(const aclptiProfilingDataResult& result, const ReportConfig& config)
{
    struct Block {
        Metric time;
        Average aic;
        Average aiv;
    };
    std::map<uint16_t, Block> blocks;
    const auto header = PmuMetricBuilder::Header("PipeUtilization");
    for (const auto& [key, row] : result.pmuLogs) {
        const auto fields = PmuMetricBuilder::Build("PipeUtilization", key, row, config);
        auto& block = blocks[key.blockId];
        for (std::size_t index = 2; index < fields.size(); ++index) {
            const auto value = fields[index].value;
            if ((header[index] == "aic_time(us)" || header[index] == "aiv_time(us)") && value) {
                block.time = block.time ? std::max(*block.time, *value) : value;
            } else if (header[index] == "aic_cube_ratio") {
                block.aic.Add(value);
            } else if (header[index] == "aiv_vec_ratio") {
                block.aiv.Add(value);
            }
        }
    }
    Average utilization;
    Average times;
    for (const auto& [key, block] : blocks) {
        utilization.Add(Sum(block.aic.Value(), block.aiv.Value()));
        times.Add(block.time);
    }
    const auto mean = times.Value();
    Metric balance;
    if (mean && *mean > 0) {
        double variance = 0;
        for (const auto& [key, block] : blocks) {
            if (block.time) {
                const double delta = *block.time - *mean;
                variance += delta * delta;
            }
        }
        balance = 1.0 - std::sqrt(variance / times.count) / *mean;
    }
    return {utilization.Value(), balance};
}

Metric Frequency(bool hasAic, bool hasAiv, Metric aic, Metric aiv)
{
    if (hasAic && hasAiv) {
        return aic && aiv && *aic == *aiv ? aic : std::nullopt;
    }
    return hasAic ? aic : hasAiv ? aiv : std::nullopt;
}

void OpInfo(
    std::ostream& output, const aclptiProfilingDataResult& result, const std::vector<std::string>& sections,
    const ReportConfig& config, const KernelMetadata& metadata, Fields memory)
{
    bool hasAic = false;
    bool hasAiv = false;
    for (const auto& [key, row] : result.taskPmuLogs) {
        hasAic = hasAic || key.coreType == ACLPTI_CORE_TYPE_AIC;
        hasAiv = hasAiv || key.coreType == ACLPTI_CORE_TYPE_AIV;
    }
    const auto selected = [&](std::string_view section) {
        return std::find(sections.begin(), sections.end(), section) != sections.end();
    };
    const auto [utilization, balance] = ParallelMetrics(result, config);
    const Metric read = Sum(memory["aic_main_mem_read_bw(GB/s)"], memory["aiv_main_mem_read_bw(GB/s)"]);
    const Metric write = Sum(memory["aic_main_mem_write_bw(GB/s)"], memory["aiv_main_mem_write_bw(GB/s)"]);
    const Metric usage = read && write ? Metric((*read + *write) / 1600.0 * 100.0) : std::nullopt;
    output << "{\"category\":\"OpInfoSummary\",\"Op Name\":"
           << (metadata.name ? Quote(DemangleName(*metadata.name)) : "null") << ",\"Op Type\":"
           << (hasAic ? (hasAiv ? "\"mix\"" : "\"cube\"") :
               hasAiv ? "\"vector\"" :
                        "null");
    const auto field = [&](std::string_view name, Metric value, bool integer = false) {
        output << ',' << Quote(name) << ':' << Number(value, integer);
    };
    field("Task Duration(us)", PmuMetricBuilder::TaskDuration(result));
    field("Block Dim", metadata.blockDim ? Metric(*metadata.blockDim) : std::nullopt, true);
    field("Mix Block Dim", std::nullopt);
    field("Device Id", metadata.deviceId ? Metric(*metadata.deviceId) : std::nullopt, true);
    field("Pid", ::getpid(), true);
    field(
        "Current Freq", Frequency(
                            hasAic, hasAiv, config.aicFrequencyMhz > 0 ? config.aicFrequencyMhz : config.frequencyMhz,
                            config.aivFrequencyMhz > 0 ? config.aivFrequencyMhz : config.frequencyMhz));
    field("Rated Freq", Frequency(hasAic, hasAiv, metadata.ratedAicFrequencyMhz, metadata.ratedAivFrequencyMhz));
    field("aicore_parallel_utilization", selected("PipeUtilization") ? utilization : std::nullopt);
    field("aicore_parallel_balance", balance);
    field("aicore_gm_bw_theoretical(GB/s)", selected("Memory") ? Metric(1600) : std::nullopt, true);
    field("aicore_gm_read_bw(GB/s)", read);
    field("aicore_gm_write_bw(GB/s)", write);
    field("aicore_gm_bw_usage_rate(%)", usage);
    output << "}\n";
}

bool Publish(const boost::filesystem::path& directory, const std::string& content)
{
    std::string temporary = (directory / "summary.jsonl.tmp.XXXXXX").string();
    const int descriptor = ::mkstemp(temporary.data());
    if (descriptor < 0) {
        return false;
    }
    bool success = true;
    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto written = ::write(descriptor, content.data() + offset, content.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            success = false;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (::fsync(descriptor) != 0) {
        success = false;
    }
    if (::close(descriptor) != 0) {
        success = false;
    }
    if (success) {
        success = ::rename(temporary.c_str(), (directory / "summary.jsonl").c_str()) == 0;
    }
    if (!success) {
        ::unlink(temporary.c_str());
        return false;
    }
    const int directoryFd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directoryFd < 0) {
        return false;
    }
    success = ::fsync(directoryFd) == 0;
    return ::close(directoryFd) == 0 && success;
}

} // namespace

aclptiResult WriteSummaryJsonl(
    const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const ReportConfig& config,
    const KernelMetadata& metadata)
{
    try {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        Fields memory;
        for (const auto& section : sections) {
            if (PmuMetricBuilder::Header(section).empty()) {
                return ACLPTI_ERROR_NOT_SUPPORTED;
            }
            auto values = Section(output, section, result, config);
            if (section == "Memory") {
                memory = std::move(values);
            }
        }
        OpInfo(output, result, sections, config, metadata, std::move(memory));
        if (!Publish(config.outputDirectory, output.str())) {
            detail::DebugLog(
                "npu-compute", "summary publication failed: %s errno=%d", config.outputDirectory.c_str(), errno);
            return ACLPTI_ERROR_INTERNAL;
        }
        return ACLPTI_SUCCESS;
    } catch (const std::exception& error) {
        detail::DebugLog("npu-compute", "summary failed: %s", error.what());
        return ACLPTI_ERROR_INTERNAL;
    }
}

} // namespace npucompute
