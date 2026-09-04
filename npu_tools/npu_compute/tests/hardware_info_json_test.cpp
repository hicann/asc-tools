/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_json.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

npu_compute::HardwareInfoSnapshot MakeSampleSnapshot()
{
    npu_compute::HardwareInfoSnapshot snapshot;
    snapshot.host.cpuPhysicalCount = 2;
    snapshot.host.cpuLogicalCount = 46;
    snapshot.host.memoryTotalSizeMb = 451071.54;
    snapshot.host.diskTotalSizeGb = 2746.56;
    snapshot.device.npuCount = 1;
    snapshot.device.chipInfo = "Ascend 950PR_9599 V100";
    snapshot.device.archInfo = "3510";
    snapshot.cpu.controlCpuCount = 1;
    snapshot.cpu.aiCpuCount = 6;
    snapshot.cpu.aiCpuFrequencyMhz = 1500;
    snapshot.aiCore.aiCoreCount = 36;
    snapshot.aiCore.aiCubeCount = 36;
    snapshot.aiCore.aiVectorCount = 72;
    snapshot.aiCore.aiCubeFrequencyMhz = 1800;
    snapshot.aiCore.aiVectorFrequencyMhz = 1800;
    snapshot.memory.hbmTotalMb = 131072;
    snapshot.memory.hbmUsedMb = 5190.55;
    snapshot.memory.hbmFrequencyMhz = 3200;
    return snapshot;
}

} // namespace

int main()
{
    const std::string expected =
        "{\"category\":\"Host Info\",\"cpu physical count\":2,\"cpu logical count\":46,"
        "\"memory total size(MB)\":451071.54,\"disk total size(GB)\":2746.56}\n"
        "{\"category\":\"Device Info\",\"npu count\":1,\"chip info\":\"Ascend 950PR_9599 V100\","
        "\"arch info\":\"3510\"}\n"
        "{\"category\":\"CPU Information\",\"control cpu count\":1,\"ai cpu count\":6,"
        "\"ai cpu frequency(MHZ)\":1500}\n"
        "{\"category\":\"AI Core Information\",\"ai core count\":36,\"ai cube count\":36,"
        "\"ai vector count\":72,\"ai cube frequency(MHZ)\":1800,\"ai vector frequency(MHZ)\":1800}\n"
        "{\"category\":\"Memory Information\",\"hbm total(MB)\":131072,"
        "\"hbm used(MB)\":5190.55,\"hbm frequency(MHZ)\":3200}\n";

    std::string jsonl;
    std::string error = "old error";
    CHECK(npu_compute::SerializeHardwareInfoJsonl(MakeSampleSnapshot(), &jsonl, &error));
    CHECK(error.empty());
    CHECK(jsonl == expected);

    npu_compute::HardwareInfoFrequencies frequencies;
    CHECK(npu_compute::ParseHardwareInfoFrequenciesJsonl(jsonl, &frequencies, &error));
    CHECK(error.empty());
    CHECK(frequencies.aiCubeCount == 36);
    CHECK(frequencies.aiVectorCount == 72);
    CHECK(frequencies.aiCubeFrequencyMhz == 1800);
    CHECK(frequencies.aiVectorFrequencyMhz == 1800);

    std::string socName;
    CHECK(npu_compute::ParseHardwareInfoSocNameJsonl(jsonl, &socName, &error));
    CHECK(error.empty());
    CHECK(socName == "Ascend 950PR_9599 V100");
    CHECK(!npu_compute::ParseHardwareInfoSocNameJsonl(
        "{\"category\":\"Device Info\",\"chip info\":\"\"}\n", &socName, &error));
    CHECK(!error.empty());

    CHECK(!npu_compute::ParseHardwareInfoFrequenciesJsonl(
        "{\"category\":\"AI Core Information\",\"ai cube frequency(MHZ)\":0,"
        "\"ai vector frequency(MHZ)\":1800}\n",
        &frequencies, &error));
    CHECK(!error.empty());

    npu_compute::HardwareInfoSnapshot escaped;
    escaped.device.chipInfo = "Ascend \"X\"\\line\nnext\t";
    escaped.device.archInfo = std::string("35\x01", 3);
    CHECK(npu_compute::SerializeHardwareInfoJsonl(escaped, &jsonl, &error));
    CHECK(jsonl.find("\"chip info\":\"Ascend \\\"X\\\"\\\\line\\nnext\\t\"") != std::string::npos);
    CHECK(jsonl.find("\"arch info\":\"35\\u0001\"") != std::string::npos);

    npu_compute::HardwareInfoSnapshot zero;
    CHECK(npu_compute::SerializeHardwareInfoJsonl(zero, &jsonl, &error));
    CHECK(
        jsonl == "{\"category\":\"Host Info\",\"cpu physical count\":0,\"cpu logical count\":0,"
                 "\"memory total size(MB)\":0,\"disk total size(GB)\":0}\n"
                 "{\"category\":\"Device Info\",\"npu count\":0,\"chip info\":\"\",\"arch info\":\"\"}\n"
                 "{\"category\":\"CPU Information\",\"control cpu count\":0,\"ai cpu count\":0,"
                 "\"ai cpu frequency(MHZ)\":0}\n"
                 "{\"category\":\"AI Core Information\",\"ai core count\":0,\"ai cube count\":0,"
                 "\"ai vector count\":0,\"ai cube frequency(MHZ)\":0,\"ai vector frequency(MHZ)\":0}\n"
                 "{\"category\":\"Memory Information\",\"hbm total(MB)\":0,\"hbm used(MB)\":0,"
                 "\"hbm frequency(MHZ)\":0}\n");

    npu_compute::HardwareInfoSnapshot invalid;
    invalid.host.memoryTotalSizeMb = -1;
    CHECK(!npu_compute::SerializeHardwareInfoJsonl(invalid, &jsonl, &error));
    CHECK(jsonl.empty());
    CHECK(!error.empty());

    invalid.host.memoryTotalSizeMb = std::nan("");
    CHECK(!npu_compute::SerializeHardwareInfoJsonl(invalid, &jsonl, &error));
    CHECK(jsonl.empty());
    CHECK(!error.empty());

    CHECK(!npu_compute::SerializeHardwareInfoJsonl(zero, nullptr, &error));
    CHECK(!error.empty());
    return 0;
}
