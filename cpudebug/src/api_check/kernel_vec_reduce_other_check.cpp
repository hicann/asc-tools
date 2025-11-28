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
 * \file kernel_vec_reduce_other_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "model/model_factory_mask.h"
#include "kernel_vec_reduce_other_check.h"

namespace AscendC {
namespace check {

bool TikcppVecReduceOtherCheck::CheckWholeReduceDtypeBytes(const std::string &errMsg)
{
    uint32_t dstDtypeBytes = param_.dstDtypeBytes;
    uint32_t srcDtypeBytes = param_.src0DtypeBytes;
    if (dstDtypeBytes != srcDtypeBytes) {
        CHECK_LOG_ERROR("%s, ""Reduce need dst data type (%u),dst src type (%u), should be same",
            errMsg.c_str(), dstDtypeBytes, srcDtypeBytes);
        return false;
    }
    return true;
}

bool TikcppVecReduceOtherCheck::CheckWholeReduceDstSize()
{
    uint32_t needCount = (param_.dstRepeatStride != 0) ? param_.repeatTimes * param_.dstRepeatStride : 1;
    uint64_t needSize = static_cast<uint64_t>(needCount * param_.dstDtypeBytes);
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param_.dstSize, "dstLocal", apiName.c_str()));
    return true;
}

static bool CheckTensorWhlSumOverflowLowCounter(std::vector<uint64_t>& maskArray, const VecReduceApiParams& param,
    const uint64_t unit, const std::string& tensorName, const std::string& apiName)
{
    uint32_t oneRepeatNum = ONE_REPEAT_BYTE_SIZE / param.dstDtypeBytes;                   // when counter mode, always full mask
    uint64_t elementNum = (maskArray.size() == 1) ? maskArray[0] : maskArray[1]; // maskLow means element num
    int32_t repeatTimes = (elementNum + oneRepeatNum - 1) / oneRepeatNum;
    uint32_t needSize = (repeatTimes - 1) * param.dstRepeatStride * unit + unit;
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param.dstSize, tensorName, apiName, ModeType::COUNTER_MODE));
    return true;
}

static bool CheckTensorWhlSumOverflowLowNorm(const VecReduceApiParams& param, const uint64_t unit,
    const std::string& tensorName, const std::string& apiName)
{
    uint32_t needSize = (param.repeatTimes - 1) * param.dstRepeatStride * unit + unit;
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param.dstSize, tensorName, apiName, ModeType::NORM_MODE));
    return true;
}

bool TikcppVecReduceOtherCheck::CheckWholeReduceDstSize(std::vector<uint64_t>& maskArray, const uint64_t unit,
    const std::string& tensorName)
{
    if (ModelFactoryGetMaskMode() == 1) { // counter mode
        return CheckTensorWhlSumOverflowLowCounter(maskArray, param_, unit, tensorName, apiName);
    }
    return CheckTensorWhlSumOverflowLowNorm(param_, unit, tensorName, apiName);
}

static uint64_t CalculatePairVecMaxOffset(const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride,
    const uint64_t maskLen, const uint64_t blockLen, uint32_t unit, const uint32_t dtypeBytes)
{
    if (repeatTimes == 0) {
        return 0;
    }
    ASSERT(blockLen != 0);
    uint64_t maskNum = maskLen / 2; // every 2 src get 1 dst
    uint64_t blkNumLastRep = DivCeil(maskNum, blockLen);   // last repeat needs x blocks for maskLen elements
    uint64_t eleNumLastBlk = ((maskNum % blockLen) != 0) ? (maskNum % blockLen) : blockLen;
    uint64_t maxOffset = (repeatTimes - 1) * unit * repStride + (blkNumLastRep - 1) * blkStride * blockLen *
        dtypeBytes + eleNumLastBlk * dtypeBytes;
    return maxOffset;
}

static uint64_t CalculatePairNeededTensorSize(std::vector<uint64_t>& maskArray, const uint32_t dtypeBytes,
    const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride, uint32_t unit)
{
    uint64_t maskVal = (maskArray.size() == 1) ? maskArray[0] : GetMaskLength(maskArray, dtypeBytes);
    ASSERT(dtypeBytes != 0);
    uint64_t eleNumPerBlock = static_cast<uint64_t>(PlatFormParams::ONE_BLK_SIZE) / dtypeBytes;
    uint64_t maxOffset = CalculatePairVecMaxOffset(repeatTimes, blkStride, repStride, maskVal, eleNumPerBlock, unit,
        dtypeBytes);
    return maxOffset;
}

static bool CheckTensorPairOverflowLowCounter(std::vector<uint64_t>& maskArray, const VecReduceApiParams& param,
    const std::string& tensorName, const std::string& apiName)
{
    std::vector<uint64_t> mainMaskArray = {0};
    std::vector<uint64_t> tailMaskArray = {0};
    uint64_t mainRepeatTimes = 0;
    uint64_t tailRepeatTimes = 0;
    CounterSplitMainTail(maskArray, param.dstDtypeBytes, mainRepeatTimes, tailRepeatTimes, mainMaskArray,
        tailMaskArray);
    uint64_t maskVal = (mainMaskArray.size() == 1) ? mainMaskArray[0] : GetMaskLength(mainMaskArray,
        param.dstDtypeBytes);
    uint32_t unit = maskVal / 2 * param.dstDtypeBytes;
    uint64_t mainBlkSize = CalculatePairNeededTensorSize(mainMaskArray, param.dstDtypeBytes, mainRepeatTimes,
        DEFAULT_BLK_STRIDE, param.dstRepeatStride, unit);
    uint64_t maxOffset = mainBlkSize;
    if (tailRepeatTimes > 0) {  // calculate tail block from the last repStride in main block
        uint64_t tailRepeatStart = mainRepeatTimes * param.dstRepeatStride * unit;
        uint64_t tailBlkSize = CalculatePairNeededTensorSize(tailMaskArray, param.dstDtypeBytes, tailRepeatTimes,
            DEFAULT_BLK_STRIDE, param.dstRepeatStride, unit);
        maxOffset = std::max(mainBlkSize, tailRepeatStart + tailBlkSize);
    }
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, param.dstSize, tensorName, apiName, ModeType::COUNTER_MODE));
    return true;
}

static bool CheckTensorPairOverflowLowNorm(std::vector<uint64_t>& maskArray, const VecReduceApiParams& param,
    const std::string& tensorName, const std::string& apiName)
{
    uint64_t maskVal = (maskArray.size() == 1) ? maskArray[0] : GetMaskLength(maskArray, param.dstDtypeBytes);
    uint32_t unit = maskVal / 2 * param.dstDtypeBytes;
    uint32_t lastRepeatSize = maskVal / 2;
    uint32_t needSize = (param.repeatTimes - 1) * param.dstRepeatStride * unit + lastRepeatSize *
        param.dstDtypeBytes;
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param.dstSize, tensorName, apiName, ModeType::NORM_MODE));
    return true;
}

bool TikcppVecReduceOtherCheck::CheckPairReduceDstSize(std::vector<uint64_t>& maskArray, const std::string& tensorName)
{
    if (ModelFactoryGetMaskMode() == 1) { // counter mode
        return CheckTensorPairOverflowLowCounter(maskArray, param_, tensorName, apiName);
    }
    return CheckTensorPairOverflowLowNorm(maskArray, param_, tensorName, apiName);
}

// this api do not support counter mode due to param elemsInOneRepeat is only for norm mode
bool TikcppVecReduceOtherCheck::CheckRepeatReduceDstSize()
{
    // in RepeatReduceSum, dstRepStride is in unit of element
    // 1 repeatTimes: 1 element      > 1 repeatTimes: 1 element + dstRepStride jump
    uint32_t expectedSize = ((param_.repeatTimes - 1) * param_.dstRepeatStride + 1) * param_.dstDtypeBytes;
    ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.dstSize, "dstLocal", apiName.c_str()));
    return true;
}

bool TikcppVecReduceOtherCheck::CheckAddrAlign()
{
    uint8_t alignByte = ONE_BLK_SIZE;
    bool dstRes = true;
    bool src0Res = true;
    if ((apiName == "BlockReduceMax") || (apiName == "BlockReduceMin") || (apiName == "BlockReduceSum")) {
        if (param_.dstDtypeBytes == sizeof(half)) {
            alignByte = 16; // half type align Bytes is 16B
        }
        dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, alignByte, "dst");
        src0Res = CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src0");
        return dstRes && src0Res;
    }
    if (apiName == "PairReduceSum") {
        dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst");
        src0Res = CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src0");
        return dstRes && src0Res;
    }
    alignByte = 4; // float type align Bytes is 4B
    if (param_.dstDtypeBytes == sizeof(half)) {
        alignByte = 2; // half type align Bytes is 2B
    }
    dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, alignByte, "dst");
    return dstRes;
}

// calculate max extent, aka the offset of the end of all effective element
// maskLen: each repeat calculate the first maskLen elements
// blockLen: element num per block
// return: in unit of element
static uint64_t CalculateByteVectorMaxOffset(const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride,
    const uint64_t maskLen, const uint64_t blockLen)
{
    if (repeatTimes == 0) {
        return 0;
    }
    ASSERT(blockLen != 0);
    uint64_t maskNum = (maskLen + blockLen - 1) / blockLen; // one block get one dst elements
    uint64_t blkNumLastRep = (maskNum + blockLen - 1) / blockLen;   // last repeat needs x blocks for maskNum elements
    uint64_t eleNumLastBlk = ((maskNum % blockLen) != 0) ? (maskNum % blockLen) : blockLen;
    uint64_t maxOffset = ((repeatTimes - 1) * repStride / 32 + (blkNumLastRep - 1) * blkStride) * blockLen +
        eleNumLastBlk;
    return maxOffset;
}

// Given repeatTimes and stride etc, to return total buffersize needed in unit of Bytes
static uint64_t CalculateNeededByteTensorSize(std::vector<uint64_t>& maskArray, const uint32_t dtypeBytes,
    const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride)
{
    uint64_t maskVal = (maskArray.size() == 1) ? maskArray[0] : GetMaskLength(maskArray, dtypeBytes);
    ASSERT(dtypeBytes != 0);
    uint64_t eleNumPerBlock = static_cast<uint64_t>(PlatFormParams::ONE_BLK_SIZE) / dtypeBytes;
    uint64_t maxOffset = CalculateByteVectorMaxOffset(repeatTimes, blkStride, repStride, maskVal, eleNumPerBlock);
    maxOffset = maxOffset * dtypeBytes;
    return maxOffset;
}

static bool CheckTensorByteOverflowLowCounter(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
    const std::string& tensorName, const std::string& apiName)
{
    std::vector<uint64_t> mainMaskArray = {0};
    std::vector<uint64_t> tailMaskArray = {0};
    uint64_t mainRepeatTimes = 0;
    uint64_t tailRepeatTimes = 0;
    CounterSplitMainTail(maskArray, params.dtypeSize, mainRepeatTimes, tailRepeatTimes, mainMaskArray,
        tailMaskArray);
    // when counter mode, repeatTimes given by user is not used
    // Need to compare: endpoint of mainBlock VS endpoint of tailBlock
    // Especially scenes where blkStride is much larger than repStride. mainBlock endpoint will be larger!!
    uint64_t mainBlkSize = CalculateNeededByteTensorSize(mainMaskArray, params.dtypeSize, mainRepeatTimes,
        params.blkStride, params.repStride);
    uint64_t maxOffset = mainBlkSize;
    if (tailRepeatTimes > 0) {  // calculate tail block from the last repStride in main block
        uint64_t tailRepeatStart = mainRepeatTimes * params.repStride / 32 * ONE_BLK_SIZE / 2;
        uint64_t tailBlkSize = CalculateNeededByteTensorSize(tailMaskArray, params.dtypeSize, tailRepeatTimes,
            params.blkStride, params.repStride); // the unit of repStride is Byte
        maxOffset = std::max(mainBlkSize, tailRepeatStart + tailBlkSize);
    }
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, params.bufferSize, tensorName, apiName, ModeType::COUNTER_MODE));
    return true;
}

// in normal mode, check whether the data calculated in cmd exceed the tensor size
static bool CheckTensorByteOverflowLowNorm(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
    const std::string& tensorName, const std::string& apiName)
{
    uint64_t maxOffset = CalculateNeededByteTensorSize(maskArray, params.dtypeSize, params.repeatTimes,
    params.blkStride, params.repStride);
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, params.bufferSize, tensorName, apiName, ModeType::NORM_MODE));
    return true;
}

// the unit of dstRepStride is Byte
bool TikcppVecReduceOtherCheck::CheckTensorByteOverflowLow(std::vector<uint64_t>& maskArray,
    const TensorOverflowParams& params, const std::string& tensorName)
{
    if (ModelFactoryGetMaskMode() == 1) { // counter mode
        return CheckTensorByteOverflowLowCounter(maskArray, params, tensorName, apiName);
    }
    return CheckTensorByteOverflowLowNorm(maskArray, params, tensorName, apiName);
}

bool TikcppVecReduceOtherCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(param_.dstDtypeBytes, param_.src0DtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));

    if ((apiName == "WholeReduceSum")) {
        ASCENDC_CHECK(CheckWholeReduceDtypeBytes("Check Whole Reduce data type"));
        ASCENDC_CHECK(CheckWholeReduceDstSize(maskArray, param_.dstDtypeBytes, "dstLocal"));
    }

    if ((apiName == "WholeReduceMax") || (apiName == "WholeReduceMin")) {
        ASCENDC_CHECK(CheckWholeReduceDtypeBytes("Check Whole Reduce data type"));
        ASCENDC_CHECK(CheckWholeReduceDstSize());
    }

    if ((apiName == "BlockReduceSum") || (apiName == "BlockReduceMax") || (apiName == "BlockReduceMin")) {
        TensorOverflowParams params = {param_.dstSize, param_.dstDtypeBytes,
            static_cast<uint64_t>(param_.repeatTimes), static_cast<uint64_t>(DEFAULT_BLK_STRIDE),
            static_cast<uint64_t>(param_.dstRepeatStride * param_.dstDtypeBytes * 8), false};
        ASCENDC_CHECK(CheckTensorByteOverflowLow(maskArray, params, "dstLocal"));
    }

    if (apiName == "PairReduceSum") {
        ASCENDC_CHECK(CheckPairReduceDstSize(maskArray, "dstLocal"));
    }

    if (apiName == "RepeatReduceSum") {
        ASCENDC_CHECK(CheckRepeatReduceDstSize());
    }

    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));

    ASCENDC_CHECK(CheckAddrAlign());

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        "check src tensor buffersize failed"));

    TensorOverflowParams params = {param_.src0Size, param_.src0DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src0BlockStride), static_cast<uint64_t>(param_.src0RepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcLocal"));
    return true;
}
}  // namespace check
}  // namespace AscendC