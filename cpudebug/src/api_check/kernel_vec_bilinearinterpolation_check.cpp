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
 * \file kernel_vec_bilinearinterpolation_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_bilinearinterpolation_check.h"

namespace AscendC {
namespace check {

const uint64_t ELE_PER_REPEAT = 8;   // 1 repeat && repeatMode = 1 -> 8 src1 elements

bool TikcppVecBilinearInterpolationCheck::CheckTensorSize(std::vector<uint64_t> maskArray)
{
    uint8_t n = param_.hRepeat;
    uint8_t m = param_.vRepeat;
    // max element calculated in a repeat
    uint64_t maskVal = (maskArray.size() == 1) ? maskArray[0] : GetMaskLength(maskArray, param_.src0DtypeBytes);

    // src0OffsetLocal: total: m * n block, but last block is related to maskVal
    uint64_t eleNumPerOffset = ONE_BLK_SIZE / param_.src0DtypeBytes;      // each offset element -> element in src0
    // example: maskVal = 17, dtype = half, thus offset needs at least 2 elements to pick blocks in src0
    uint64_t offsetNumPerRep = (maskVal + eleNumPerOffset - 1) / eleNumPerOffset;
    uint64_t expectedSize = (m * n - 1) * ONE_BLK_SIZE + offsetNumPerRep * param_.offsetDtypeBytes;
    ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.offsetSize, "src0OffsetLocal", "BilinearInterpolation"));

    // src0Local: needs to read values in src0OffsetLocal. Thus make optimistic check
    // example: maskVal = 7, only need 7 elements at most; maskVal = 16, probably src0Local all points to same block
    expectedSize = (maskVal <= eleNumPerOffset) ? maskVal * param_.src0DtypeBytes : ONE_BLK_SIZE;
    ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.src0Size, "src0Local", "BilinearInterpolation"));

    // src1Local: similar to src0Offset
    expectedSize = (param_.repeatMode) ? ELE_PER_REPEAT * (m * n -1) + offsetNumPerRep : m * n;  // in unit of elements
    ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize * param_.src1DtypeBytes, param_.src1Size, "src1Local",
        "BilinearInterpolation"));

    // dstLocal:
    uint64_t blockLen = ONE_BLK_SIZE / param_.dstDtypeBytes;        // ele num in one block
    uint64_t blkNumLastRep = DivCeil(maskVal, blockLen);   // last repeat needs x blocks for maskLen elements
    uint64_t eleNumLastBlk = ((maskVal % blockLen) != 0) ? (maskVal % blockLen) : blockLen;
    uint64_t dstLastRepStart = (m - 1) * param_.vROffset;                                               // unit of elem
    uint64_t dstLastRepEleNum = (blkNumLastRep - 1) * param_.dstBlockStride * blockLen + eleNumLastBlk; // unit of elem
    expectedSize = (dstLastRepStart + dstLastRepEleNum) * param_.dstDtypeBytes;
    ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.dstSize, "dstLocal", "BilinearInterpolation"));
    return true;
}

bool TikcppVecBilinearInterpolationCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(std::max(param_.dstDtypeBytes, param_.src0DtypeBytes), param_.src1DtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));

    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dstLocal", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src0Local", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.offsetLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src0OffsetLocal",
        supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src1LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src1Local", supportPos));

    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dstLocal"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src0Local"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.offsetAddr, param_.offsetPos, ONE_BLK_SIZE, "src0OffsetLocal"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.src1Addr, param_.src1Pos, ONE_BLK_SIZE, "src1Local"));

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        "Failed to check src0Local tensor buffersize"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src1Size, GlobalParams::Instance().bufferSizeMap.at(param_.src1Pos),
        "Failed to check src1Local tensor buffersize"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.offsetSize,
        GlobalParams::Instance().bufferSizeMap.at(param_.offsetPos),
        "Failed to check src0OffsetLocal tensor buffersize"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "Failed to check dstLocal tensor buffersize"));

    ASCENDC_CHECK(CheckTensorSize(maskArray));
    return true;
}

}
}