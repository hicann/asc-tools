/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "import_output_directory.h"
#include "imported_profile_results.h"
#include "launcher.h"

int main(int argc, char** argv)
{
    npu_compute::compute_launcher::CliConfig config;
    std::vector<std::string> parse_errors;
    const bool parsed = npu_compute::compute_launcher::ParseCli(argc, argv, &config, &parse_errors);
    for (const std::string& error : parse_errors) {
        std::fprintf(stderr, "npu-compute: %s\n", error.c_str());
    }
    if (!parse_errors.empty()) {
        std::fflush(stderr);
    }
    if (config.show_help) {
        npu_compute::compute_launcher::PrintUsage(stdout, argv[0]);
    }
    if (!parsed) {
        return npu_compute::compute_launcher::kUsageErrorExitCode;
    }

    if (config.show_help) {
        return 0;
    }

    if (config.list_sections) {
        npu_compute::compute_launcher::PrintSections(stdout);
        return 0;
    }

    if (config.import_path.has_value()) {
        std::string error;
        std::vector<npu_compute::compute_launcher::ImportedProfileEntry> results;
        if (!npu_compute::compute_launcher::ReadImportedProfileResults(*config.import_path, &results, &error)) {
            std::fprintf(stderr, "npu-compute: %s\n", error.c_str());
            return npu_compute::compute_launcher::kReportErrorExitCode;
        }
        npu_compute::compute_launcher::ImportOutputDirectory outputDirectory;
        if (!npu_compute::compute_launcher::ImportOutputDirectory::Create(
                *config.import_path, config.export_path, &outputDirectory, &error) ||
            !npu_compute::compute_launcher::UnpackImportedProfileResults(
                results, outputDirectory.TemporaryPath(), &error) ||
            !outputDirectory.Publish(&error)) {
            std::fprintf(stderr, "npu-compute: %s\n", error.c_str());
            return npu_compute::compute_launcher::kReportErrorExitCode;
        }
        std::fprintf(stderr, "npu-compute: unpacked=%s\n", outputDirectory.FinalPath().c_str());
        return 0;
    }

    std::string collection_data_directory;
    std::string report_path;
    std::string error;
    int result = npu_compute::compute_launcher::LaunchTarget(config, &collection_data_directory, &report_path, &error);
    if (!collection_data_directory.empty()) {
        std::fprintf(stderr, "npu-compute: data-directory=%s\n", collection_data_directory.c_str());
    }
    if (result != 0) {
        if (!error.empty()) {
            std::fprintf(stderr, "npu-compute: %s\n", error.c_str());
        }
        return result;
    }
    if (!report_path.empty()) {
        std::fprintf(stderr, "npu-compute: report=%s\n", report_path.c_str());
    }
    return 0;
}
