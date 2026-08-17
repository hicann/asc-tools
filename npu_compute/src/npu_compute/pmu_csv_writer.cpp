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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>

namespace npu_compute {
namespace {

using Metric = std::optional<double>;

constexpr double kBytesPerGb = 1024.0 * 1024.0 * 1024.0;
constexpr double kBytesPerKb = 1024.0;

struct TypeMetrics {
    bool present = false;
    std::uint64_t sampleCount = 0;
    double totalCyclesSum = 0.0;
    bool overflow = false;
    std::map<std::uint32_t, double> valueSums;
    std::map<std::uint32_t, std::uint64_t> valueCounts;

    double Cycles() const { return sampleCount == 0 ? 0.0 : totalCyclesSum / static_cast<double>(sampleCount); }

    Metric Value(std::uint32_t eventId) const
    {
        const auto count = valueCounts.find(eventId);
        if (count == valueCounts.end() || count->second == 0) {
            return std::nullopt;
        }
        return valueSums.at(eventId) / static_cast<double>(count->second);
    }
};

struct RowMetrics {
    aclptiBlockKey key{};
    TypeMetrics aic;
    TypeMetrics aiv;
};

std::string Number(double value)
{
    if (!std::isfinite(value)) {
        return "NA";
    }
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

std::string Number(const Metric& value) { return value.has_value() ? Number(*value) : "NA"; }

std::string Integer(std::uint16_t value) { return std::to_string(value); }

void AddValue(TypeMetrics& metrics, std::uint32_t eventId, double value, std::uint64_t count)
{
    metrics.valueSums[eventId] += value * static_cast<double>(count);
    metrics.valueCounts[eventId] += count;
}

void AddCore(TypeMetrics& metrics, const aclptiPmuDataRow::CoreData& core)
{
    const std::uint64_t sampleCount = core.sampleCount == 0 ? 1 : core.sampleCount;
    metrics.present = true;
    metrics.sampleCount += sampleCount;
    metrics.totalCyclesSum += core.totalCycles * static_cast<double>(sampleCount);
    metrics.overflow = metrics.overflow || core.overflow;
    for (const auto& [eventId, value] : core.values) {
        const auto countIt = core.valueCounts.find(eventId);
        const std::uint64_t count =
            countIt == core.valueCounts.end() || countIt->second == 0 ? sampleCount : countIt->second;
        AddValue(metrics, eventId, value, count);
    }
}

RowMetrics MakeRowMetrics(aclptiBlockKey key, const aclptiPmuDataRow& row)
{
    RowMetrics metrics;
    metrics.key = key;
    if (!row.coreData.empty()) {
        for (const auto& core : row.coreData) {
            AddCore(core.coreType == ACLPTI_CORE_TYPE_AIC ? metrics.aic : metrics.aiv, core);
        }
        return metrics;
    }

    aclptiPmuDataRow::CoreData fallback{};
    fallback.coreType = row.coreType;
    fallback.coreId = row.coreId;
    fallback.sampleCount = 1;
    fallback.totalCycles = row.totalCycles;
    fallback.overflow = row.overflow;
    fallback.values = row.values;
    for (const auto& [eventId, value] : row.values) {
        static_cast<void>(value);
        fallback.valueCounts.emplace(eventId, 1);
    }
    AddCore(row.coreType == ACLPTI_CORE_TYPE_AIC ? metrics.aic : metrics.aiv, fallback);
    return metrics;
}

std::optional<double> Ratio(double numerator, double denominator)
{
    if (denominator == 0.0) {
        return std::nullopt;
    }
    return numerator / denominator;
}

std::optional<double> Time(const TypeMetrics& metrics, double frequencyMhz)
{
    if (!metrics.present || frequencyMhz <= 0.0) {
        return std::nullopt;
    }
    return metrics.Cycles() / frequencyMhz;
}

std::optional<double> ValueRatio(const TypeMetrics& metrics, std::uint32_t eventId)
{
    const auto value = metrics.Value(eventId);
    if (!metrics.present || !value.has_value()) {
        return std::nullopt;
    }
    return Ratio(*value, metrics.Cycles());
}

std::optional<double> IcacheMissRate(const TypeMetrics& metrics)
{
    const auto numerator = metrics.Value(0x35U);
    const auto denominator = metrics.Value(0x34U);
    if (!metrics.present || !numerator.has_value() || !denominator.has_value()) {
        return std::nullopt;
    }
    return Ratio(*numerator, *denominator);
}

std::optional<double> EventTime(const TypeMetrics& metrics, std::uint32_t eventId, double frequencyMhz)
{
    const auto value = metrics.Value(eventId);
    if (!metrics.present || !value.has_value() || frequencyMhz <= 0.0) {
        return std::nullopt;
    }
    return *value / frequencyMhz;
}

std::optional<double> Bandwidth(double bytes, double durationUs)
{
    if (durationUs <= 0.0) {
        return std::nullopt;
    }
    return bytes * 1000000.0 / kBytesPerGb / durationUs;
}

std::optional<double> EventBandwidth(
    const TypeMetrics& metrics, std::uint32_t eventId, double bytesPerEvent, double frequencyMhz)
{
    const auto value = metrics.Value(eventId);
    const auto duration = Time(metrics, frequencyMhz);
    if (!value.has_value() || !duration.has_value()) {
        return std::nullopt;
    }
    return Bandwidth(*value * bytesPerEvent, *duration);
}

std::optional<double> ActiveBandwidth(double bytes, double activeCycles, double frequencyMhz)
{
    if (frequencyMhz <= 0.0 || activeCycles <= 0.0) {
        return std::nullopt;
    }
    return Bandwidth(bytes, activeCycles / frequencyMhz);
}

Metric EventSum(const TypeMetrics& first, const TypeMetrics& second, std::uint32_t eventId)
{
    const auto firstValue = first.Value(eventId);
    const auto secondValue = second.Value(eventId);
    if (!firstValue.has_value() || !secondValue.has_value()) {
        return std::nullopt;
    }
    return *firstValue + *secondValue;
}

Metric SumEvents(const TypeMetrics& metrics, std::initializer_list<std::uint32_t> events)
{
    double sum = 0.0;
    for (const std::uint32_t event : events) {
        const auto value = metrics.Value(event);
        if (!value.has_value()) {
            return std::nullopt;
        }
        sum += *value;
    }
    return sum;
}

Metric NonNegativeDifference(
    const TypeMetrics& metrics, std::uint32_t minuend, std::initializer_list<std::uint32_t> subtrahends)
{
    const auto first = metrics.Value(minuend);
    if (!first.has_value()) {
        return std::nullopt;
    }
    double result = *first;
    for (const std::uint32_t event : subtrahends) {
        const auto value = metrics.Value(event);
        if (!value.has_value()) {
            return std::nullopt;
        }
        result -= *value;
    }
    return std::max(0.0, result);
}

Metric Scale(const Metric& value, double factor) { return value.has_value() ? Metric(*value * factor) : std::nullopt; }

Metric ActiveBandwidthMetric(const Metric& bytes, const Metric& activeCycles, double frequencyMhz)
{
    if (!bytes.has_value() || !activeCycles.has_value()) {
        return std::nullopt;
    }
    return ActiveBandwidth(*bytes, *activeCycles, frequencyMhz);
}

std::optional<double> UsageRate(const std::optional<double>& bandwidth, double maxBandwidth)
{
    if (!bandwidth.has_value() || maxBandwidth <= 0.0) {
        return std::nullopt;
    }
    return 100.0 * std::min(*bandwidth, maxBandwidth) / maxBandwidth;
}

double MaxBandwidth(std::string_view soc, std::string_view kind)
{
    const bool is959 = soc.find("959") != std::string_view::npos;
    if (kind == "GM_TO_L1") {
        return is959 ? 177.57 : 162.77;
    }
    if (kind == "L0C_TO_L1") {
        return is959 ? 412.15 : 377.80;
    }
    if (kind == "L0C_TO_GM") {
        return is959 ? 142.05 : 130.21;
    }
    if (kind == "GM_TO_UB") {
        return is959 ? 177.96 : 163.13;
    }
    if (kind == "UB_TO_GM") {
        return is959 ? 143.74 : 131.76;
    }
    return 0.0;
}

void AppendCommon(std::vector<std::string>& values, const RowMetrics& row, double frequencyMhz)
{
    values.push_back(Integer(row.key.blockId));
    values.push_back(Integer(row.key.subBlockId));
    const auto aicTime = Time(row.aic, frequencyMhz);
    const auto aivTime = Time(row.aiv, frequencyMhz);
    values.push_back(aicTime.has_value() ? Number(*aicTime) : "NA");
    values.push_back(row.aic.present ? Number(row.aic.Cycles()) : "NA");
    values.push_back(aivTime.has_value() ? Number(*aivTime) : "NA");
    values.push_back(row.aiv.present ? Number(row.aiv.Cycles()) : "NA");
}

void AppendMetric(std::vector<std::string>& values, const Metric& metric)
{
    values.push_back(metric.has_value() ? Number(*metric) : "NA");
}

std::vector<std::string> L2Header()
{
    const std::vector<std::string> suffixes = {
        "read_close_hit",  "read_close_miss",  "read_close_victim", "read_far_hit",     "read_far_miss",
        "read_far_victim", "read_hit_rate(%)", "write_close_hit",   "write_close_miss", "write_close_victim",
        "write_far_hit",   "write_far_miss",   "write_far_victim",  "write_hit_rate(%)"};
    std::vector<std::string> header = {"block_id",         "sub_block_id", "aic_time(us)",
                                       "aic_total_cycles", "aiv_time(us)", "aiv_total_cycles"};
    for (const char* prefix : {"aic_", "aiv_"}) {
        for (const auto& suffix : suffixes) {
            header.emplace_back(std::string(prefix) + suffix);
        }
    }
    return header;
}

std::vector<std::string> L2Row(const RowMetrics& row, double frequencyMhz)
{
    std::vector<std::string> values;
    AppendCommon(values, row, frequencyMhz);
    for (const TypeMetrics* metrics : {&row.aic, &row.aiv}) {
        for (const std::uint32_t event : {0x424U, 0x425U, 0x426U, 0x427U, 0x428U, 0x429U}) {
            values.push_back(Number(metrics->Value(event)));
        }
        const auto readHit = SumEvents(*metrics, {0x424U, 0x427U});
        const auto readTotal = SumEvents(*metrics, {0x424U, 0x425U, 0x426U, 0x427U, 0x428U, 0x429U});
        AppendMetric(
            values, !readHit.has_value() || !readTotal.has_value() ? std::nullopt :
                    *readTotal == 0.0                              ? Metric(0.0) :
                                                                     Metric(100.0 * *readHit / *readTotal));
        for (const std::uint32_t event : {0x42aU, 0x42bU, 0x42cU, 0x42dU, 0x42eU, 0x42fU}) {
            values.push_back(Number(metrics->Value(event)));
        }
        const auto writeHit = SumEvents(*metrics, {0x42aU, 0x42dU});
        const auto writeTotal = SumEvents(*metrics, {0x42aU, 0x42bU, 0x42cU, 0x42dU, 0x42eU, 0x42fU});
        AppendMetric(
            values, !writeHit.has_value() || !writeTotal.has_value() ? std::nullopt :
                    *writeTotal == 0.0                               ? Metric(0.0) :
                                                                       Metric(100.0 * *writeHit / *writeTotal));
    }
    return values;
}

std::vector<std::string> MemoryHeader()
{
    return {
        "block_id",
        "sub_block_id",
        "aic_time(us)",
        "aic_total_cycles",
        "aiv_time(us)",
        "aiv_total_cycles",
        "aiv_ub_to_gm_bw(GB/s)",
        "aiv_gm_to_ub_bw(GB/s)",
        "aic_l1_read_bw(GB/s)",
        "aic_l1_write_bw(GB/s)",
        "aic_main_mem_read_bw(GB/s)",
        "aic_main_mem_write_bw(GB/s)",
        "aic_mte1_instructions",
        "aic_mte1_ratio",
        "aic_mte2_instructions",
        "aic_mte2_ratio",
        "aic_mte3_instructions",
        "aic_mte3_ratio",
        "aiv_main_mem_read_bw(GB/s)",
        "aiv_main_mem_write_bw(GB/s)",
        "aiv_mte2_instructions",
        "aiv_mte2_ratio",
        "aiv_mte3_instructions",
        "aiv_mte3_ratio",
        "read_main_memory_datas(KB)",
        "write_main_memory_datas(KB)",
        "GM_to_L1_datas(KB)",
        "L0C_to_L1_datas(KB)",
        "L0C_to_GM_datas(KB)",
        "GM_to_UB_datas(KB)",
        "UB_to_GM_datas(KB)",
        "GM_to_L1_bw_usage_rate(%)",
        "L0C_to_L1_bw_usage_rate(%)",
        "L0C_to_GM_bw_usage_rate(%)",
        "GM_to_UB_bw_usage_rate(%)",
        "UB_to_GM_bw_usage_rate(%)"};
}

std::vector<std::string> MemoryRow(const RowMetrics& row, double frequencyMhz, std::string_view soc)
{
    std::vector<std::string> values;
    AppendCommon(values, row, frequencyMhz);
    const auto duration = Time(row.aiv, frequencyMhz);
    const auto xgu = NonNegativeDifference(row.aiv, 0x422U, {0x57fU, 0x580U});
    AppendMetric(values, std::nullopt); // DBI UB_TO_GM_DATA is not available yet.
    AppendMetric(values, xgu.has_value() && duration.has_value() ? Bandwidth(*xgu * 128.0, *duration) : std::nullopt);
    AppendMetric(values, EventBandwidth(row.aic, 0x707U, 256.0, frequencyMhz));
    AppendMetric(values, EventBandwidth(row.aic, 0x709U, 128.0, frequencyMhz));
    AppendMetric(values, EventBandwidth(row.aic, 0x422U, 128.0, frequencyMhz));
    AppendMetric(values, EventBandwidth(row.aic, 0x423U, 128.0, frequencyMhz));
    values.push_back(Number(row.aic.Value(0x700U)));
    AppendMetric(values, ValueRatio(row.aic, 0x702U));
    values.push_back(Number(row.aic.Value(0x200U)));
    AppendMetric(values, ValueRatio(row.aic, 0x202U));
    values.push_back(Number(row.aic.Value(0x201U)));
    AppendMetric(values, ValueRatio(row.aic, 0x203U));
    AppendMetric(values, EventBandwidth(row.aiv, 0x422U, 128.0, frequencyMhz));
    AppendMetric(values, EventBandwidth(row.aiv, 0x423U, 128.0, frequencyMhz));
    values.push_back(Number(row.aiv.Value(0x200U)));
    AppendMetric(values, ValueRatio(row.aiv, 0x202U));
    values.push_back(Number(row.aiv.Value(0x201U)));
    AppendMetric(values, ValueRatio(row.aiv, 0x203U));
    const auto mainRead = EventSum(row.aic, row.aiv, 0x422U);
    const auto mainWrite = EventSum(row.aic, row.aiv, 0x423U);
    values.push_back(Number(Scale(mainRead, 128.0 / kBytesPerKb)));
    values.push_back(Number(Scale(mainWrite, 128.0 / kBytesPerKb)));
    values.push_back(Number(Scale(row.aic.Value(0x422U), 128.0 / kBytesPerKb)));
    values.push_back(Number(Scale(row.aic.Value(0x70eU), 128.0 / kBytesPerKb)));
    values.push_back(Number(Scale(row.aic.Value(0x423U), 128.0 / kBytesPerKb)));
    values.push_back(Number(Scale(xgu, 128.0 / kBytesPerKb)));
    AppendMetric(values, std::nullopt);
    AppendMetric(
        values, UsageRate(EventBandwidth(row.aic, 0x422U, 128.0, frequencyMhz), MaxBandwidth(soc, "GM_TO_L1")));
    AppendMetric(
        values, UsageRate(EventBandwidth(row.aic, 0x70eU, 128.0, frequencyMhz), MaxBandwidth(soc, "L0C_TO_L1")));
    AppendMetric(
        values, UsageRate(EventBandwidth(row.aic, 0x423U, 128.0, frequencyMhz), MaxBandwidth(soc, "L0C_TO_GM")));
    AppendMetric(
        values, UsageRate(
                    xgu.has_value() && duration.has_value() ? Bandwidth(*xgu * 128.0, *duration) : std::nullopt,
                    MaxBandwidth(soc, "GM_TO_UB")));
    AppendMetric(values, std::nullopt);
    return values;
}

std::vector<std::string> MemoryL0Header()
{
    return {
        "block_id",
        "sub_block_id",
        "aic_time(us)",
        "aic_total_cycles",
        "aiv_time(us)",
        "aiv_total_cycles",
        "aic_l0a_read_bw(GB/s)",
        "aic_l0a_write_bw(GB/s)",
        "aic_l0b_read_bw(GB/s)",
        "aic_l0b_write_bw(GB/s)",
        "aic_l0c_read_bw_cube(GB/s)",
        "aic_l0c_write_bw_cube(GB/s)"};
}

std::vector<std::string> MemoryL0Row(const RowMetrics& row, double frequencyMhz)
{
    std::vector<std::string> values;
    AppendCommon(values, row, frequencyMhz);
    for (const auto& [event, bytes] : std::initializer_list<std::pair<std::uint32_t, double>>{
             {0x304U, 64.0}, {0x703U, 256.0}, {0x306U, 256.0}, {0x705U, 256.0}, {0x30aU, 1024.0}, {0x308U, 1024.0}}) {
        AppendMetric(values, EventBandwidth(row.aic, event, bytes, frequencyMhz));
    }
    return values;
}

std::vector<std::string> MemoryUBHeader()
{
    return {
        "block_id",
        "sub_block_id",
        "aic_time(us)",
        "aic_total_cycles",
        "aiv_time(us)",
        "aiv_total_cycles",
        "aiv_ub_read_bw_vector(GB/s)",
        "aiv_ub_write_bw_vector(GB/s)",
        "aiv_ub_write_bw_gm(GB/s)",
        "aiv_ub_read_bw_gm(GB/s)"};
}

std::vector<std::string> MemoryUBRow(const RowMetrics& row, double frequencyMhz)
{
    std::vector<std::string> values;
    AppendCommon(values, row, frequencyMhz);
    AppendMetric(values, EventBandwidth(row.aiv, 0x571U, 256.0, frequencyMhz));
    AppendMetric(values, EventBandwidth(row.aiv, 0x572U, 256.0, frequencyMhz));
    const auto xgu = NonNegativeDifference(row.aiv, 0x422U, {0x57fU, 0x580U});
    const auto duration = Time(row.aiv, frequencyMhz);
    AppendMetric(values, xgu.has_value() && duration.has_value() ? Bandwidth(*xgu * 128.0, *duration) : std::nullopt);
    // UB_TO_GM_DATA is supplied by DBI, which is not part of aclptiPmuDataResult yet.
    AppendMetric(values, std::nullopt);
    return values;
}

std::vector<std::string> PipeHeader()
{
    return {
        "block_id",
        "sub_block_id",
        "aic_time(us)",
        "aic_total_cycles",
        "aiv_time(us)",
        "aiv_total_cycles",
        "aiv_vec_time(us)",
        "aiv_vec_ratio",
        "aic_cube_time(us)",
        "aic_cube_ratio",
        "aic_scalar_time(us)",
        "aic_scalar_ratio",
        "aiv_scalar_time(us)",
        "aiv_scalar_ratio",
        "aic_fixpipe_time(us)",
        "aic_fixpipe_ratio",
        "aic_mte1_time(us)",
        "aic_mte1_ratio",
        "aic_mte2_time(us)",
        "aic_mte2_ratio",
        "aiv_mte2_time(us)",
        "aiv_mte2_ratio",
        "aic_mte3_time(us)",
        "aic_mte3_ratio",
        "aiv_mte3_time(us)",
        "aiv_mte3_ratio",
        "aic_icache_miss_rate",
        "aiv_icache_miss_rate",
        "aic_mte3_active_bw(GB/s)",
        "aiv_mte3_active_bw(GB/s)",
        "aic_fixpipe_active_bw(GB/s)",
        "aiv_mte2_active_bw(GB/s)",
        "aic_mte1_active_bw(GB/s)",
        "aic_mte2_active_bw(GB/s)"};
}

std::vector<std::string> PipeRow(const RowMetrics& row, double frequencyMhz)
{
    std::vector<std::string> values;
    AppendCommon(values, row, frequencyMhz);
    AppendMetric(values, EventTime(row.aiv, 0x501U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x501U));
    AppendMetric(values, EventTime(row.aic, 0x32aU, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x32aU));
    AppendMetric(values, EventTime(row.aic, 0x1U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x1U));
    AppendMetric(values, EventTime(row.aiv, 0x1U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x1U));
    AppendMetric(values, EventTime(row.aic, 0x714U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x714U));
    AppendMetric(values, EventTime(row.aic, 0x702U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x702U));
    AppendMetric(values, EventTime(row.aic, 0x202U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x202U));
    AppendMetric(values, EventTime(row.aiv, 0x202U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x202U));
    AppendMetric(values, EventTime(row.aic, 0x203U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x203U));
    AppendMetric(values, EventTime(row.aiv, 0x203U, frequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x203U));
    AppendMetric(values, IcacheMissRate(row.aic));
    AppendMetric(values, IcacheMissRate(row.aiv));

    const auto xgu = NonNegativeDifference(row.aiv, 0x422U, {0x57fU, 0x580U});
    const auto xulBase = NonNegativeDifference(row.aic, 0x709U, {0x70eU});
    const auto aicMainRead = row.aic.Value(0x422U);
    const auto xul = xulBase.has_value() && aicMainRead.has_value() ?
                         Metric(std::max(0.0, *xulBase - std::floor(*aicMainRead / 2.0))) :
                         std::nullopt;
    const auto xfp = SumEvents(row.aic, {0x70cU, 0x70eU, 0x717U, 0x423U});
    AppendMetric(values, ActiveBandwidthMetric(Scale(xul, 256.0), row.aic.Value(0x203U), frequencyMhz));
    AppendMetric(values, std::nullopt);
    AppendMetric(values, ActiveBandwidthMetric(Scale(xfp, 128.0), row.aic.Value(0x714U), frequencyMhz));
    AppendMetric(values, ActiveBandwidthMetric(Scale(xgu, 128.0), row.aiv.Value(0x202U), frequencyMhz));
    AppendMetric(
        values, ActiveBandwidthMetric(Scale(row.aic.Value(0x707U), 256.0), row.aic.Value(0x702U), frequencyMhz));
    AppendMetric(
        values, ActiveBandwidthMetric(Scale(row.aic.Value(0x422U), 128.0), row.aic.Value(0x202U), frequencyMhz));
    return values;
}

struct SectionWriter {
    std::vector<std::string> (*header)();
    std::function<std::vector<std::string>(const RowMetrics&, double, std::string_view)> row;
};

const SectionWriter* FindWriter(std::string_view section)
{
    static const std::map<std::string_view, SectionWriter> writers = {
        {"L2Cache",
         {&L2Header, [](const RowMetrics& row, double frequency, std::string_view) { return L2Row(row, frequency); }}},
        {"Memory",
         {&MemoryHeader, [](const RowMetrics& row, double frequency,
                            std::string_view soc) { return MemoryRow(row, frequency, soc); }}},
        {"MemoryL0",
         {&MemoryL0Header,
          [](const RowMetrics& row, double frequency, std::string_view) { return MemoryL0Row(row, frequency); }}},
        {"MemoryUB",
         {&MemoryUBHeader,
          [](const RowMetrics& row, double frequency, std::string_view) { return MemoryUBRow(row, frequency); }}},
        {"PipeUtilization",
         {&PipeHeader,
          [](const RowMetrics& row, double frequency, std::string_view) { return PipeRow(row, frequency); }}},
    };
    const auto it = writers.find(section);
    return it == writers.end() ? nullptr : &it->second;
}

void WriteCsvLine(std::ostream& output, const std::vector<std::string>& values)
{
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << values[index];
    }
    output << '\n';
}

std::string ResolveOutputDirectory(const PmuCsvConfig& config)
{
    if (!config.outputDirectory.empty()) {
        return config.outputDirectory;
    }
    const char* environmentDirectory = std::getenv("NPU_COMPUTE_CSV_OUTPUT_DIR");
    return environmentDirectory == nullptr ? "" : environmentDirectory;
}

} // namespace

aclptiResult PmuCsvWriter::Write(
    const aclptiPmuDataResult& result, const std::vector<std::string>& sections, const PmuCsvConfig& config)
{
    const std::string outputDirectory = ResolveOutputDirectory(config);
    if (outputDirectory.empty() || config.frequencyMhz <= 0.0 || !std::isfinite(config.frequencyMhz)) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    try {
        std::filesystem::create_directories(outputDirectory);
        for (const auto& section : sections) {
            if (FindWriter(section) == nullptr) {
                return ACLPTI_ERROR_NOT_SUPPORTED;
            }
        }
        for (const auto& section : sections) {
            const SectionWriter* writer = FindWriter(section);
            const std::filesystem::path path = std::filesystem::path(outputDirectory) / (section + ".csv");
            std::ofstream output(path, std::ios::out | std::ios::trunc);
            if (!output.is_open()) {
                return ACLPTI_ERROR_CSV_WRITE;
            }
            WriteCsvLine(output, writer->header());
            for (const auto& [key, row] : result.pmuLogs) {
                WriteCsvLine(output, writer->row(MakeRowMetrics(key, row), config.frequencyMhz, config.socName));
            }
            output.flush();
            if (!output.good()) {
                return ACLPTI_ERROR_CSV_WRITE;
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        return ACLPTI_ERROR_CSV_WRITE;
    } catch (const std::bad_alloc&) {
        return ACLPTI_ERROR_INTERNAL;
    }
    return ACLPTI_SUCCESS;
}

} // namespace npu_compute
