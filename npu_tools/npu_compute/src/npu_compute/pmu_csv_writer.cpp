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

#include "common/debug_log.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

#include <unistd.h>

namespace npu_compute {
namespace {

using Metric = std::optional<double>;

constexpr double kBytesPerGb = 1024.0 * 1024.0 * 1024.0;
constexpr double kBytesPerKb = 1024.0;

struct TypeMetrics {
    bool present = false;
    uint64_t sampleCount = 0;
    double totalCyclesSum = 0.0;
    bool overflow = false;
    std::map<uint32_t, double> valueSums;
    std::map<uint32_t, uint64_t> valueCounts;

    double Cycles() const { return sampleCount == 0 ? 0.0 : totalCyclesSum / static_cast<double>(sampleCount); }

    Metric Value(uint32_t eventId) const
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

struct CsvSectionStats {
    std::size_t rows = 0;
    std::size_t totalFields = 0;
    std::size_t missingFields = 0;
    std::size_t missingRows = 0;
    std::size_t mismatchedRows = 0;
    std::map<std::string, std::size_t> missingColumns;
    std::map<std::string, std::size_t> missingReasons;
    std::map<std::string, std::size_t> missingColumnReasons;
};

enum class MissingReason {
    None,
    Unknown,
    CoreTypeNotApplicable,
    EventMissing,
    InvalidDenominator,
    InvalidFrequencyOrDuration,
    DbiUnavailable,
    UnsupportedSocBandwidth,
    RowSizeMismatch,
};

struct CsvCell {
    std::string text;
    MissingReason missingReason = MissingReason::None;
};

using CsvRow = std::vector<CsvCell>;

const char* MissingReasonName(MissingReason reason)
{
    switch (reason) {
        case MissingReason::None:
            return "None";
        case MissingReason::Unknown:
            return "Unknown";
        case MissingReason::CoreTypeNotApplicable:
            return "CoreTypeNotApplicable";
        case MissingReason::EventMissing:
            return "EventMissing";
        case MissingReason::InvalidDenominator:
            return "InvalidDenominator";
        case MissingReason::InvalidFrequencyOrDuration:
            return "InvalidFrequencyOrDuration";
        case MissingReason::DbiUnavailable:
            return "DbiUnavailable";
        case MissingReason::UnsupportedSocBandwidth:
            return "UnsupportedSocBandwidth";
        case MissingReason::RowSizeMismatch:
            return "RowSizeMismatch";
    }
    return "Unknown";
}

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

std::string Integer(uint16_t value) { return std::to_string(value); }

CsvCell ValueCell(std::string value) { return CsvCell{std::move(value), MissingReason::None}; }

CsvCell MissingCell(MissingReason reason)
{
    return CsvCell{"NA", reason == MissingReason::None ? MissingReason::Unknown : reason};
}

CsvCell MetricCell(const Metric& value, MissingReason missingReason)
{
    return value.has_value() ? ValueCell(Number(*value)) : MissingCell(missingReason);
}

bool HasEvent(const TypeMetrics& metrics, uint32_t eventId) { return metrics.Value(eventId).has_value(); }

bool HasEvents(const TypeMetrics& metrics, std::initializer_list<uint32_t> events)
{
    for (const uint32_t event : events) {
        if (!HasEvent(metrics, event)) {
            return false;
        }
    }
    return true;
}

MissingReason EventReason(const TypeMetrics& metrics, std::initializer_list<uint32_t> events)
{
    if (!metrics.present) {
        return MissingReason::CoreTypeNotApplicable;
    }
    return HasEvents(metrics, events) ? MissingReason::Unknown : MissingReason::EventMissing;
}

MissingReason TimeReason(const TypeMetrics& metrics, double frequencyMhz)
{
    if (!metrics.present) {
        return MissingReason::CoreTypeNotApplicable;
    }
    return frequencyMhz <= 0.0 || !std::isfinite(frequencyMhz) ? MissingReason::InvalidFrequencyOrDuration :
                                                                 MissingReason::Unknown;
}

MissingReason RatioReason(const TypeMetrics& metrics, std::initializer_list<uint32_t> events)
{
    const MissingReason eventReason = EventReason(metrics, events);
    if (eventReason != MissingReason::Unknown) {
        return eventReason;
    }
    return metrics.Cycles() == 0.0 ? MissingReason::InvalidDenominator : MissingReason::Unknown;
}

MissingReason EventBandwidthReason(
    const TypeMetrics& metrics, std::initializer_list<uint32_t> events, double frequencyMhz)
{
    const MissingReason eventReason = EventReason(metrics, events);
    if (eventReason != MissingReason::Unknown) {
        return eventReason;
    }
    const MissingReason timeReason = TimeReason(metrics, frequencyMhz);
    return timeReason == MissingReason::Unknown && metrics.Cycles() == 0.0 ? MissingReason::InvalidFrequencyOrDuration :
                                                                             timeReason;
}

MissingReason EventSumReason(const TypeMetrics& first, const TypeMetrics& second, uint32_t eventId)
{
    if (!first.present || !second.present) {
        return MissingReason::CoreTypeNotApplicable;
    }
    return HasEvent(first, eventId) && HasEvent(second, eventId) ? MissingReason::Unknown : MissingReason::EventMissing;
}

MissingReason UsageRateReason(MissingReason bandwidthReason, double maxBandwidth)
{
    if (maxBandwidth <= 0.0) {
        return MissingReason::UnsupportedSocBandwidth;
    }
    return bandwidthReason == MissingReason::None ? MissingReason::Unknown : bandwidthReason;
}

std::string CoreSubBlockLabel(const aclptiBlockKey& key)
{
    std::ostringstream output;
    output << (key.coreType == ACLPTI_CORE_TYPE_AIC ? "cube" : "vector") << key.subBlockId;
    if (key.coreId != static_cast<uint8_t>(key.subBlockId)) {
        output << "_core" << static_cast<unsigned int>(key.coreId);
    }
    return output.str();
}

void AddValue(TypeMetrics& metrics, uint32_t eventId, double value, uint64_t count)
{
    metrics.valueSums[eventId] += value * static_cast<double>(count);
    metrics.valueCounts[eventId] += count;
}

void AddCore(TypeMetrics& metrics, const aclptiPmuDataRow::CoreData& core)
{
    const uint64_t sampleCount = core.sampleCount == 0 ? 1 : core.sampleCount;
    metrics.present = true;
    metrics.sampleCount += sampleCount;
    metrics.totalCyclesSum += core.totalCycles * static_cast<double>(sampleCount);
    metrics.overflow = metrics.overflow || core.overflow;
    for (const auto& [eventId, value] : core.values) {
        const auto countIt = core.valueCounts.find(eventId);
        const uint64_t count =
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

struct BlockCounts {
    uint32_t aic = 0;
    uint32_t aiv = 0;
};

// 从采集结果统计真实启动的 AIC / AIV block 数，按 (blockId, subBlockId) 去重。
// core type 判定方式与 MakeRowMetrics 保持一致：优先看 coreData，各 core 按自身
// coreType 归类；coreData 为空时退回用 row.coreType。
BlockCounts ComputeBlockCounts(const std::map<aclptiBlockKey, aclptiPmuDataRow>& pmuLogs)
{
    std::set<std::pair<uint16_t, uint16_t>> aicBlocks;
    std::set<std::pair<uint16_t, uint16_t>> aivBlocks;
    for (const auto& [key, row] : pmuLogs) {
        const std::pair<uint16_t, uint16_t> block{key.blockId, key.subBlockId};
        if (!row.coreData.empty()) {
            for (const auto& core : row.coreData) {
                (core.coreType == ACLPTI_CORE_TYPE_AIC ? aicBlocks : aivBlocks).insert(block);
            }
        } else {
            (row.coreType == ACLPTI_CORE_TYPE_AIC ? aicBlocks : aivBlocks).insert(block);
        }
    }
    return {static_cast<uint32_t>(aicBlocks.size()), static_cast<uint32_t>(aivBlocks.size())};
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

double MsprofTimeScale(uint32_t blockCount, uint32_t coreCount)
{
    if (blockCount == 0) {
        return 1.0;
    }
    const uint32_t waves = coreCount == 0 ? 1U : ((blockCount + coreCount - 1U) / coreCount);
    return static_cast<double>(waves) / static_cast<double>(blockCount);
}

std::optional<double> Time(const TypeMetrics& metrics, double frequencyMhz, uint32_t blockCount, uint32_t coreCount)
{
    if (!metrics.present || frequencyMhz <= 0.0) {
        return std::nullopt;
    }
    return metrics.Cycles() / frequencyMhz * MsprofTimeScale(blockCount, coreCount);
}

std::optional<double> ValueRatio(const TypeMetrics& metrics, uint32_t eventId)
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

std::optional<double> EventTime(const TypeMetrics& metrics, uint32_t eventId, double frequencyMhz)
{
    const auto value = metrics.Value(eventId);
    if (!metrics.present || !value.has_value() || frequencyMhz <= 0.0) {
        return std::nullopt;
    }
    return *value / frequencyMhz;
}

std::optional<double> EventTime(
    const TypeMetrics& metrics, uint32_t eventId, double frequencyMhz, uint32_t blockCount, uint32_t coreCount)
{
    const auto value = metrics.Value(eventId);
    if (!metrics.present || !value.has_value() || frequencyMhz <= 0.0) {
        return std::nullopt;
    }
    return *value / frequencyMhz * MsprofTimeScale(blockCount, coreCount);
}

uint32_t AicCubeEventId(const TypeMetrics& metrics)
{
    constexpr uint32_t kMsprofAicCubeEvent = 0x301U;
    constexpr uint32_t kA5AicCubeFallbackEvent = 0x32aU;
    const auto primary = metrics.Value(kMsprofAicCubeEvent);
    if (primary.has_value() && *primary != 0.0) {
        return kMsprofAicCubeEvent;
    }
    const auto fallback = metrics.Value(kA5AicCubeFallbackEvent);
    if (fallback.has_value() && *fallback != 0.0) {
        return kA5AicCubeFallbackEvent;
    }
    return kMsprofAicCubeEvent;
}

std::optional<double> Bandwidth(double bytes, double durationUs)
{
    if (durationUs <= 0.0) {
        return std::nullopt;
    }
    return bytes * 1000000.0 / kBytesPerGb / durationUs;
}

std::optional<double> EventBandwidth(
    const TypeMetrics& metrics, uint32_t eventId, double bytesPerEvent, double frequencyMhz)
{
    const auto value = metrics.Value(eventId);
    const auto duration = Time(metrics, frequencyMhz);
    if (!value.has_value() || !duration.has_value()) {
        return std::nullopt;
    }
    return Bandwidth(*value * bytesPerEvent, *duration);
}

std::optional<double> EventBandwidth(
    const TypeMetrics& metrics, uint32_t eventId, double bytesPerEvent, double frequencyMhz, uint32_t blockCount,
    uint32_t coreCount)
{
    const auto value = metrics.Value(eventId);
    const auto duration = Time(metrics, frequencyMhz, blockCount, coreCount);
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

std::optional<double> ActiveBandwidth(
    double bytes, double activeCycles, double frequencyMhz, uint32_t blockCount, uint32_t coreCount)
{
    if (frequencyMhz <= 0.0 || activeCycles <= 0.0) {
        return std::nullopt;
    }
    return Bandwidth(bytes, activeCycles / frequencyMhz * MsprofTimeScale(blockCount, coreCount));
}

Metric EventSum(const TypeMetrics& first, const TypeMetrics& second, uint32_t eventId)
{
    const auto firstValue = first.Value(eventId);
    const auto secondValue = second.Value(eventId);
    if (!firstValue.has_value() || !secondValue.has_value()) {
        return std::nullopt;
    }
    return *firstValue + *secondValue;
}

Metric SumEvents(const TypeMetrics& metrics, std::initializer_list<uint32_t> events)
{
    double sum = 0.0;
    for (const uint32_t event : events) {
        const auto value = metrics.Value(event);
        if (!value.has_value()) {
            return std::nullopt;
        }
        sum += *value;
    }
    return sum;
}

Metric NonNegativeDifference(const TypeMetrics& metrics, uint32_t minuend, std::initializer_list<uint32_t> subtrahends)
{
    const auto first = metrics.Value(minuend);
    if (!first.has_value()) {
        return std::nullopt;
    }
    double result = *first;
    for (const uint32_t event : subtrahends) {
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

Metric ActiveBandwidthMetric(
    const Metric& bytes, const Metric& activeCycles, double frequencyMhz, uint32_t blockCount, uint32_t coreCount)
{
    if (!bytes.has_value() || !activeCycles.has_value()) {
        return std::nullopt;
    }
    return ActiveBandwidth(*bytes, *activeCycles, frequencyMhz, blockCount, coreCount);
}

std::optional<double> UsageRate(const std::optional<double>& bandwidth, double maxBandwidth)
{
    if (!bandwidth.has_value() || maxBandwidth <= 0.0) {
        return std::nullopt;
    }
    return 100.0 * std::min(*bandwidth, maxBandwidth) / maxBandwidth;
}

double AicFrequencyMhz(const PmuCsvConfig& config)
{
    return config.aicFrequencyMhz > 0.0 ? config.aicFrequencyMhz : config.frequencyMhz;
}

double AivFrequencyMhz(const PmuCsvConfig& config)
{
    return config.aivFrequencyMhz > 0.0 ? config.aivFrequencyMhz : config.frequencyMhz;
}

uint32_t AicCoreCount(const PmuCsvConfig& config) { return config.aicCoreCount; }

uint32_t AivCoreCount(const PmuCsvConfig& config) { return config.aivCoreCount; }

uint32_t AicBlockCount(const PmuCsvConfig& config)
{
    return config.aicBlockCount != 0 ? config.aicBlockCount : config.aicCoreCount;
}

uint32_t AivBlockCount(const PmuCsvConfig& config)
{
    return config.aivBlockCount != 0 ? config.aivBlockCount : config.aivCoreCount;
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

void AppendCommon(CsvRow& values, const RowMetrics& row, const PmuCsvConfig& config)
{
    const double aicFrequencyMhz = AicFrequencyMhz(config);
    const double aivFrequencyMhz = AivFrequencyMhz(config);
    const uint32_t aicBlockCount = AicBlockCount(config);
    const uint32_t aivBlockCount = AivBlockCount(config);
    values.push_back(ValueCell(Integer(row.key.blockId)));
    values.push_back(ValueCell(CoreSubBlockLabel(row.key)));
    const auto aicTime = Time(row.aic, aicFrequencyMhz, aicBlockCount, AicCoreCount(config));
    const auto aivTime = Time(row.aiv, aivFrequencyMhz, aivBlockCount, AivCoreCount(config));
    values.push_back(MetricCell(aicTime, TimeReason(row.aic, aicFrequencyMhz)));
    values.push_back(
        row.aic.present ? ValueCell(Number(row.aic.Cycles())) : MissingCell(MissingReason::CoreTypeNotApplicable));
    values.push_back(MetricCell(aivTime, TimeReason(row.aiv, aivFrequencyMhz)));
    values.push_back(
        row.aiv.present ? ValueCell(Number(row.aiv.Cycles())) : MissingCell(MissingReason::CoreTypeNotApplicable));
}

void AppendMetric(CsvRow& values, const Metric& metric, MissingReason missingReason)
{
    values.push_back(MetricCell(metric, missingReason));
}

void AppendEvent(CsvRow& values, const TypeMetrics& metrics, uint32_t eventId)
{
    values.push_back(MetricCell(metrics.Value(eventId), EventReason(metrics, {eventId})));
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

CsvRow L2Row(const RowMetrics& row, const PmuCsvConfig& config)
{
    CsvRow values;
    AppendCommon(values, row, config);
    for (const TypeMetrics* metrics : {&row.aic, &row.aiv}) {
        for (const uint32_t event : {0x424U, 0x425U, 0x426U, 0x427U, 0x428U, 0x429U}) {
            AppendEvent(values, *metrics, event);
        }
        const auto readHit = SumEvents(*metrics, {0x424U, 0x427U});
        const auto readTotal = SumEvents(*metrics, {0x424U, 0x425U, 0x426U, 0x427U, 0x428U, 0x429U});
        AppendMetric(
            values,
            !readHit.has_value() || !readTotal.has_value() ? std::nullopt :
            *readTotal == 0.0                              ? Metric(0.0) :
                                                             Metric(100.0 * *readHit / *readTotal),
            EventReason(*metrics, {0x424U, 0x425U, 0x426U, 0x427U, 0x428U, 0x429U}));
        for (const uint32_t event : {0x42aU, 0x42bU, 0x42cU, 0x42dU, 0x42eU, 0x42fU}) {
            AppendEvent(values, *metrics, event);
        }
        const auto writeHit = SumEvents(*metrics, {0x42aU, 0x42dU});
        const auto writeTotal = SumEvents(*metrics, {0x42aU, 0x42bU, 0x42cU, 0x42dU, 0x42eU, 0x42fU});
        AppendMetric(
            values,
            !writeHit.has_value() || !writeTotal.has_value() ? std::nullopt :
            *writeTotal == 0.0                               ? Metric(0.0) :
                                                               Metric(100.0 * *writeHit / *writeTotal),
            EventReason(*metrics, {0x42aU, 0x42bU, 0x42cU, 0x42dU, 0x42eU, 0x42fU}));
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

CsvRow MemoryRow(const RowMetrics& row, const PmuCsvConfig& config)
{
    CsvRow values;
    const double aicFrequencyMhz = AicFrequencyMhz(config);
    const double aivFrequencyMhz = AivFrequencyMhz(config);
    const uint32_t aicCoreCount = AicCoreCount(config);
    const uint32_t aivCoreCount = AivCoreCount(config);
    const uint32_t aicBlockCount = AicBlockCount(config);
    const uint32_t aivBlockCount = AivBlockCount(config);
    AppendCommon(values, row, config);
    const auto duration = Time(row.aiv, aivFrequencyMhz, aivBlockCount, aivCoreCount);
    const auto xgu = NonNegativeDifference(row.aiv, 0x422U, {0x57fU, 0x580U});
    const MissingReason xguReason = EventBandwidthReason(row.aiv, {0x422U, 0x57fU, 0x580U}, aivFrequencyMhz);
    AppendMetric(
        values, std::nullopt, row.aiv.present ? MissingReason::DbiUnavailable : MissingReason::CoreTypeNotApplicable);
    AppendMetric(
        values, xgu.has_value() && duration.has_value() ? Bandwidth(*xgu * 128.0, *duration) : std::nullopt, xguReason);
    AppendMetric(
        values, EventBandwidth(row.aic, 0x707U, 256.0, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x707U}, aicFrequencyMhz));
    AppendMetric(
        values, EventBandwidth(row.aic, 0x709U, 128.0, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x709U}, aicFrequencyMhz));
    AppendMetric(
        values, EventBandwidth(row.aic, 0x422U, 128.0, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x422U}, aicFrequencyMhz));
    AppendMetric(
        values, EventBandwidth(row.aic, 0x423U, 128.0, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x423U}, aicFrequencyMhz));
    AppendEvent(values, row.aic, 0x700U);
    AppendMetric(values, ValueRatio(row.aic, 0x702U), RatioReason(row.aic, {0x702U}));
    AppendEvent(values, row.aic, 0x200U);
    AppendMetric(values, ValueRatio(row.aic, 0x202U), RatioReason(row.aic, {0x202U}));
    AppendEvent(values, row.aic, 0x201U);
    AppendMetric(values, ValueRatio(row.aic, 0x203U), RatioReason(row.aic, {0x203U}));
    AppendMetric(
        values, EventBandwidth(row.aiv, 0x422U, 128.0, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x422U}, aivFrequencyMhz));
    AppendMetric(
        values, EventBandwidth(row.aiv, 0x423U, 128.0, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x423U}, aivFrequencyMhz));
    AppendEvent(values, row.aiv, 0x200U);
    AppendMetric(values, ValueRatio(row.aiv, 0x202U), RatioReason(row.aiv, {0x202U}));
    AppendEvent(values, row.aiv, 0x201U);
    AppendMetric(values, ValueRatio(row.aiv, 0x203U), RatioReason(row.aiv, {0x203U}));
    const auto mainRead = EventSum(row.aic, row.aiv, 0x422U);
    const auto mainWrite = EventSum(row.aic, row.aiv, 0x423U);
    values.push_back(MetricCell(Scale(mainRead, 128.0 / kBytesPerKb), EventSumReason(row.aic, row.aiv, 0x422U)));
    values.push_back(MetricCell(Scale(mainWrite, 128.0 / kBytesPerKb), EventSumReason(row.aic, row.aiv, 0x423U)));
    values.push_back(MetricCell(Scale(row.aic.Value(0x422U), 128.0 / kBytesPerKb), EventReason(row.aic, {0x422U})));
    values.push_back(MetricCell(Scale(row.aic.Value(0x70eU), 128.0 / kBytesPerKb), EventReason(row.aic, {0x70eU})));
    values.push_back(MetricCell(Scale(row.aic.Value(0x423U), 128.0 / kBytesPerKb), EventReason(row.aic, {0x423U})));
    values.push_back(MetricCell(Scale(xgu, 128.0 / kBytesPerKb), EventReason(row.aiv, {0x422U, 0x57fU, 0x580U})));
    AppendMetric(
        values, std::nullopt, row.aiv.present ? MissingReason::DbiUnavailable : MissingReason::CoreTypeNotApplicable);
    const double gmToL1Max = MaxBandwidth(config.socName, "GM_TO_L1");
    const double l0cToL1Max = MaxBandwidth(config.socName, "L0C_TO_L1");
    const double l0cToGmMax = MaxBandwidth(config.socName, "L0C_TO_GM");
    const double gmToUbMax = MaxBandwidth(config.socName, "GM_TO_UB");
    AppendMetric(
        values,
        UsageRate(EventBandwidth(row.aic, 0x422U, 128.0, aicFrequencyMhz, aicBlockCount, aicCoreCount), gmToL1Max),
        UsageRateReason(EventBandwidthReason(row.aic, {0x422U}, aicFrequencyMhz), gmToL1Max));
    AppendMetric(
        values,
        UsageRate(EventBandwidth(row.aic, 0x70eU, 128.0, aicFrequencyMhz, aicBlockCount, aicCoreCount), l0cToL1Max),
        UsageRateReason(EventBandwidthReason(row.aic, {0x70eU}, aicFrequencyMhz), l0cToL1Max));
    AppendMetric(
        values,
        UsageRate(EventBandwidth(row.aic, 0x423U, 128.0, aicFrequencyMhz, aicBlockCount, aicCoreCount), l0cToGmMax),
        UsageRateReason(EventBandwidthReason(row.aic, {0x423U}, aicFrequencyMhz), l0cToGmMax));
    AppendMetric(
        values,
        UsageRate(
            xgu.has_value() && duration.has_value() ? Bandwidth(*xgu * 128.0, *duration) : std::nullopt, gmToUbMax),
        UsageRateReason(xguReason, gmToUbMax));
    AppendMetric(
        values, std::nullopt, row.aiv.present ? MissingReason::DbiUnavailable : MissingReason::CoreTypeNotApplicable);
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

CsvRow MemoryL0Row(const RowMetrics& row, const PmuCsvConfig& config)
{
    CsvRow values;
    const double aicFrequencyMhz = AicFrequencyMhz(config);
    const uint32_t aicCoreCount = AicCoreCount(config);
    const uint32_t aicBlockCount = AicBlockCount(config);
    AppendCommon(values, row, config);
    for (const auto& [event, bytes] : std::initializer_list<std::pair<uint32_t, double>>{
             {0x304U, 64.0}, {0x703U, 256.0}, {0x306U, 256.0}, {0x705U, 256.0}, {0x30aU, 1024.0}, {0x308U, 1024.0}}) {
        AppendMetric(
            values, EventBandwidth(row.aic, event, bytes, aicFrequencyMhz, aicBlockCount, aicCoreCount),
            EventBandwidthReason(row.aic, {event}, aicFrequencyMhz));
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

CsvRow MemoryUBRow(const RowMetrics& row, const PmuCsvConfig& config)
{
    CsvRow values;
    const double aivFrequencyMhz = AivFrequencyMhz(config);
    const uint32_t aivCoreCount = AivCoreCount(config);
    const uint32_t aivBlockCount = AivBlockCount(config);
    AppendCommon(values, row, config);
    AppendMetric(
        values, EventBandwidth(row.aiv, 0x571U, 256.0, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x571U}, aivFrequencyMhz));
    AppendMetric(
        values, EventBandwidth(row.aiv, 0x572U, 256.0, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x572U}, aivFrequencyMhz));
    const auto xgu = NonNegativeDifference(row.aiv, 0x422U, {0x57fU, 0x580U});
    const auto duration = Time(row.aiv, aivFrequencyMhz, aivBlockCount, aivCoreCount);
    AppendMetric(
        values, xgu.has_value() && duration.has_value() ? Bandwidth(*xgu * 128.0, *duration) : std::nullopt,
        EventBandwidthReason(row.aiv, {0x422U, 0x57fU, 0x580U}, aivFrequencyMhz));
    // UB_TO_GM_DATA is supplied by DBI, which is not part of aclptiProfilingDataResult yet.
    AppendMetric(
        values, std::nullopt, row.aiv.present ? MissingReason::DbiUnavailable : MissingReason::CoreTypeNotApplicable);
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

CsvRow PipeRow(const RowMetrics& row, const PmuCsvConfig& config)
{
    CsvRow values;
    const double aicFrequencyMhz = AicFrequencyMhz(config);
    const double aivFrequencyMhz = AivFrequencyMhz(config);
    const uint32_t aicCoreCount = AicCoreCount(config);
    const uint32_t aivCoreCount = AivCoreCount(config);
    const uint32_t aicBlockCount = AicBlockCount(config);
    const uint32_t aivBlockCount = AivBlockCount(config);
    const uint32_t aicCubeEvent = AicCubeEventId(row.aic);
    AppendCommon(values, row, config);
    AppendMetric(
        values, EventTime(row.aiv, 0x501U, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x501U}, aivFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x501U), RatioReason(row.aiv, {0x501U}));
    AppendMetric(
        values, EventTime(row.aic, aicCubeEvent, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {aicCubeEvent}, aicFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, aicCubeEvent), RatioReason(row.aic, {aicCubeEvent}));
    AppendMetric(
        values, EventTime(row.aic, 0x1U, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x1U}, aicFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x1U), RatioReason(row.aic, {0x1U}));
    AppendMetric(
        values, EventTime(row.aiv, 0x1U, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x1U}, aivFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x1U), RatioReason(row.aiv, {0x1U}));
    AppendMetric(
        values, EventTime(row.aic, 0x714U, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x714U}, aicFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x714U), RatioReason(row.aic, {0x714U}));
    AppendMetric(
        values, EventTime(row.aic, 0x702U, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x702U}, aicFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x702U), RatioReason(row.aic, {0x702U}));
    AppendMetric(
        values, EventTime(row.aic, 0x202U, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x202U}, aicFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x202U), RatioReason(row.aic, {0x202U}));
    AppendMetric(
        values, EventTime(row.aiv, 0x202U, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x202U}, aivFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x202U), RatioReason(row.aiv, {0x202U}));
    AppendMetric(
        values, EventTime(row.aic, 0x203U, aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x203U}, aicFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aic, 0x203U), RatioReason(row.aic, {0x203U}));
    AppendMetric(
        values, EventTime(row.aiv, 0x203U, aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x203U}, aivFrequencyMhz));
    AppendMetric(values, ValueRatio(row.aiv, 0x203U), RatioReason(row.aiv, {0x203U}));
    AppendMetric(values, IcacheMissRate(row.aic), EventReason(row.aic, {0x35U, 0x34U}));
    AppendMetric(values, IcacheMissRate(row.aiv), EventReason(row.aiv, {0x35U, 0x34U}));

    const auto xgu = NonNegativeDifference(row.aiv, 0x422U, {0x57fU, 0x580U});
    const auto xulBase = NonNegativeDifference(row.aic, 0x709U, {0x70eU});
    const auto aicMainRead = row.aic.Value(0x422U);
    const auto xul = xulBase.has_value() && aicMainRead.has_value() ?
                         Metric(std::max(0.0, *xulBase - std::floor(*aicMainRead / 2.0))) :
                         std::nullopt;
    const auto xfp = SumEvents(row.aic, {0x70cU, 0x70eU, 0x717U, 0x423U});
    AppendMetric(
        values,
        ActiveBandwidthMetric(Scale(xul, 256.0), row.aic.Value(0x203U), aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x709U, 0x70eU, 0x422U, 0x203U}, aicFrequencyMhz));
    AppendMetric(
        values, std::nullopt, row.aiv.present ? MissingReason::DbiUnavailable : MissingReason::CoreTypeNotApplicable);
    AppendMetric(
        values,
        ActiveBandwidthMetric(Scale(xfp, 128.0), row.aic.Value(0x714U), aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x70cU, 0x70eU, 0x717U, 0x423U, 0x714U}, aicFrequencyMhz));
    AppendMetric(
        values,
        ActiveBandwidthMetric(Scale(xgu, 128.0), row.aiv.Value(0x202U), aivFrequencyMhz, aivBlockCount, aivCoreCount),
        EventBandwidthReason(row.aiv, {0x422U, 0x57fU, 0x580U, 0x202U}, aivFrequencyMhz));
    AppendMetric(
        values,
        ActiveBandwidthMetric(
            Scale(row.aic.Value(0x707U), 256.0), row.aic.Value(0x702U), aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x707U, 0x702U}, aicFrequencyMhz));
    AppendMetric(
        values,
        ActiveBandwidthMetric(
            Scale(row.aic.Value(0x422U), 128.0), row.aic.Value(0x202U), aicFrequencyMhz, aicBlockCount, aicCoreCount),
        EventBandwidthReason(row.aic, {0x422U, 0x202U}, aicFrequencyMhz));
    return values;
}

struct SectionWriter {
    std::vector<std::string> (*header)();
    std::function<CsvRow(const RowMetrics&, const PmuCsvConfig&)> row;
};

const std::map<std::string_view, SectionWriter>& SectionWriters()
{
    static const std::map<std::string_view, SectionWriter> writers = {
        {"L2Cache", {&L2Header, [](const RowMetrics& row, const PmuCsvConfig& config) { return L2Row(row, config); }}},
        {"Memory",
         {&MemoryHeader, [](const RowMetrics& row, const PmuCsvConfig& config) { return MemoryRow(row, config); }}},
        {"MemoryL0",
         {&MemoryL0Header, [](const RowMetrics& row, const PmuCsvConfig& config) { return MemoryL0Row(row, config); }}},
        {"MemoryUB",
         {&MemoryUBHeader, [](const RowMetrics& row, const PmuCsvConfig& config) { return MemoryUBRow(row, config); }}},
        {"PipeUtilization",
         {&PipeHeader, [](const RowMetrics& row, const PmuCsvConfig& config) { return PipeRow(row, config); }}},
    };
    return writers;
}

const SectionWriter* FindWriter(std::string_view section)
{
    const auto& writers = SectionWriters();
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

void WriteCsvLine(std::ostream& output, const CsvRow& values)
{
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << values[index].text;
    }
    output << '\n';
}

void UpdateMissingStats(const std::vector<std::string>& header, const CsvRow& values, CsvSectionStats* stats)
{
    if (stats == nullptr) {
        return;
    }
    ++stats->rows;
    stats->totalFields += header.size();
    bool rowMissing = false;
    if (values.size() != header.size()) {
        ++stats->mismatchedRows;
    }
    const std::size_t commonSize = std::min(header.size(), values.size());
    for (std::size_t index = 0; index < commonSize; ++index) {
        if (values[index].text != "NA") {
            continue;
        }
        ++stats->missingFields;
        ++stats->missingColumns[header[index]];
        const char* reasonName = MissingReasonName(values[index].missingReason);
        ++stats->missingReasons[reasonName];
        ++stats->missingColumnReasons[header[index] + "@" + reasonName];
        rowMissing = true;
    }
    for (std::size_t index = values.size(); index < header.size(); ++index) {
        ++stats->missingFields;
        ++stats->missingColumns[header[index]];
        const char* reasonName = MissingReasonName(MissingReason::RowSizeMismatch);
        ++stats->missingReasons[reasonName];
        ++stats->missingColumnReasons[header[index] + "@" + reasonName];
        rowMissing = true;
    }
    if (rowMissing) {
        ++stats->missingRows;
    }
}

std::string FormatMissingColumns(const std::map<std::string, std::size_t>& missingColumns)
{
    if (missingColumns.empty()) {
        return "none";
    }
    std::ostringstream output;
    bool first = true;
    for (const auto& [column, count] : missingColumns) {
        if (!first) {
            output << ';';
        }
        output << column << ':' << count;
        first = false;
    }
    return output.str();
}

std::string FormatMissingReasons(const std::map<std::string, std::size_t>& missingReasons)
{
    if (missingReasons.empty()) {
        return "none";
    }
    std::ostringstream output;
    bool first = true;
    for (const auto& [reason, count] : missingReasons) {
        if (!first) {
            output << ';';
        }
        output << reason << ':' << count;
        first = false;
    }
    return output.str();
}

std::string ResolveOutputDirectory(const PmuCsvConfig& config) { return config.outputDirectory; }

bool IsEmptySuccessfulResult(const aclptiProfilingDataResult& result)
{
    return result.status == ACLPTI_SUCCESS && result.taskLogs.empty() && result.blockLogs.empty() &&
           result.pmuLogs.empty() && result.errorStats.failedRecordCount == 0;
}

bool HasRootSectionCsv(const boost::filesystem::path& outputDirectory)
{
    for (const auto& [section, writer] : SectionWriters()) {
        static_cast<void>(writer);
        if (boost::filesystem::exists(outputDirectory / (std::string(section) + ".csv"))) {
            return true;
        }
    }
    return false;
}

std::string CollectionDirectoryName(uint64_t sequence)
{
    std::ostringstream output;
    output << "collection-p" << static_cast<long long>(::getpid()) << '-' << std::setw(4) << std::setfill('0')
           << sequence;
    return output.str();
}

boost::filesystem::path CreateUniqueCollectionDirectory(const boost::filesystem::path& outputDirectory)
{
    static std::atomic<uint64_t> sequence{0};
    for (std::size_t attempt = 0; attempt < 1024; ++attempt) {
        const boost::filesystem::path collectionDirectory = outputDirectory / CollectionDirectoryName(++sequence);
        if (boost::filesystem::create_directory(collectionDirectory)) {
            return collectionDirectory;
        }
    }
    throw boost::filesystem::filesystem_error(
        "create unique CSV collection directory failed", outputDirectory,
        boost::system::errc::make_error_code(boost::system::errc::file_exists));
}

boost::filesystem::path ResolveCsvWriteDirectory(const boost::filesystem::path& outputDirectory)
{
    if (!HasRootSectionCsv(outputDirectory)) {
        return outputDirectory;
    }
    return CreateUniqueCollectionDirectory(outputDirectory);
}

void MirrorCsvFiles(
    const boost::filesystem::path& rootDirectory, const boost::filesystem::path& writeDirectory,
    const std::vector<std::string>& sections, const std::string& mirrorOutputDirectory)
{
    if (mirrorOutputDirectory.empty()) {
        return;
    }

    const boost::filesystem::path mirrorRoot(mirrorOutputDirectory);
    if (!mirrorRoot.is_absolute()) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV mirror skipped: output path must be absolute path=%s", mirrorRoot.c_str());
        return;
    }

    const boost::filesystem::path relativeWriteDirectory = writeDirectory.lexically_relative(rootDirectory);
    const boost::filesystem::path mirrorWriteDirectory =
        relativeWriteDirectory.empty() || relativeWriteDirectory == "." ? mirrorRoot :
                                                                          mirrorRoot / relativeWriteDirectory;

    boost::system::error_code filesystemError;
    boost::filesystem::create_directories(mirrorWriteDirectory, filesystemError);
    if (filesystemError) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV mirror failed: create output directory failed path=%s code=%d reason=%s",
            mirrorWriteDirectory.c_str(), filesystemError.value(), filesystemError.message().c_str());
        return;
    }

    for (const auto& section : sections) {
        const boost::filesystem::path source = writeDirectory / (section + ".csv");
        const boost::filesystem::path destination = mirrorWriteDirectory / (section + ".csv");
        if (source.lexically_normal() == destination.lexically_normal()) {
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV mirror skipped: source and destination match path=%s", source.c_str());
            continue;
        }
        boost::filesystem::copy_file(
            source, destination, boost::filesystem::copy_options::overwrite_existing, filesystemError);
        if (filesystemError) {
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV mirror failed: copy source=%s destination=%s code=%d reason=%s", source.c_str(),
                destination.c_str(), filesystemError.value(), filesystemError.message().c_str());
            filesystemError.clear();
            continue;
        }
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV mirror complete: source=%s destination=%s", source.c_str(), destination.c_str());
    }
}

aclptiResult EnsureCsvOutputDirectory(const boost::filesystem::path& outputDirectory)
{
    if (outputDirectory.empty()) {
        npu_compute::detail::DebugLog("npu-compute", "CSV write rejected: output path is empty");
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    if (!outputDirectory.is_absolute()) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV write rejected: output path must be absolute path=%s", outputDirectory.c_str());
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }

    boost::system::error_code filesystemError;
    const boost::filesystem::file_status outputStatus =
        boost::filesystem::symlink_status(outputDirectory, filesystemError);
    if (filesystemError == boost::system::errc::no_such_file_or_directory) {
        filesystemError.clear();
    }
    if (filesystemError) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV write rejected: inspect output path failed path=%s code=%d reason=%s",
            outputDirectory.c_str(), filesystemError.value(), filesystemError.message().c_str());
        return ACLPTI_ERROR_CSV_WRITE;
    }
    const bool exists = boost::filesystem::exists(outputStatus);
    if (exists && !boost::filesystem::is_directory(outputDirectory, filesystemError)) {
        if (filesystemError) {
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV write rejected: inspect output path failed path=%s code=%d reason=%s",
                outputDirectory.c_str(), filesystemError.value(), filesystemError.message().c_str());
        } else {
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV write rejected: output path is not a directory path=%s reason=not a directory",
                outputDirectory.c_str());
        }
        return ACLPTI_ERROR_CSV_WRITE;
    }
    if (!exists) {
        boost::filesystem::create_directories(outputDirectory, filesystemError);
        if (filesystemError) {
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV write rejected: create output directory failed path=%s code=%d reason=%s",
                outputDirectory.c_str(), filesystemError.value(), filesystemError.message().c_str());
            return ACLPTI_ERROR_CSV_WRITE;
        }
    }
    if (::access(outputDirectory.c_str(), W_OK | X_OK) != 0) {
        const int errorNumber = errno;
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV write rejected: output path is not writable path=%s errno=%d reason=%s",
            outputDirectory.c_str(), errorNumber, std::strerror(errorNumber));
        return ACLPTI_ERROR_CSV_WRITE;
    }
    return ACLPTI_SUCCESS;
}

} // namespace

aclptiResult PmuCsvWriter::Write(
    const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const PmuCsvConfig& config)
{
    const std::string outputDirectory = ResolveOutputDirectory(config);
    npu_compute::detail::DebugLog(
        "npu-compute", "CSV write requested: output=%s sections=%zu pmuBlocks=%zu status=%d failedRecords=%llu",
        outputDirectory.c_str(), sections.size(), result.pmuLogs.size(), static_cast<int>(result.status),
        static_cast<unsigned long long>(result.errorStats.failedRecordCount));
    if (IsEmptySuccessfulResult(result)) {
        npu_compute::detail::DebugLog("npu-compute", "CSV write skipped: empty successful result");
        return ACLPTI_SUCCESS;
    }
    if (result.pmuLogs.empty()) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV write skipped: no PMU rows status=%d failedRecords=%llu",
            static_cast<int>(result.status), static_cast<unsigned long long>(result.errorStats.failedRecordCount));
        return ACLPTI_SUCCESS;
    }
    PmuCsvConfig csvConfig = config;
    const BlockCounts blockCounts = ComputeBlockCounts(result.pmuLogs);
    csvConfig.aicBlockCount = blockCounts.aic;
    csvConfig.aivBlockCount = blockCounts.aiv;
    npu_compute::detail::DebugLog(
        "npu-compute", "CSV timing config: aicBlockCount=%u aicCoreCount=%u aivBlockCount=%u aivCoreCount=%u",
        AicBlockCount(csvConfig), AicCoreCount(csvConfig), AivBlockCount(csvConfig), AivCoreCount(csvConfig));
    const double aicFrequencyMhz = AicFrequencyMhz(csvConfig);
    const double aivFrequencyMhz = AivFrequencyMhz(csvConfig);
    if (aicFrequencyMhz <= 0.0 || !std::isfinite(aicFrequencyMhz) || aivFrequencyMhz <= 0.0 ||
        !std::isfinite(aivFrequencyMhz)) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV write rejected: invalid config frequency=%f aicFrequency=%f aivFrequency=%f",
            config.frequencyMhz, config.aicFrequencyMhz, config.aivFrequencyMhz);
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    try {
        const boost::filesystem::path rootDirectory(outputDirectory);
        const aclptiResult outputDirectoryStatus = EnsureCsvOutputDirectory(rootDirectory);
        if (outputDirectoryStatus != ACLPTI_SUCCESS) {
            return outputDirectoryStatus;
        }
        for (const auto& section : sections) {
            if (FindWriter(section) == nullptr) {
                npu_compute::detail::DebugLog(
                    "npu-compute", "CSV write rejected: unsupported section=%s", section.c_str());
                return ACLPTI_ERROR_NOT_SUPPORTED;
            }
        }
        const boost::filesystem::path writeDirectory = ResolveCsvWriteDirectory(rootDirectory);
        if (writeDirectory != rootDirectory) {
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV write routed to collection directory: path=%s", writeDirectory.c_str());
        }
        for (const auto& section : sections) {
            const SectionWriter* writer = FindWriter(section);
            const boost::filesystem::path path = writeDirectory / (section + ".csv");
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV section write start: section=%s path=%s rows=%zu", section.c_str(), path.c_str(),
                result.pmuLogs.size());
            std::ofstream output(path, std::ios::out | std::ios::trunc);
            if (!output.is_open()) {
                const int errorNumber = errno;
                npu_compute::detail::DebugLog(
                    "npu-compute", "CSV section write failed: open path=%s errno=%d reason=%s", path.c_str(),
                    errorNumber, std::strerror(errorNumber));
                return ACLPTI_ERROR_CSV_WRITE;
            }
            const std::vector<std::string> header = writer->header();
            CsvSectionStats stats;
            WriteCsvLine(output, header);
            for (const auto& [key, row] : result.pmuLogs) {
                const CsvRow values = writer->row(MakeRowMetrics(key, row), csvConfig);
                UpdateMissingStats(header, values, &stats);
                WriteCsvLine(output, values);
            }
            output.flush();
            if (!output.good()) {
                const int errorNumber = errno;
                npu_compute::detail::DebugLog(
                    "npu-compute", "CSV section write failed: flush path=%s errno=%d reason=%s", path.c_str(),
                    errorNumber, std::strerror(errorNumber));
                return ACLPTI_ERROR_CSV_WRITE;
            }
            const std::string missingColumns = FormatMissingColumns(stats.missingColumns);
            const std::string missingReasons = FormatMissingReasons(stats.missingReasons);
            const std::string missingColumnReasons = FormatMissingReasons(stats.missingColumnReasons);
            npu_compute::detail::DebugLog(
                "npu-compute",
                "CSV section data availability: section=%s rows=%zu fields=%zu missingFields=%zu missingRows=%zu "
                "mismatchedRows=%zu missingColumns=%s missingReasons=%s missingColumnReasons=%s",
                section.c_str(), stats.rows, stats.totalFields, stats.missingFields, stats.missingRows,
                stats.mismatchedRows, missingColumns.c_str(), missingReasons.c_str(), missingColumnReasons.c_str());
            npu_compute::detail::DebugLog(
                "npu-compute", "CSV section write complete: section=%s path=%s", section.c_str(), path.c_str());
        }
        MirrorCsvFiles(rootDirectory, writeDirectory, sections, config.mirrorOutputDirectory);
    } catch (const boost::filesystem::filesystem_error& error) {
        const boost::filesystem::path errorPath =
            error.path1().empty() ? boost::filesystem::path(outputDirectory) : error.path1();
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV write failed: filesystem error path=%s code=%d reason=%s detail=%s", errorPath.c_str(),
            error.code().value(), error.code().message().c_str(), error.what());
        return ACLPTI_ERROR_CSV_WRITE;
    } catch (const std::bad_alloc&) {
        npu_compute::detail::DebugLog("npu-compute", "CSV write failed: out of memory");
        return ACLPTI_ERROR_INTERNAL;
    }
    npu_compute::detail::DebugLog("npu-compute", "CSV write complete");
    return ACLPTI_SUCCESS;
}

} // namespace npu_compute
