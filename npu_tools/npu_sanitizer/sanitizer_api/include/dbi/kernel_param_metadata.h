// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef ACLSAN_KERNEL_PARAM_METADATA_H
#define ACLSAN_KERNEL_PARAM_METADATA_H

#include <cstdint>
#include <string>

namespace aclsan {
// 更新插桩后.o的.ascend.meta的section段，使得该内容与dbi的统一偏移保持一致
// original: 插桩前原始device elf的完整二进制内容     patched: bisheng-tune插桩后devce elf的完整二进制内容
bool ModifyKernelParamMetadata(
    const std::string& original, std::string& patched, uint32_t traceOffset, std::string& diagnostic);
} // namespace aclsan

#endif
