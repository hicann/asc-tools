#pragma once

#include "aclpti/aclpti_data.h"
#include "compute_types.h"

#include <vector>

namespace npucompute {

aclptiResult WritePmuCsv(
    const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const ReportConfig& config);

aclptiResult WriteSummaryJsonl(
    const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const ReportConfig& config,
    const KernelMetadata& metadata);

aclptiResult WritePmuReport(
    const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const ReportConfig& config,
    const KernelMetadata& metadata = {});

} // namespace npucompute
