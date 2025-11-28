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
 * \file kernel_vec_scatter_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_scatter_check.h"

namespace AscendC {
namespace check {

bool TikcppVecScatterCheck::CheckTensorSize(std::vector<uint64_t> maskArray)
{
    // max element calculated in a repeat
    uint64_t maskVal = (maskArray.size() == 1) ? maskArray[0] : GetMaskLength(maskArray, param_.dstDtypeBytes);

    // dstLocal:
    uint64_t blockLen = ONE_BLK_SIZE / param_.dstDtypeBytes;        // ele num in one block
    uint64_t blkNumLastRep = DivCeil(maskVal, blockLen);   // last repeat needs x blocks for maskLen elements
    uint64_t eleNumLastBlk = ((maskVal % blockLen) != 0) ? (maskVal % blockLen) : blockLen;
    uint64_t maxOffset = ((param_.repeatTimes - 1) * DEFAULT_REPEAT_STRIDE + (blkNumLastRep - 1) *
        DEFAULT_BLK_STRIDE) * blockLen + eleNumLastBlk;
    maxOffset = maxOffset * param_.dstDtypeBytes + param_.dstBaseAddr;
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, param_.dstSize, "dstLocal", "Scatter"));
    return true;
}

bool TikcppVecScatterCheck::CommonCheck()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.srcLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.dstOffsetLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dstOffset", supportPos));

    ASCENDC_CHECK(CheckAddrAlign());
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
            "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.srcSize, GlobalParams::Instance().bufferSizeMap.at(param_.srcPos),
        "check src tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstOffsetSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstOffsetPos),
        "check dstOffset tensor buffersize failed"));
    return true;
}

bool TikcppVecScatterCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = param_.dstDtypeBytes;
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));
    ASCENDC_CHECK(CommonCheck());
    ASCENDC_CHECK(CheckTensorSize(maskArray));
    TensorOverflowParams params = {param_.srcSize, param_.srcDtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(DEFAULT_BLK_STRIDE), static_cast<uint64_t>(param_.srcRepStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcLocal"));
    return true;
}
bool TikcppVecScatterCheck::CheckAddrAlign()
{
    bool dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst");
    bool srcRes = CheckTensorAddrAlign(param_.srcAddr, param_.srcPos, ONE_BLK_SIZE, "src");
    bool dstOffsetRes = CheckTensorAddrAlign(param_.dstOffsetAddr, param_.dstOffsetPos, ONE_BLK_SIZE, "dstOffset");
    return dstRes && srcRes && dstOffsetRes;
}

bool TikcppVecScatterCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CommonCheck());
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.count,
        "dstLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.srcDtypeBytes, param_.srcSize, param_.count,
        "srcLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstOffsetDtypeBytes, param_.dstOffsetSize, param_.count,
        "dstOffsetLocal"));
    return true;
}
} // namespace check
} // namespace AscendC
