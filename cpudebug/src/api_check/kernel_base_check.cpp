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
 * \file kernel_base_check.cpp
 * \brief
 */
#include "kernel_base_check.h"
#include "kernel_check_params.h"
#include "kernel_utils.h"
#include "model/model_factory_mask.h"

namespace AscendC {
namespace check {
constexpr const uint64_t CONST_UINT32_MAX = 0xffffffff;

// extract total element num in each repeat. Used in normal mode
uint64_t GetMaskLength(std::vector<uint64_t> &maskArray, const uint32_t dtypeSize)
{
    uint64_t maskLen = 0;
    if (maskArray[static_cast<int32_t>(CommonParams::MASK_HIGH_IDX)] == 0) {
        // get last element index in mask_low_idx
        for (uint32_t i = 0; i < static_cast<uint32_t>(CommonParams::MASK_MAX_ELE_LEN); ++i) {
            if ((maskArray[static_cast<int32_t>(CommonParams::MASK_LOW_IDX)] & (CONST_MASK_VALUE >> i)) != 0) {
                maskLen = static_cast<uint64_t>(CommonParams::MASK_MAX_ELE_LEN) - static_cast<uint64_t>(i);
                break;
            }
        }
    } else {
        // get last element index in mask_high_idx, then add 64
        for (uint32_t i = 0; i < static_cast<uint32_t>(CommonParams::MASK_MAX_ELE_LEN); ++i) {
            if ((maskArray[static_cast<int32_t>(CommonParams::MASK_HIGH_IDX)] & (CONST_MASK_VALUE >> i)) != 0) {
                maskLen = static_cast<uint64_t>(CommonParams::MASK_MAX_ELE_LEN) +
                    static_cast<uint64_t>(CommonParams::MASK_MAX_ELE_LEN) - static_cast<uint64_t>(i);
                break;
            }
        }
    }
    if (dtypeSize >= sizeof(uint32_t)) {
        uint64_t maxElePerRep = DEFAULT_BLOCK_SIZE / dtypeSize;
        maskLen = (maskLen >= maxElePerRep) ? maxElePerRep : maskLen;
    }
    return maskLen;
}

bool CheckTensorSizeOverflow(uint64_t expectedSize, uint64_t tensorSize, const std::string& tensorName,
    const std::string& apiName, const ModeType mode)
{
    std::string curMode = "";
    if (mode == ModeType::NORM_MODE) {
        curMode = " when in normal mode";
    } else if (mode == ModeType::COUNTER_MODE) {
        curMode = " when in counter mode";
    }
    ASCENDC_CHECK_AND_LOG(expectedSize <= tensorSize, {CHECK_LOG_ERROR("Failed to check %s size in %s%s, tensor size "
        "needs to be at least %lu bytes, while current tensor size is only %lu bytes.", tensorName.c_str(),
        apiName.c_str(), curMode.c_str(), expectedSize, tensorSize);});
    return true;
}

bool TikcppBaseCheck::CheckTensorOverflowHigh(const uint32_t dtypeSize, const uint64_t bufferSize,
    const uint32_t calCount, const std::string& tensorName) const
{
    uint64_t needSize = static_cast<uint64_t>(dtypeSize * calCount);
    if (Int4Setter::Instance().GetInt4()) {
        needSize = static_cast<uint64_t>(calCount / INT4_TWO);
        Int4Setter::Instance().ResetInt4();
    }
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, bufferSize, tensorName, apiName));
    return true;
}

bool TikcppBaseCheck::UpdateMaskArrayAndCheck(std::vector<uint64_t>& maskArray, const uint32_t maxByteLen) const
{
    // do not use mask given by user, use mask value stored in registers
    if (!MaskSetter::Instance().GetMask()) {
        maskArray = {ModelFactoryGetMaskHigh(), ModelFactoryGetMaskLow()};
        CHECK_LOG_INFO("Due to isSetMask = false, maskArray is changed to maskHigh %lu, maskLow %lu.",
            ModelFactoryGetMaskHigh(), ModelFactoryGetMaskLow());
    }
    MaskSetter::Instance().SetMask(true);
    if (maxByteLen >= sizeof(int32_t) && maskArray.size() == 2) { // when size 2 need to update maskHigh
        // Example: mask[64, 64] but dtype is float, maximum only read 1 64. maskHigh is unused, need to set to 0.
        maskArray[0] = 0;    // in counter / norm mode, both maskHigh is in fact 0
    }
    if (maskArray.size() != 1 && maskArray.size() != 2) {   // mask = len 1, mask[2] = len 2
        ASCENDC_CHECK_AND_LOG(false, {CHECK_LOG_ERROR("maskArray size is %lu, which should be 1 or 2.",
            maskArray.size());});
    } else if (maskArray.size() == 1) {
        // when norm mode, update mask value to <= 128 when array size is 1
        if (ModelFactoryGetMaskMode() == 0) {
            uint64_t maxElePerRep = DEFAULT_BLOCK_SIZE / maxByteLen;
            maskArray[0] = std::min(maskArray[0], maxElePerRep);
        }
        ASCENDC_CHECK_AND_LOG(CheckMaskImm(maskArray[0]), {CHECK_LOG_ERROR("When maskArray size is 1, mask value %lu "
            "is invalid", maskArray[0]);});
    } else {
        ASCENDC_CHECK_AND_LOG(CheckMaskArray(maskArray), {CHECK_LOG_ERROR("When maskArray size is 2, maskHigh %lu, "
            "maskLow %lu is invalid", maskArray[0], maskArray[1]);});
    }
    return true;
}

bool TikcppBaseCheck::CheckTensorScope(const uint8_t logicPos, const uint8_t expectedPos,
    const std::string& tensorInfo, const std::string& posInfo) const
{
    auto& hwNameMap = GlobalParams::Instance().hardwareNameMap;
    auto& logicNameMap = ConstDefiner::Instance().logicNameMap;
    // tensorPos从逻辑位置转换到物理位置
    uint8_t hardPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(logicPos)));
    if (hwNameMap.find(hardPos) == hwNameMap.end() || hwNameMap.find(expectedPos) == hwNameMap.end()) {
        CHECK_LOG_ERROR("the tensorPos/expectPos hardware pos not found, tensorPos index is %hhu, expectedPos is %hhu.",
            hardPos, expectedPos);
        return false;
    }

    ASCENDC_CHECK_AND_LOG(hardPos == expectedPos, {CHECK_LOG_ERROR("Failed to check %s tensor position in %s, "
        "supported positions are %s, current position is %s.", tensorInfo.c_str(), apiName.c_str(), posInfo.c_str(),
        logicNameMap.at(logicPos).c_str());});
    return true;
}

bool TikcppBaseCheck::CheckBufferSizeOverFlow(const uint64_t localSize, const uint64_t bufferSize,
    const std::string& errMsg) const
{
    if (localSize > bufferSize) {
        CHECK_LOG_ERROR("%s, Allocated buffer size overflow, allocate buffer size is %lu, the limit size is %lu.",
            errMsg.c_str(), localSize, bufferSize);
        return false;
    }
    return true;
}

// check vector mask input range
bool TikcppBaseCheck::CheckMaskArray(std::vector<uint64_t> maskArray) const
{
    // counter mode
    if (ModelFactoryGetMaskMode() == 1) {
        if (maskArray[1] > CONST_UINT32_MAX) {  // only use maskLow[0:32] as counter mask. Only give warning!
            CHECK_LOG_WARNING("Failed to check mask array in counter mode, maskLow must be in range 0 ~ %lu, current "
                "value is %lu", CONST_UINT32_MAX, maskArray[1]);
        }
        return true;
    }
    // normal mode
#if defined (__NPU_ARCH__) && (__NPU_ARCH__ == 1001 || __NPU_ARCH__ == 2002)
    ASCENDC_CHECK_AND_LOG(maskArray[0] != 0ULL || maskArray[1] != 0ULL, {CHECK_LOG_ERROR("During calculation in "
        "normal mode in Ascend910 / Ascend310p / Ascend610, maskHigh %lu and maskLow %lu cannot be both 0.",
        maskArray[0], maskArray[1]);});
#endif
    return true;
}

// check mask value is in valid range
bool TikcppBaseCheck::CheckMaskImm(const uint64_t mask) const
{
    // counter mode: mask means element num
    if (ModelFactoryGetMaskMode() == 1) {
        if (mask > CONST_UINT32_MAX) {  // only use mask[0:32] as counter mask. Only give warning!
            CHECK_LOG_WARNING("Failed to check mask in counter mode, mask must be in range 0 ~ %lu, current value "
                "is %lu", CONST_UINT32_MAX, mask);
        }
        return true;
    }
    // normal mode
#if defined (__NPU_ARCH__) && (__NPU_ARCH__ == 1001 || __NPU_ARCH__ == 2002)
    ASCENDC_CHECK_AND_LOG(mask != 0ULL, {CHECK_LOG_ERROR("During calculation in normal mode in Ascend910 / Ascend310p "
        "/ Ascend610, mask %lu cannot be 0.", mask);});
#endif
    return true;
}

// when counter mode, split into norm mode with main block and tail block
void CounterSplitMainTail(std::vector<uint64_t>& maskArray, const uint32_t dtypeBytes, uint64_t& mainRepeatTimes,
    uint64_t& tailRepeatTimes, std::vector<uint64_t>& mainMaskArray, std::vector<uint64_t>& tailMaskArray)
{
    uint32_t oneRepeatNum = ONE_REPEAT_BYTE_SIZE / dtypeBytes;                   // when counter mode, always full mask
    uint64_t elementNum = (maskArray.size() == 1) ? maskArray[0] : maskArray[1]; // maskLow means element num
    mainRepeatTimes = elementNum / oneRepeatNum;
    uint64_t tailEleNum = elementNum % oneRepeatNum;
    tailRepeatTimes = (tailEleNum == 0) ? 0 : 1;
    mainMaskArray = {oneRepeatNum};
    tailMaskArray = {tailEleNum};
}

// calculate max extent, aka the offset of the end of all effective element
// maskLen: each repeat calculate the first maskLen elements
// blockLen: element num per block
// return: in unit of element
uint64_t CalculateVectorMaxOffset(const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride,
    const uint64_t maskLen, const uint64_t blockLen)
{
    if (repeatTimes == 0) {
        return 0;
    }
    ASSERT(blockLen != 0);
    uint64_t blkNumLastRep = DivCeil(maskLen, blockLen);   // last repeat needs x blocks for maskLen elements
    uint64_t eleNumLastBlk = ((maskLen % blockLen) != 0) ? (maskLen % blockLen) : blockLen;
    uint64_t maxOffset = ((repeatTimes - 1) * repStride + (blkNumLastRep - 1) * blkStride) * blockLen + eleNumLastBlk;
    return maxOffset;
}

namespace {
// Given repeatTimes and stride etc, to return total buffersize needed in unit of Bytes
uint64_t CalculateNeededTensorSize(std::vector<uint64_t>& maskArray, const uint32_t dtypeBytes,
    const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride)
{
    uint64_t maskVal = (maskArray.size() == 1) ? maskArray[0] : GetMaskLength(maskArray, dtypeBytes);
    ASSERT(dtypeBytes != 0);
    uint64_t eleNumPerBlock = static_cast<uint64_t>(PlatFormParams::ONE_BLK_SIZE) / dtypeBytes;
    uint64_t maxOffset = CalculateVectorMaxOffset(repeatTimes, blkStride, repStride, maskVal, eleNumPerBlock);
    maxOffset = maxOffset * dtypeBytes;
    if (Int4Setter::Instance().GetInt4()) {
        eleNumPerBlock = static_cast<uint64_t>(PlatFormParams::ONE_BLK_SIZE) * INT4_TWO;
        maxOffset = CalculateVectorMaxOffset(repeatTimes, blkStride, repStride, maskVal, eleNumPerBlock);
        maxOffset = maxOffset / INT4_TWO;
        Int4Setter::Instance().ResetInt4();
    }
    return maxOffset;
}

// in counter mode, check whether the data calculated in cmd exceed the tensor size
// need to convert to norm mode with main block and tail block
bool CheckTensorOverflowLowCounter(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
    const std::string& tensorName, const std::string& apiName)
{
    std::vector<uint64_t> mainMaskArray = {0};
    std::vector<uint64_t> tailMaskArray = {0};
    uint64_t mainRepeatTimes = 0;
    uint64_t tailRepeatTimes = 0;
    CounterSplitMainTail(maskArray, params.dtypeSize, mainRepeatTimes, tailRepeatTimes, mainMaskArray, tailMaskArray);
    // when counter mode, repeatTimes given by user is not used
    // Need to compare: endpoint of mainBlock VS endpoint of tailBlock
    // Especially scenes where blkStride is much larger than repStride. mainBlock endpoint will be larger!!
    uint64_t mainBlkSize = CalculateNeededTensorSize(mainMaskArray, params.dtypeSize, mainRepeatTimes, params.blkStride,
        params.repStride);
    uint64_t maxOffset = mainBlkSize;
    if (tailRepeatTimes > 0) {  // calculate tail block from the last repStride in main block
        uint64_t tailRepeatStart = mainRepeatTimes * params.repStride * ONE_BLK_SIZE;
        uint64_t tailBlkSize = CalculateNeededTensorSize(tailMaskArray, params.dtypeSize, tailRepeatTimes,
            params.blkStride, params.repStride);
        maxOffset = std::max(mainBlkSize, tailRepeatStart + tailBlkSize);
    }
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, params.bufferSize, tensorName, apiName, ModeType::COUNTER_MODE));
    return true;
}

// in normal mode, check whether the data calculated in cmd exceed the tensor size
bool CheckTensorOverflowLowNorm(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
    const std::string& tensorName, const std::string& apiName)
{
    uint64_t maxOffset = CalculateNeededTensorSize(maskArray, params.dtypeSize, params.repeatTimes, params.blkStride,
        params.repStride);
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, params.bufferSize, tensorName, apiName, ModeType::NORM_MODE));
    return true;
}

// in counter mode, check whether the data calculated in cmd exceed the tensor size for GatherMask
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
     (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
bool CheckTensorOverflowLowCounterGatherMask(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
    const std::string& tensorName)
{
    uint64_t maskValue = (maskArray.size() == 1) ? maskArray[0] : maskArray[1];
    uint64_t eleNumPerBlock = ONE_BLK_SIZE / params.dtypeSize;
    uint64_t blockNumPerRep = (maskValue + eleNumPerBlock - 1) / eleNumPerBlock;   // total blks needed for mask num
    uint64_t repeatOffset = (params.repeatTimes - 1) * params.repStride * ONE_BLK_SIZE;   // last repeat start pos
    uint64_t extraNum = blockNumPerRep * eleNumPerBlock - maskValue;        // elements that do not need in tail blocks
    uint64_t blockOffset = ((blockNumPerRep - 1) * params.blkStride + 1) * ONE_BLK_SIZE; // endblk pos(include extraNum)
    uint64_t maxOffset = repeatOffset + blockOffset - extraNum * params.dtypeSize;
    ASCENDC_CHECK(CheckTensorSizeOverflow(maxOffset, params.bufferSize, tensorName, "GatherMask",
        ModeType::COUNTER_MODE));
    return true;
}
#endif
}  // namespace


// check whether the needed size overflow the allocated buffersize, the unit is bytes.
bool TikcppBaseCheck::CheckTensorOverflowLow(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
    const std::string& tensorName) const
{
    if (ModelFactoryGetMaskMode() == 1) { // counter mode
        return CheckTensorOverflowLowCounter(maskArray, params, tensorName, apiName);
    }
    return CheckTensorOverflowLowNorm(maskArray, params, tensorName, apiName);
}

// check whether the needed size overflow the allocated buffersize, the unit is bytes.
bool TikcppBaseCheck::CheckTensorOverflowLowGathermask(std::vector<uint64_t>& maskArray,
    const TensorOverflowParams& params, const std::string& tensorName) const
{
    if (params.isCounter) { // counter mode
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
     (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
        return CheckTensorOverflowLowCounterGatherMask(maskArray, params, tensorName);
#else
        return CheckTensorOverflowLowCounter(maskArray, params, tensorName, apiName);
#endif
    }
    std::vector<uint64_t> maskArrayReal = {FULL_MASK, FULL_MASK};  // when norm mode, GatherMask neglect mask
    return CheckTensorOverflowLowNorm(maskArrayReal, params, tensorName, apiName);
}

bool TikcppBaseCheck::CheckTensorOverflowLowBrcb(const TensorOverflowParams& params,
    const std::string& tensorName) const
{
    std::vector<uint64_t> maskArrayReal = {FULL_MASK, FULL_MASK};
    return CheckTensorOverflowLowNorm(maskArrayReal, params, tensorName, apiName);
}

bool TikcppBaseCheck::CheckTensorAddrAlign(const uint64_t tensorAddr, const uint8_t phyPos, const uint64_t alignBytes,
    const std::string& tensorInfo) const
{
    uint64_t hardwareBaseAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        ConstDefiner::Instance().GetHardwareBaseAddr(static_cast<Hardware>(phyPos))));
    uint64_t tensorAbsPos = tensorAddr - hardwareBaseAddr;
    ASCENDC_CHECK_AND_LOG(((tensorAbsPos % alignBytes) == 0), {CHECK_LOG_ERROR("Failed to check %s tensor address "
        "alignment in %s, current tensor address is %lu, which should be %lu byte aligned.", tensorInfo.c_str(),
        apiName.c_str(), tensorAbsPos, alignBytes);});
    return true;
}

}
}