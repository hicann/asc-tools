/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <string_view>

namespace {

constexpr std::string_view kHardwareInfo = "{\"category\":\"Host Info\",\"cpu physical count\":1}\n"
                                           "{\"category\":\"Device Info\",\"npu count\":1}\n"
                                           "{\"category\":\"CPU Information\",\"control cpu count\":1}\n"
                                           "{\"category\":\"AI Core Information\",\"ai core count\":1}\n"
                                           "{\"category\":\"Memory Information\",\"hbm total(MB)\":1}\n";
constexpr std::string_view kPipeCsv = "block_id,pipe_utilization\n0,75\n";
constexpr std::string_view kMemoryCsv = "block_id,read_bytes\n0,128\n";
constexpr std::string_view kL2CacheCsv = "block_id,hit_rate\n0,99\n";

bool ParseExitCode(const char* value, int* exit_code)
{
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == nullptr || end[0] != '\0' || parsed < 0 || parsed > 255) {
        return false;
    }
    *exit_code = static_cast<int>(parsed);
    return true;
}

bool WriteFile(const boost::filesystem::path& path, std::string_view content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        std::fprintf(stderr, "[rep-fixture] open failed: %s\n", path.c_str());
        return false;
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output.good()) {
        std::fprintf(stderr, "[rep-fixture] write failed: %s\n", path.c_str());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    int exit_code = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--exit-code") != 0 || index + 1 >= argc ||
            !ParseExitCode(argv[++index], &exit_code)) {
            std::fprintf(stderr, "[rep-fixture] invalid arguments\n");
            return 2;
        }
    }

    const char* output_value = std::getenv("NPU_COMPUTE_OUTPUT");
    if (output_value == nullptr || output_value[0] == '\0') {
        std::fprintf(stderr, "[rep-fixture] NPU_COMPUTE_OUTPUT is missing\n");
        return 2;
    }
    const boost::filesystem::path output(output_value);
    const boost::filesystem::path device = output / "device_0";
    const boost::filesystem::path details = device / "details";
    boost::system::error_code directory_error;
    boost::filesystem::create_directories(details, directory_error);
    if (directory_error) {
        std::fprintf(stderr, "[rep-fixture] create directories failed: %s\n", directory_error.message().c_str());
        return 2;
    }

    if (!WriteFile(output / "HardwareInfo.jsonl", kHardwareInfo) ||
        !WriteFile(output / "PipeUtilization.csv", kPipeCsv) || !WriteFile(device / "Memory.csv", kMemoryCsv) ||
        !WriteFile(details / "L2Cache.csv", kL2CacheCsv) || !WriteFile(output / ".hardware_info.lock", "lock")) {
        return 2;
    }
    std::fprintf(stderr, "[rep-fixture] files written\n");
    return exit_code;
}
