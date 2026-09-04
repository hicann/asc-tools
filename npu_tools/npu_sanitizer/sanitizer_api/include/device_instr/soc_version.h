/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_SOC_VERSION_H_
#define NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_SOC_VERSION_H_

#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <unordered_map>

namespace aclsan {

enum class SocVersion : uint8_t {
    DAV_3510,
};

using SocVersionMappings = std::unordered_map<std::string, SocVersion>;

inline const SocVersionMappings& GetSocVersionMappings() noexcept
{
    static const SocVersionMappings mappings{{
        {"Ascend950PR_9599", SocVersion::DAV_3510},  {"Ascend950PR_958a", SocVersion::DAV_3510},
        {"Ascend950PR_9589", SocVersion::DAV_3510},  {"Ascend950PR_958b", SocVersion::DAV_3510},
        {"Ascend950PR_9579", SocVersion::DAV_3510},  {"Ascend950PR_957b", SocVersion::DAV_3510},
        {"Ascend950PR_957bx", SocVersion::DAV_3510}, {"Ascend950PR_957c", SocVersion::DAV_3510},
        {"Ascend950PR_957d", SocVersion::DAV_3510},  {"Ascend950PR_950z", SocVersion::DAV_3510},
        {"Ascend950DT_950x", SocVersion::DAV_3510},  {"Ascend950DT_950y", SocVersion::DAV_3510},
        {"Ascend950DT_95A1", SocVersion::DAV_3510},  {"Ascend950DT_95A2", SocVersion::DAV_3510},
        {"Ascend950DT_9591", SocVersion::DAV_3510},  {"Ascend950DT_9592", SocVersion::DAV_3510},
        {"Ascend950DT_9595", SocVersion::DAV_3510},  {"Ascend950DT_9596", SocVersion::DAV_3510},
        {"Ascend950DT_9581", SocVersion::DAV_3510},  {"Ascend950DT_9582", SocVersion::DAV_3510},
        {"Ascend950DT_9582x", SocVersion::DAV_3510}, {"Ascend950DT_9583", SocVersion::DAV_3510},
        {"Ascend950DT_9584", SocVersion::DAV_3510},  {"Ascend950DT_9585", SocVersion::DAV_3510},
        {"Ascend950DT_9586", SocVersion::DAV_3510},  {"Ascend950DT_9587", SocVersion::DAV_3510},
        {"Ascend950DT_9588", SocVersion::DAV_3510},  {"Ascend950DT_9571", SocVersion::DAV_3510},
        {"Ascend950DT_9572", SocVersion::DAV_3510},  {"Ascend950DT_9573", SocVersion::DAV_3510},
        {"Ascend950DT_9574", SocVersion::DAV_3510},  {"Ascend950DT_9575", SocVersion::DAV_3510},
        {"Ascend950DT_9576", SocVersion::DAV_3510},  {"Ascend950DT_9577", SocVersion::DAV_3510},
        {"Ascend950DT_9578", SocVersion::DAV_3510},
    }};
    return mappings;
}

inline std::optional<SocVersion> ResolveSocVersion(const char* socName) noexcept
{
    if (socName == nullptr || socName[0] == '\0') {
        return std::nullopt;
    }
    try {
        const SocVersionMappings& mappings = GetSocVersionMappings();
        const auto iterator = mappings.find(socName);
        if (iterator == mappings.end()) {
            return std::nullopt;
        }
        return iterator->second;
    } catch (const std::bad_alloc&) {
        return std::nullopt;
    }
}

} // namespace aclsan

#endif // NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_SOC_VERSION_H_
