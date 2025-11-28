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
 * \file kernel_vec_binary_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_binary_check.h"

namespace AscendC {
namespace check {

bool TikcppVecBinaryCheck::CheckCmpTensorOverflowHigh(const uint32_t dtypeSize, const uint64_t bufferSize,
    const uint32_t calCount, const std::string& tensorName)
{
    uint64_t needSize = static_cast<uint64_t>(dtypeSize * calCount / 8); // 1 uint8 equal to 8 bits
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, bufferSize, tensorName, apiName));
    return true;
}

static uint64_t CalculateNeededCmpTensorSize(const uint64_t repeatTimes, const uint64_t srcDtypeBytes)
{
    if (repeatTimes == 0) {
        return 0;
    }
    ASCENDC_CHECK(srcDtypeBytes != 0);
    uint64_t maxOffset = repeatTimes *
        static_cast<uint64_t>(PlatFormParams::ONE_REP_BYTE_SIZE) / srcDtypeBytes / 8;  // 1 uint8 equal to 8 bits
    return maxOffset;
}

bool TikcppVecBinaryCheck::CheckCmpTensorOverflowLowNorm(const TensorOverflowParams& params,
    const std::string& tensorName)
{
    uint64_t maxOffset = CalculateNeededCmpTensorSize(params.repeatTimes, param_.src0DtypeBytes);
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, params.bufferSize, tensorName, apiName, ModeType::NORM_MODE));
    return true;
}

bool TikcppVecBinaryCheck::CommonCheck()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src0", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src1LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src1", supportPos));

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        "check src0 tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src1Size, GlobalParams::Instance().bufferSizeMap.at(param_.src1Pos),
        "check src1 tensor buffersize failed"));
    ASCENDC_CHECK(CheckAddrAlign());
    return true;
}

bool TikcppVecBinaryCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(std::max(param_.dstDtypeBytes, param_.src0DtypeBytes), param_.src1DtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));
    ASCENDC_CHECK(CommonCheck());

    TensorOverflowParams params = {param_.dstSize, param_.dstDtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.dstBlockStride), static_cast<uint64_t>(param_.dstRepeatStride), false};
    // check dst src0 and src1 tensor overflow
    if (apiName == "Compare" || apiName == "Compare operator") {
        ASCENDC_CHECK(CheckCmpTensorOverflowLowNorm(params, "dstLocal"));
    } else {
        ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "dstLocal"));
    }
    params = {param_.src0Size, param_.src0DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src0BlockStride), static_cast<uint64_t>(param_.src0RepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "src0Local"));
    params = {param_.src1Size, param_.src1DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src1BlockStride), static_cast<uint64_t>(param_.src1RepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "src1Local"));

    return true;
}

bool TikcppVecBinaryCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CommonCheck());

    if (apiName == "Compare" || apiName == "Compare operator") {
        ASCENDC_CHECK(CheckCmpTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.calCount, "dstLocal"));
    } else {
        ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.calCount, "dstLocal"));
    }
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, param_.calCount, "src0Local"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src1DtypeBytes, param_.src1Size, param_.calCount, "src1Local"));
    return true;
}

bool TikcppVecBinaryCheck::CheckAddrAlign()
{
    bool dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst");
    bool src0Res = CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src0");
    bool src1Res = CheckTensorAddrAlign(param_.src1Addr, param_.src1Pos, ONE_BLK_SIZE, "src1");
    return dstRes && src0Res && src1Res;
}
} // namespace check
} // namespace AscendC