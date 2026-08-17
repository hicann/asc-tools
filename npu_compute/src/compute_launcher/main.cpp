/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "config.h"
#include "launcher.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    npu_compute::compute_launcher::CliConfig config;
    std::string error;
    if (!npu_compute::compute_launcher::ParseCli(argc, argv, &config, &error)) {
        std::fprintf(stderr, "npu-compute: %s\n", error.c_str());
        return 2;
    }

    if (config.show_help) {
        npu_compute::compute_launcher::PrintUsage(stdout, argv[0]);
        return 0;
    }

    if (config.list_sections) {
        npu_compute::compute_launcher::PrintSections(stdout);
        return 0;
    }

    if (config.import_path.has_value() || config.export_path.has_value()) {
        std::fprintf(
            stderr, "npu-compute: repo import/export is not available in this integration "
                    "phase\n");
        return 4;
    }

    std::string staging_directory;
    int result = npu_compute::compute_launcher::LaunchTarget(config, &staging_directory, &error);
    if (!staging_directory.empty()) {
        std::fprintf(stderr, "npu-compute: staging=%s\n", staging_directory.c_str());
    }
    if (result != 0) {
        if (!error.empty()) {
            std::fprintf(stderr, "npu-compute: %s\n", error.c_str());
        }
        return result;
    }
    return 0;
}
