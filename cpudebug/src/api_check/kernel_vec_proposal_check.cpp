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
 * \file kernel_vec_proposal_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_proposal_check.h"

namespace AscendC {
namespace check {
const uint32_t ONE_REPEAT_CAL_NUM = 16;   // 1 repeat = 16 region proposals
const uint32_t PROPOSAL_SIZE = 8;         // 1 proposal = 8 element

bool TikcppVecProposalCheck::CheckAddrAlign(const std::string& src0Name)
{
    uint8_t alignByte = ONE_BLK_SIZE;
    bool dstRes = true;
    bool src0Res = true;
    if (apiName == "MrgSort") {
        alignByte = 8; // half and float type align Bytes is 8B
        dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst");
        src0Res = CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, alignByte, src0Name);
        return dstRes && src0Res;
    }
    if (apiName == "MrgSort4" && param_.dstDtypeBytes == sizeof(half)) {
        alignByte = 16; // half type align Bytes is 16B
    }
    dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, alignByte, "dst");
    src0Res = CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, alignByte, src0Name);
    return dstRes && src0Res;
}

// validBit to num of proposal lists: 3->2, 7->3, 15->4
uint8_t TikcppVecProposalCheck::CountBit(uint16_t validBit) const
{
    uint8_t count = 0;
    while (validBit != 0) {
        count += (validBit & 0x1);
        validBit >>= 1;
    }
    return count;
}

bool TikcppVecProposalCheck::CheckValidBit(uint16_t validBit) const
{
    bool validBitRes = validBit == 3 || validBit == 7 || validBit == 15;
    ASCENDC_CHECK_AND_LOG((validBitRes), {CHECK_LOG_ERROR("Failed to check validBit value in %s, its valid value is "
        "[3, 7, 15], current value is %u.", apiName.c_str(), validBit);});
    return true;
}

// calculate total elements that needs to be sorted per repeatTimes. (1 proposal -> 1 element)
uint64_t TikcppVecProposalCheck::CalSortElemPerRep(uint16_t elementLengths[4], uint8_t count) const
{
    uint64_t elePerRep = 0;
    for (uint8_t i = 0; i < count; ++i) {
        elePerRep += elementLengths[i];
    }
    return elePerRep;
}

bool TikcppVecProposalCheck::NeedRepeatTimes() const
{
    // 1. 4 region proposals has same lengths
    bool cond1 = (param_.elementLengths[0] == param_.elementLengths[1]) &&
        (param_.elementLengths[1] == param_.elementLengths[2]) &&
        (param_.elementLengths[2] == param_.elementLengths[3]);
    // 2. continuous stored  3. ifExhaused = false 4. validBit = 15
    return cond1 && param_.isContinuous && (!param_.isExhausted) && (param_.validBit == 15);
}

bool TikcppVecProposalCheck::Vbs16Check() const
{
    // 1 repeat = 16 proposals, 1 proposals = 8 * element. Data are continuously stored.
    uint64_t calCount = param_.repeatTimes * ONE_REPEAT_CAL_NUM * PROPOSAL_SIZE;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, calCount, "dstLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, calCount, "srcLocal"));
    return true;
}

bool TikcppVecProposalCheck::Vbs32Check() const
{
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src1Size, GlobalParams::Instance().bufferSizeMap.at(param_.src1Pos),
        "check src1 tensor buffersize failed"));

    if (param_.dstDtypeBytes == 0) {
        CHECK_LOG_ERROR("dst dtype bytes is zero");
        return false;
    }
    // In 1 repeat, dst: 256B   src0: 32 * element   src1: 32 element
    const uint32_t oneCalNumVbs32 = 32;   // 1 repeat calculates 32 groups of (score + index)
    uint32_t elemPerRepeat = ONE_REPEAT_BYTE_SIZE / param_.dstDtypeBytes;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, elemPerRepeat * param_.repeatTimes,
        "dstLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, oneCalNumVbs32 * param_.repeatTimes,
        "src0Local"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src1DtypeBytes, param_.src1Size, oneCalNumVbs32 * param_.repeatTimes,
        "src1Local"));
    return true;
}

bool TikcppVecProposalCheck::Vms4Check() const
{
    ASCENDC_CHECK(CheckValidBit(param_.validBit));
    uint8_t count = CountBit(param_.validBit);
    if (param_.srcIndex >= count) {
        return true; // if current list index is large than valid list number, no need to check
    }

    uint64_t sortElePerRep = CalSortElemPerRep(param_.elementLengths, count);
    uint64_t elemPerRep = sortElePerRep * PROPOSAL_SIZE;     // 1 sort element = 1 proposal = 8 elements
    uint64_t validRepeatTimes = 1;
    if (NeedRepeatTimes()) {
        validRepeatTimes = param_.repeatTimes;
        ASCENDC_CHECK_VALUE_RANGE(validRepeatTimes, 1, MAX_REPEAT_TIMES, "repeatTimes", "MrgSort4");
    }
    // if exhausted, do not know total size. Thus no check
    if (!param_.isExhausted) {
        ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, elemPerRep * param_.repeatTimes,
            "dstLocal"));
    }
    std::string tensorName = "src" + std::to_string(param_.srcIndex) + " in srcLocal";
    uint64_t srcEle = (validRepeatTimes - 1) * elemPerRep + param_.elementLengths[param_.srcIndex] * PROPOSAL_SIZE;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, srcEle, tensorName));
    return true;
}

bool TikcppVecProposalCheck::Vms4v2Check() const
{
    ASCENDC_CHECK(CheckValidBit(param_.validBit));
    uint8_t count = CountBit(param_.validBit);
    if (param_.srcIndex >= count) {
        return true; // if current list index is large than valid list number, no need to check
    }

    uint64_t sortElePerRep = CalSortElemPerRep(param_.elementLengths, count);
    uint64_t bytePerRep = sortElePerRep * PROPOSAL_SIZE;                         // 1 sorted element = 8 Byte
    uint64_t validRepeatTimes = 1;
    if (NeedRepeatTimes()) {
        validRepeatTimes = param_.repeatTimes;
        ASCENDC_CHECK_VALUE_RANGE(validRepeatTimes, 1, MAX_REPEAT_TIMES, "repeatTimes", "MrgSort");
    }
    // if exhausted, do not know total size. Thus no check
    if (!param_.isExhausted) {
        ASCENDC_CHECK(CheckTensorOverflowHigh(1, param_.dstSize, bytePerRep * param_.repeatTimes, "dstLocal"));
    }
    std::string tensorName = "src" + std::to_string(param_.srcIndex) + " in srcLocal";
    uint64_t srcBytes = (validRepeatTimes - 1) * bytePerRep + param_.elementLengths[param_.srcIndex] * PROPOSAL_SIZE;
    // calcount is set as Bytes, thus set sizeof(dtype) to 1
    ASCENDC_CHECK(CheckTensorOverflowHigh(1, param_.src0Size, srcBytes, tensorName));
    return true;
}

bool TikcppVecProposalCheck::VconcatCheck() const
{
    // src: repeat * 16 element, dst: repeat * 16 region proposal (16 * 8 element)   both continuously stored
    uint32_t base = param_.repeatTimes * ONE_REPEAT_CAL_NUM;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, base, "srcLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, base * PROPOSAL_SIZE, "dstLocal"));
    return true;
}

bool TikcppVecProposalCheck::VextractCheck() const
{
    // src: repeat * 16 region proposal (16 * 8 element), dst: repeat * 16 element   both continuously stored
    uint32_t base = param_.repeatTimes * ONE_REPEAT_CAL_NUM;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, base * PROPOSAL_SIZE, "srcLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, base, "dstLocal"));
    return true;
}

bool TikcppVecProposalCheck::ConcatCheck() const
{
    // src: repeat * 16 element        dst: V220 dst = src, V200:  repeat * 16 region proposal (16 * 8 element)
    uint32_t base = param_.repeatTimes * ONE_REPEAT_CAL_NUM;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, base, "srcLocal"));
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
     (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
    // tmpLocal is not used
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, base, "concatLocal"));
#elif defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 1001) || (__NPU_ARCH__ == 2002))
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src1DtypeBytes, param_.src1Size, base * PROPOSAL_SIZE, "tmpLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, base * PROPOSAL_SIZE, "concatLocal"));
#endif
    return true;
}

bool TikcppVecProposalCheck::ExtractCheck() const
{
    // In extract: dst -> dstValueLocal, src1 -> dstIndexLocal, src0 -> sortedLocal
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
     (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
    // 1 repeat:  32 groups of (score + index)   sortedLocal: 256B      dst: 32 elements
    uint64_t groupNumPerRep = 32;   // 1 sort result is 8 Bytes, thus 1 repeat = 32 groups
    uint64_t totalEleNum = groupNumPerRep * param_.repeatTimes;
    ASCENDC_CHECK(CheckTensorSizeOverflow(param_.repeatTimes * ONE_REPEAT_BYTE_SIZE, param_.src0Size, "sortedLocal",
        "Extract"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src1DtypeBytes, param_.src1Size, totalEleNum, "dstIndexLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, totalEleNum, "dstValueLocal"));
#elif defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 1001) || (__NPU_ARCH__ == 2002))
    // 1 repeat:  src: 16 region proposal   dst: 16 elements
    uint32_t base = param_.repeatTimes * ONE_REPEAT_CAL_NUM;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src0DtypeBytes, param_.src0Size, base * PROPOSAL_SIZE, "sortedLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.src1DtypeBytes, param_.src1Size, base, "dstIndexLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, base, "dstValueLocal"));
#endif
    return true;
}

bool TikcppVecProposalCheck::CheckAllHighLevel()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    std::string src0Name = "src0";
    if (apiName == "MrgSort" || apiName == "MrgSort4") {  // only has src1 ~ src4
        src0Name = "src" + std::to_string(param_.srcIndex) + " in srcLocal";
    }
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), src0Name, supportPos));
    if (apiName == "Sort32" || apiName == "Concat" || apiName == "Extract") {
        ASCENDC_CHECK(CheckTensorScope(param_.src1LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src1", supportPos));
        ASCENDC_CHECK(CheckTensorAddrAlign(param_.src1Addr, param_.src1Pos, ONE_BLK_SIZE, "src1"));
    }

    ASCENDC_CHECK(CheckAddrAlign(src0Name));

    std::string bufferSrc0 = "check " + src0Name + " tensor buffersize failed";
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        bufferSrc0));

    if (apiName == "Sort32") {
        return Vbs32Check();
    } else if (apiName == "ProposalConcat") {
        return VconcatCheck();
    } else if (apiName == "Concat") {
        return ConcatCheck();
    } else if (apiName == "ProposalExtract") {
        return VextractCheck();
    } else if (apiName == "Extract") {
        return ExtractCheck();
    } else if (apiName == "RpSort16") {
        return Vbs16Check();
    } else if (apiName == "MrgSort4") {
        return Vms4Check();
    } else if (apiName == "MrgSort") {
        return Vms4v2Check();
    }
    return true;
}
}  // namespace check
}  // namespace AscendC