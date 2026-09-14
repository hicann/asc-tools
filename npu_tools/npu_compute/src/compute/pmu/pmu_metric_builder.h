#pragma once

#include "aclpti/aclpti_data.h"
#include "compute_types.h"
#include <vector>

#include <optional>
#include <string_view>

namespace npucompute {

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

struct PmuMetricField {
    std::string text;
    MissingReason missingReason = MissingReason::None;
    std::optional<double> value;
    bool integer = false;
};

using PmuMetricRow = std::vector<PmuMetricField>;

class PmuMetricBuilder final {
public:
    static std::vector<std::string> Header(std::string_view section);
    static PmuMetricRow Build(
        std::string_view section, const aclptiBlockKey& key, const aclptiPmuDataRow& row, const ReportConfig& config,
        std::optional<double> duration = std::nullopt);
    static std::optional<double> TaskDuration(const aclptiProfilingDataResult& result);
    static std::optional<double> OperationDuration(const aclptiProfilingDataResult& result, PmuDataLevel level);
    static const char* ReasonName(MissingReason reason);
};

} // namespace npucompute
