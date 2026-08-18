// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/source_resolver.h"

#include "aclsan/aclsan_api.h"

#include <array>
#include <iomanip>
#include <sstream>

namespace npu::sanitizer {

std::string SourceResolver::Resolve(const InstructionContext& instruction) const
{
    if (!instruction.present) {
        return {};
    }
    AclsanDevicePcQuery query{};
    query.version = ACLSAN_API_VERSION;
    query.size = sizeof(query);
    query.launchId = instruction.launchId;
    query.binaryId = instruction.binaryId;
    query.functionId = instruction.functionId;
    query.siteId = instruction.siteId;
    query.pc = instruction.pc;
    std::array<char, 8192> payload{};
    uint64_t payloadBytes = 0;
    if (aclsanSymbolizeDevicePc(&query, payload.data(), payload.size(), &payloadBytes) == ACLSAN_STATUS_SUCCESS &&
        payloadBytes != 0 && payloadBytes <= payload.size()) {
        return std::string(payload.data(), static_cast<size_t>(payloadBytes));
    }

    AclsanPatchSiteInfo site{};
    site.version = ACLSAN_API_VERSION;
    site.size = sizeof(site);
    if (instruction.siteId != 0 && aclsanGetPatchSiteInfo(instruction.siteId, &site) == ACLSAN_STATUS_SUCCESS) {
        std::ostringstream output;
        output << (site.sourceFile != nullptr ? site.sourceFile : "<unknown>");
        if (site.sourceLine != 0) {
            output << ':' << site.sourceLine;
        }
        output << " in " << (site.functionName != nullptr ? site.functionName : "<unknown>");
        if (site.opName != nullptr && site.opName[0] != '\0') {
            output << " op=" << site.opName;
        }
        return output.str();
    }

    std::ostringstream fallback;
    fallback << "device pc=0x" << std::hex << instruction.pc << std::dec << " site=" << instruction.siteId;
    return fallback.str();
}

} // namespace npu::sanitizer
