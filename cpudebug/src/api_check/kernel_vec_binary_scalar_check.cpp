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
 * \file kernel_vec_binary_scalar_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_binary_scalar_check.h"

namespace AscendC {
namespace check {

bool TikcppVecBinaryScalarCheck::CommonCheck()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        "check src tensor buffersize failed"));
    return true;
}

bool TikcppVecBinaryScalarCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(param_.dstDtypeBytes, param_.src0DtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));
    ASCENDC_CHECK(CommonCheck());

    // check tensor overflow
    TensorOverflowParams params = {param_.dstSize, param_.dstDtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.dstBlockStride), static_cast<uint64_t>(param_.dstRepeatStride), false};
    // check dst src0 tensor overflow
    if (Int4Setter::Instance().GetDstInt4()) {
        Int4Setter::Instance().SetInt4();
    }
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "dstLocal"));
    params = {param_.src0Size, param_.src0DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src0BlockStride), static_cast<uint64_t>(param_.src0RepeatStride), false};
    if (Int4Setter::Instance().GetSrcInt4()) {
        Int4Setter::Instance().SetInt4();
    }
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
    if (param_.enableFlexibleScalar != 0) {
        if (param_.scalarPos == 1) {
            ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "src0"));
        } else {
            ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "src1"));
        }
    } else {
        ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcLocal"));
    }
#else
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcLocal"));
#endif
    return true;
}

bool TikcppVecBinaryScalarCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CommonCheck());
    if (Int4Setter::Instance().GetDstInt4()) {
        Int4Setter::Instance().SetInt4();
    }
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.calCount, "dstLocal"));
    if (Int4Setter::Instance().GetSrcInt4()) {
        Int4Setter::Instance().SetInt4();
    }

#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
    if (param_.enableFlexibleScalar != 0) {
        if (param_.scalarPos == 1) {
            ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, param_.calCount, "src0"));
        } else {
            ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, param_.calCount, "src1"));
        }
    } else {
        ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, param_.calCount, "srcLocal"));
    }
#else
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, param_.calCount, "srcLocal"));
#endif
    return true;
}

}
}