/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "section_config.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace npu_compute {
namespace {

constexpr std::array<const char*, 5> kSupportedSections = {
    "PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache",
};

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool IsSupportedSection(const std::string& section)
{
    return std::find(kSupportedSections.begin(), kSupportedSections.end(), section) != kSupportedSections.end();
}

} // namespace

bool SectionConfig::LoadFromEnvironment(const char* name, std::string* error)
{
    Reset();
    if (name == nullptr || name[0] == '\0') {
        return Fail("environment variable name is empty", error);
    }

    const char* value = std::getenv(name);
    if (value == nullptr) {
        return Fail(std::string(name) + " is not set", error);
    }
    const std::string encoded(value);
    if (encoded.empty()) {
        return Fail(std::string(name) + " is empty", error);
    }

    std::vector<std::string> parsed_sections;
    std::size_t start = 0;
    while (start <= encoded.size()) {
        const std::size_t separator = encoded.find(',', start);
        const std::size_t length = separator == std::string::npos ? encoded.size() - start : separator - start;
        const std::string section = encoded.substr(start, length);
        if (section.empty()) {
            return Fail("section list contains an empty value", error);
        }
        if (!IsSupportedSection(section)) {
            return Fail("unknown section: " + section, error);
        }
        if (std::find(parsed_sections.begin(), parsed_sections.end(), section) == parsed_sections.end()) {
            parsed_sections.push_back(section);
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }

    const std::size_t section_count = parsed_sections.size();
    if (section_count == 0 || section_count > kSupportedSections.size()) {
        return Fail("section count is out of range", error);
    }
    sections_ = std::move(parsed_sections);
    section_pointers_.clear();
    section_pointers_.reserve(sections_.size());
    for (const std::string& section : sections_) {
        section_pointers_.push_back(section.c_str());
    }
    params_.sections = section_pointers_.data();
    params_.numSections = section_pointers_.size();
    params_.blockResult = ACLPTI_BLOCK_RESULT_ALL;
    params_.collectPipeline = false;
    params_.collectPcSampling = false;
    return true;
}

aclptiRangeProfilerSetConfigParams* SectionConfig::Params() { return &params_; }

std::string SectionConfig::JoinedSections() const
{
    std::string joined;
    for (const std::string& section : sections_) {
        if (!joined.empty()) {
            joined += ',';
        }
        joined += section;
    }
    return joined;
}

const std::vector<std::string>& SectionConfig::Sections() const { return sections_; }

void SectionConfig::Reset()
{
    params_ = {};
    section_pointers_.clear();
    sections_.clear();
}

} // namespace npu_compute
