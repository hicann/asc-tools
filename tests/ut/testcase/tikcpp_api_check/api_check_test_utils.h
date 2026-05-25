/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef ASCENDC_API_CHECK_TEST_UTILS_H
#define ASCENDC_API_CHECK_TEST_UTILS_H

#include <cstdint>
#include "kernel_event.h"
#include "kernel_utils.h"

namespace AscToolsUt {
struct TestTensor {
    uint64_t addr = 0;
    uint64_t length = 0;
    AscendC::TPosition pos = AscendC::TPosition::GM;
};

inline uint64_t AlignAddr(uint64_t addr)
{
    return ((addr + 31) / 32) * 32;
}

inline TestTensor MakeTensor(AscendC::TPosition pos, uint64_t byteSize, uint64_t byteOffset = 0)
{
    auto hardPos = AscendC::GetPhyType(pos);
    auto base = AscendC::ConstDefiner::Instance().GetHardwareBaseAddr(hardPos);
    return {reinterpret_cast<uint64_t>(base) + byteOffset, byteSize, pos};
}

inline uint8_t LogicPos(const TestTensor& tensor)
{
    return static_cast<uint8_t>(tensor.pos);
}
} // namespace AscToolsUt

#define ALIGN_ADDR(addr) AscToolsUt::AlignAddr(addr)

#endif // ASCENDC_API_CHECK_TEST_UTILS_H
