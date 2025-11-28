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
 * \file kernel_vec_gather_mask_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_gather_mask_check.h"
namespace AscendC {
namespace check {
bool TikcppGatherMaskCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src0", supportPos));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src0"));
    if (param_.src1Pattern == 0) {
        ASCENDC_CHECK(CheckTensorScope(param_.src1LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src1", supportPos));
        ASCENDC_CHECK(CheckTensorAddrAlign(param_.src1Addr, param_.src1Pos, ONE_BLK_SIZE, "src1"));
    }

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        "check src0 tensor buffersize failed"));
    if (param_.src1Pattern == 0) {
        ASCENDC_CHECK(
            CheckBufferSizeOverFlow(param_.src1Size, GlobalParams::Instance().bufferSizeMap.at(param_.src1Pos),
            "check src1 tensor buffersize failed"));
    }

    // mask is invalid in normal mode. When counter mode, update mask array
    if (param_.reduceMode) {
        uint32_t maxByteLen = std::max(std::max(param_.dstDtypeBytes, param_.src0DtypeBytes), param_.src1DtypeBytes);
        ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));
    }

    TensorOverflowParams params = {param_.src0Size, param_.src0DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src0BlockStride), static_cast<uint64_t>(param_.src0RepeatStride), param_.reduceMode
    };
    ASCENDC_CHECK(CheckTensorOverflowLowGathermask(maskArray, params, "src0Local"));
    return true;
}
} // namespace check
} // namespace AscendC
