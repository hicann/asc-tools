/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/*!
 * \file kernel_vec_broadcast_to_mm_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_broadcast_to_mm_check.h"

namespace AscendC {
namespace check {
bool TikcppBroadCastToMMCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::L0C), "dst", "CO1"));
    ASCENDC_CHECK(CheckTensorScope(param_.srcLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src",
        "VECIN / VECOUT / VECCALC"));

    constexpr uint64_t baseNumL0C = 256;    // L0C has 16 * 16 elements as 1 fractal
    uint64_t dstAlignBytes = param_.dstDtypeBytes * baseNumL0C;
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, dstAlignBytes, "dst"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.srcAddr, param_.srcPos, param_.srcDtypeBytes, "src")); // dtype aligned

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.srcSize, GlobalParams::Instance().bufferSizeMap.at(param_.srcPos),
        "check src tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    // unit element
    return true;
};
}
}