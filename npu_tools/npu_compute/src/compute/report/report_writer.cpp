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
#include "common/debug_log.h"
#include "pmu/pmu_metric_builder.h"

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <unistd.h>

namespace npucompute {

aclptiResult WritePmuReport(
    const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const ReportConfig& config,
    const KernelMetadata& metadata)
{
    try {
        const boost::filesystem::path root(config.outputDirectory);
        if (!root.is_absolute()) {
            return ACLPTI_ERROR_INVALID_PARAMETER;
        }
        for (const auto& section : sections) {
            if (PmuMetricBuilder::Header(section).empty()) {
                return ACLPTI_ERROR_NOT_SUPPORTED;
            }
        }
        boost::filesystem::create_directories(root);
        bool occupied = boost::filesystem::exists(root / "summary.jsonl");
        for (const auto* section : {"L2Cache", "Memory", "MemoryL0", "MemoryUB", "PipeUtilization"}) {
            occupied = occupied || boost::filesystem::exists(root / (std::string(section) + ".csv"));
        }
        auto directory = root;
        if (occupied) {
            std::string candidate = (root / ("collection-p" + std::to_string(::getpid()) + "-XXXXXX")).string();
            if (::mkdtemp(candidate.data()) == nullptr) {
                return ACLPTI_ERROR_INTERNAL;
            }
            directory = candidate;
        }
        ReportConfig outputConfig = config;
        outputConfig.outputDirectory = directory.string();
        outputConfig.fixedOutputDirectory = true;
        if (!config.mirrorOutputDirectory.empty() && directory != root) {
            outputConfig.mirrorOutputDirectory =
                (boost::filesystem::path(config.mirrorOutputDirectory) / directory.filename()).string();
        }
        const auto status = WritePmuCsv(result, sections, outputConfig);
        if (status != ACLPTI_SUCCESS) {
            return status;
        }
        const auto summaryStatus = WriteSummaryJsonl(result, sections, outputConfig, metadata);
        if (summaryStatus == ACLPTI_SUCCESS && !outputConfig.mirrorOutputDirectory.empty()) {
            const boost::filesystem::path mirror(outputConfig.mirrorOutputDirectory);
            if (mirror.is_absolute() && mirror.lexically_normal() != directory.lexically_normal()) {
                boost::system::error_code error;
                boost::filesystem::create_directories(mirror, error);
                if (!error) {
                    boost::filesystem::copy_file(
                        directory / "summary.jsonl", mirror / "summary.jsonl",
                        boost::filesystem::copy_options::overwrite_existing, error);
                }
                if (error) {
                    detail::DebugLog("npu-compute", "summary mirror failed: %s", error.message().c_str());
                }
            }
        }
        return summaryStatus;
    } catch (const std::exception& error) {
        detail::DebugLog("npu-compute", "report failed: %s", error.what());
        return ACLPTI_ERROR_INTERNAL;
    }
}

} // namespace npucompute
