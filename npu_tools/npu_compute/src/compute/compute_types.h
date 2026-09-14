#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace npucompute {

enum class PmuDataLevel {
    Block,
    Task,
};

struct ReportConfig {
    std::string outputDirectory;
    std::string mirrorOutputDirectory;
    double frequencyMhz = 1000.0;
    double aicFrequencyMhz = 0.0;
    double aivFrequencyMhz = 0.0;
    std::string socName = "950X";
    PmuDataLevel pmuDataLevel = PmuDataLevel::Block;
    bool fixedOutputDirectory = false;
};

struct KernelMetadata {
    std::optional<std::string> name;
    std::optional<uint64_t> blockDim;
    std::optional<int32_t> deviceId;
    std::optional<double> ratedAicFrequencyMhz;
    std::optional<double> ratedAivFrequencyMhz;
};

} // namespace npucompute
