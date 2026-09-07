/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rep_decoder.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "rep_encoder.h"

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

using npu_compute::compute_launcher::DecodedRep;
using npu_compute::compute_launcher::DecodeRep;
using npu_compute::compute_launcher::EncodeRep;
using npu_compute::compute_launcher::NpuRepFileType;
using npu_compute::compute_launcher::RepEntry;

std::vector<uint8_t> Bytes(std::string_view value) { return {value.begin(), value.end()}; }

void WriteLe16(uint16_t value, uint8_t* output)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
}

void WriteLe32(uint32_t value, uint8_t* output)
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        output[index] = static_cast<uint8_t>(value >> (index * 8U));
    }
}

void WriteLe64(uint64_t value, uint8_t* output)
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        output[index] = static_cast<uint8_t>(value >> (index * 8U));
    }
}

bool BuildRep(std::vector<uint8_t>* encoded)
{
    std::string error;
    return EncodeRep(
        {{"HardwareInfo.jsonl", NpuRepFileType::Jsonl, Bytes("{\"category\":\"Host Info\"}\n")},
         {"PipeUtilization.csv", NpuRepFileType::Csv, Bytes("name,value\npipe,1\n")}},
        encoded, &error);
}

int TestDecodesValidRep()
{
    std::vector<uint8_t> encoded;
    CHECK(BuildRep(&encoded));
    DecodedRep decoded;
    std::string error = "old error";
    CHECK(DecodeRep(encoded, &decoded, &error));
    CHECK(error.empty());
    CHECK(decoded.entries.size() == 2U);
    CHECK(decoded.entries[0].file_name == "HardwareInfo.jsonl");
    CHECK(decoded.entries[0].file_type == NpuRepFileType::Jsonl);
    CHECK(decoded.entries[0].payload == Bytes("{\"category\":\"Host Info\"}\n"));
    CHECK(decoded.entries[1].file_name == "PipeUtilization.csv");
    CHECK(decoded.entries[1].file_type == NpuRepFileType::Csv);
    CHECK(decoded.entries[1].payload == Bytes("name,value\npipe,1\n"));
    return 0;
}

int TestRejectsInvalidHeader()
{
    std::vector<uint8_t> encoded;
    CHECK(BuildRep(&encoded));
    DecodedRep decoded;
    std::string error;

    std::vector<uint8_t> invalid = encoded;
    invalid[0] = 'x';
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("magic") != std::string::npos);

    invalid = encoded;
    WriteLe32(0x00020000U, invalid.data() + 8U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("version") != std::string::npos);

    invalid = encoded;
    WriteLe16(2U, invalid.data() + 12U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("origin") != std::string::npos);

    invalid = encoded;
    WriteLe16(35U, invalid.data() + 14U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("header length") != std::string::npos);
    return 0;
}

int TestRejectsInvalidFileInfo()
{
    std::vector<uint8_t> encoded;
    CHECK(BuildRep(&encoded));
    DecodedRep decoded;
    std::string error;
    constexpr std::size_t kFirstInfo = 36U;
    constexpr std::size_t kSecondInfo = 36U + 160U;

    std::vector<uint8_t> invalid = encoded;
    const std::string duplicate = "HardwareInfo.jsonl";
    std::fill(invalid.begin() + kSecondInfo + 8U, invalid.begin() + kSecondInfo + 8U + 128U, 0U);
    std::copy(duplicate.begin(), duplicate.end(), invalid.begin() + kSecondInfo + 8U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("duplicate") != std::string::npos);

    invalid = encoded;
    invalid[kFirstInfo + 8U] = '/';
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("separator") != std::string::npos);

    invalid = encoded;
    invalid[kFirstInfo + 8U] = '\\';
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("separator") != std::string::npos);

    invalid = encoded;
    std::fill(invalid.begin() + kFirstInfo + 8U, invalid.begin() + kFirstInfo + 8U + 128U, 0U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("empty") != std::string::npos);

    invalid = encoded;
    std::fill(invalid.begin() + kFirstInfo + 8U, invalid.begin() + kFirstInfo + 8U + 128U, 'a');
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("terminated") != std::string::npos);

    invalid = encoded;
    WriteLe16(99U, invalid.data() + kFirstInfo + 136U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("file type") != std::string::npos);

    for (const std::string& unsafe_name : {std::string("."), std::string("..")}) {
        CHECK(EncodeRep({{unsafe_name, NpuRepFileType::Json, Bytes("{}")}}, &invalid, &error));
        CHECK(!DecodeRep(invalid, &decoded, &error));
        CHECK(error.find("unsafe") != std::string::npos);
    }
    return 0;
}

int TestRejectsInvalidPayloadRanges()
{
    std::vector<uint8_t> encoded;
    CHECK(BuildRep(&encoded));
    DecodedRep decoded;
    std::string error;
    constexpr std::size_t kFirstInfo = 36U;
    constexpr std::size_t kSecondInfo = 36U + 160U;
    constexpr uint64_t kPayloadStart = 36U + 2U * 160U;

    std::vector<uint8_t> invalid = encoded;
    WriteLe64(kPayloadStart - 1U, invalid.data() + kFirstInfo + 152U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("payload") != std::string::npos);

    invalid = encoded;
    WriteLe64(kPayloadStart + 1U, invalid.data() + kFirstInfo + 152U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("payload") != std::string::npos);

    invalid = encoded;
    WriteLe64(static_cast<uint64_t>(encoded.size()), invalid.data() + kSecondInfo + 144U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("payload") != std::string::npos);

    invalid = encoded;
    invalid.push_back(0U);
    WriteLe64(invalid.size(), invalid.data() + 28U);
    CHECK(!DecodeRep(invalid, &decoded, &error));
    CHECK(error.find("unreferenced") != std::string::npos);
    return 0;
}

} // namespace

int main()
{
    if (TestDecodesValidRep() != 0 || TestRejectsInvalidHeader() != 0 || TestRejectsInvalidFileInfo() != 0 ||
        TestRejectsInvalidPayloadRanges() != 0) {
        return 1;
    }
    return 0;
}
