/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "biu/biu_pipeline.h"

#include <boost/filesystem.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define CHECK(condition)                                                                         \
    do {                                                                                         \
        if (!(condition)) {                                                                      \
            std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                            \
        }                                                                                        \
    } while (false)

namespace {

uint32_t Stamp(uint16_t value, uint8_t blockByte = 0)
{
    return 0xe0000000U | (static_cast<uint32_t>(blockByte) << 16U) | value;
}

std::vector<uint32_t> TraceWords(uint64_t timestamp, uint16_t blockId, uint16_t busyCycles)
{
    return {
        Stamp(static_cast<uint16_t>(timestamp), static_cast<uint8_t>(blockId)),
        Stamp(static_cast<uint16_t>(timestamp >> 16U), static_cast<uint8_t>(blockId >> 8U)),
        Stamp(static_cast<uint16_t>(timestamp >> 32U)),
        Stamp(static_cast<uint16_t>(timestamp >> 48U)),
        0xf0000000U,
        0xf001000aU,
        0x00030005U,
        0xf0000000U | busyCycles,
        0xefffffffU,
        0xddddddddU,
    };
}

int TestParseIntervals()
{
    aclptiPipelineData replay;
    replay.deviceId = 0;
    replay.format = aclptiBiuFormat::Chip6;
    replay.channels[{0, aclptiBiuCoreKind::Aic}] = TraceWords(1000, 4, 15);
    replay.channels[{2, aclptiBiuCoreKind::Aiv0}] = TraceWords(1100, 9, 4);

    npucompute::BiuClockConfig clocks{100000000.0, 1000000000.0, 2000000000.0, "unit-test"};
    npucompute::BiuParseStats stats;
    std::vector<npucompute::BiuInterval> intervals;
    const aclptiResult status = npucompute::ParsePipelineReplay(
        7, replay, clocks,
        [&intervals](const npucompute::BiuInterval& interval) {
            intervals.push_back(interval);
            return ACLPTI_SUCCESS;
        },
        &stats);

    CHECK(status == ACLPTI_SUCCESS);
    CHECK(intervals.size() == 2);
    CHECK(intervals[0].channel.groupId == 0);
    CHECK(intervals[0].channel.coreKind == aclptiBiuCoreKind::Aic);
    CHECK(intervals[0].pipe == npucompute::BiuPipe::Scalar);
    CHECK(intervals[0].blockId == 4);
    CHECK(std::abs(intervals[0].startUs - 0.01) < 1e-12);
    CHECK(std::abs(intervals[0].durationUs - 0.02) < 1e-12);
    CHECK(intervals[1].channel.groupId == 2);
    CHECK(std::abs(intervals[1].startUs - 1.005) < 1e-12);
    CHECK(std::abs(intervals[1].durationUs - 0.0045) < 1e-12);
    CHECK(stats.payloadBytes == 80);
    CHECK(stats.paddingBytes == 8);
    CHECK(stats.intervalCount == 2);
    return 0;
}

int TestIncompleteIntervalsAreSkipped()
{
    aclptiPipelineData replay;
    replay.deviceId = 0;
    replay.format = aclptiBiuFormat::Chip6;
    // vector_add: unknown initial SCALAR start, complete MTE2, unclosed SCALAR tail.
    for (const auto core : {aclptiBiuCoreKind::Aiv0, aclptiBiuCoreKind::Aiv1}) {
        replay.channels[{0, core}] = {Stamp(1000), Stamp(0),    Stamp(0),    Stamp(0),
                                      0xf0010014U, 0xf01001b0U, 0xf00004daU, 0xf0010004U};
    }
    npucompute::BiuClockConfig clocks{1e9, 1e9, 1e9, "unit-test"};
    npucompute::BiuParseStats stats;
    const auto parse = [&]() {
        return npucompute::ParsePipelineReplay(1, replay, clocks, [](const auto&) { return ACLPTI_SUCCESS; }, &stats);
    };
    CHECK(parse() == ACLPTI_SUCCESS);
    CHECK(stats.intervalCount == 2);
    CHECK(stats.incompleteIntervalCount == 4);
    // A new anchor also ends an unclosed interval without losing later data.
    auto& words = replay.channels.begin()->second;
    const auto next = words;
    words.insert(words.end(), next.begin(), next.end());
    CHECK(parse() == ACLPTI_SUCCESS);
    CHECK(stats.intervalCount == 3);
    CHECK(stats.incompleteIntervalCount == 6);
    // A valid stream with only incomplete intervals still permits an empty trace.
    replay.channels.clear();
    replay.channels[{0, aclptiBiuCoreKind::Aiv0}] = {Stamp(1000), Stamp(0), Stamp(0), Stamp(0), 0xf0010014U};
    CHECK(parse() == ACLPTI_SUCCESS);
    CHECK(stats.intervalCount == 0);
    CHECK(stats.incompleteIntervalCount == 1);
    return 0;
}

int TestDecodeFailures()
{
    aclptiPipelineData replay;
    replay.deviceId = 0;
    replay.format = aclptiBiuFormat::Chip6;
    replay.channels[{0, aclptiBiuCoreKind::Aic}] = {Stamp(1), Stamp(2)};
    npucompute::BiuClockConfig clocks{100000000.0, 1000000000.0, 1000000000.0, "unit-test"};
    npucompute::BiuParseStats stats;
    CHECK(
        npucompute::ParsePipelineReplay(
            1, replay, clocks, [](const auto&) { return ACLPTI_SUCCESS; }, &stats) == ACLPTI_ERROR_TRACE_INCOMPLETE);

    replay.channels[{0, aclptiBiuCoreKind::Aic}] = {0x70000000U};
    CHECK(
        npucompute::ParsePipelineReplay(
            1, replay, clocks, [](const auto&) { return ACLPTI_SUCCESS; }, &stats) == ACLPTI_ERROR_DECODE);
    return 0;
}

int TestOnlyRequiresUsedCoreClock()
{
    aclptiPipelineData replay;
    replay.deviceId = 0;
    replay.format = aclptiBiuFormat::Chip6;
    replay.channels[{0, aclptiBiuCoreKind::Aic}] = TraceWords(1000, 4, 15);

    npucompute::BiuClockConfig clocks{100000000.0, 1000000000.0, 0.0, "unit-test"};
    npucompute::BiuParseStats stats;
    CHECK(
        npucompute::ParsePipelineReplay(
            1, replay, clocks, [](const auto&) { return ACLPTI_SUCCESS; }, &stats) == ACLPTI_SUCCESS);

    replay.channels.clear();
    replay.channels[{0, aclptiBiuCoreKind::Aiv0}] = TraceWords(1000, 4, 15);
    CHECK(
        npucompute::ParsePipelineReplay(
            1, replay, clocks, [](const auto&) { return ACLPTI_SUCCESS; }, &stats) == ACLPTI_ERROR_RESULT_UNRELIABLE);
    return 0;
}

int TestPipeTraceWriter()
{
    const boost::filesystem::path directory =
        boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("npu-compute-biu-%%%%-%%%%");
    CHECK(boost::filesystem::create_directory(directory));

    npucompute::PipeTraceWriter writer;
    std::string error;
    const aclptiResult beginStatus = writer.Begin(directory / "PipeTrace.json", &error);
    if (beginStatus != ACLPTI_SUCCESS) {
        std::fprintf(stderr, "PipeTraceWriter::Begin failed: %d %s\n", beginStatus, error.c_str());
    }
    CHECK(beginStatus == ACLPTI_SUCCESS);
    npucompute::BiuInterval interval{{7, 0, 3, aclptiBiuCoreKind::Aiv1}, npucompute::BiuPipe::Vector, 5, 1.25, 0.5};
    CHECK(writer.Append(interval) == ACLPTI_SUCCESS);
    CHECK(writer.Commit() == ACLPTI_SUCCESS);

    std::ifstream input((directory / "PipeTrace.json").string());
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CHECK(json.find("\"displayTimeUnit\":\"ns\"") != std::string::npos);
    CHECK(json.find("\"name\":\"VECTOR\"") != std::string::npos);
    CHECK(json.find("\"pid\":\"group3.veccore1\"") != std::string::npos);
    CHECK(json.find("\"tid\":\"VECTOR\"") != std::string::npos);
    CHECK(json.find("\"ph\":\"X\"") != std::string::npos);
    npucompute::PipeTraceWriter emptyWriter;
    CHECK(emptyWriter.Begin(directory / "empty.json", &error) == ACLPTI_SUCCESS);
    CHECK(emptyWriter.Commit() == ACLPTI_SUCCESS);
    boost::system::error_code ignored;
    boost::filesystem::remove_all(directory, ignored);
    return 0;
}

} // namespace

int main()
{
    if (const int result = TestParseIntervals(); result != 0) {
        return result;
    }
    if (const int result = TestDecodeFailures(); result != 0) {
        return result;
    }
    if (const int result = TestOnlyRequiresUsedCoreClock(); result != 0) {
        return result;
    }
    if (const int result = TestIncompleteIntervalsAreSkipped(); result != 0) {
        return result;
    }
    return TestPipeTraceWriter();
}
