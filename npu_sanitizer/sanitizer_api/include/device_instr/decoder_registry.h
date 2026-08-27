/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_DECODER_REGISTRY_H_
#define NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_DECODER_REGISTRY_H_

#include "device_instr/decoder.h"

namespace aclsan {

// 根据运行时aclrtGetSocName获取的 SoC 名称，找到对应架构的指令解码器。
const DeviceInstructionDecoder* FindDeviceInstructionDecoder(const char* socName) noexcept;

} // namespace aclsan

#endif // NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_DECODER_REGISTRY_H_
