/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_instr/decoder_registry.h"

#include "device_instr/arch/dav_3510/decoder.h"
#include "internal/aclsan_log.h"

#include <array>
#include <cstring>

namespace aclsan {
namespace {

using GetDecoderFunc = const DeviceInstructionDecoder& (*)() noexcept;

struct DecoderRegistration {
    const char* socName;
    GetDecoderFunc getDecoder;
};

// TODO: 映射表需要加全
constexpr std::array<DecoderRegistration, 1> DECODER_REGISTRATIONS{{
    {"Ascend950PR_9599", dav3510::GetDeviceInstructionDecoder},
}};

} // namespace

const DeviceInstructionDecoder* FindDeviceInstructionDecoder(const char* socName) noexcept
{
    if (socName == nullptr || socName[0] == '\0') {
        ASC_SAN_ERROR("FindDeviceInstructionDecoder failed: socName is nullptr or empty");
        return nullptr;
    }
    for (const DecoderRegistration& registration : DECODER_REGISTRATIONS) {
        if (std::strcmp(socName, registration.socName) == 0) {
            return &registration.getDecoder();
        }
    }
    ASC_SAN_ERROR("FindDeviceInstructionDecoder failed: unsupported SoC=%s", socName);
    return nullptr;
}

} // namespace aclsan
