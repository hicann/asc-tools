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
 * \file kernel_vec_reduce_other_whl_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "model/model_factory_mask.h"
#include "kernel_vec_reduce_other_whl_check.h"

namespace AscendC {
namespace check {

bool TikcppVecReduceOtherWhlCheck::CheckWholeReduceDtypeBytes(const std::string &errMsg)
{
    uint32_t dstDtypeBytes = params_.dstDtypeBytes;
    uint32_t srcDtypeBytes = params_.src0DtypeBytes;
    if (dstDtypeBytes != srcDtypeBytes) {
        CHECK_LOG_ERROR("%s, ""Reduce need dst data type (%u),dst src type (%u), should be same",
            errMsg.c_str(), dstDtypeBytes, srcDtypeBytes);
        return false;
    }
    return true;
}

bool TikcppVecReduceOtherWhlCheck::CheckAddrAlign()
{
    uint8_t alignByte = 4; // float type align Bytes is 4B
    if (params_.dstDtypeBytes == sizeof(half)) {
        alignByte = 2; // half type align Bytes is 2B
    }
    return CheckTensorAddrAlign(params_.dstAddr, params_.dstPos, alignByte, "dst");
}

static bool CheckTensorWhlOverflowLowCounter(std::vector<uint64_t>& maskArray, const VecReduceWhlApiParams& param,
    const uint64_t unit, const std::string& tensorName, const std::string& apiName)
{
    uint32_t oneRepeatNum = ONE_REPEAT_BYTE_SIZE / param.dstDtypeBytes;        // when counter mode, always full mask
    uint64_t elementNum = (maskArray.size() == 1) ? maskArray[0] : maskArray[1]; // maskLow means element num
    int32_t repeatTimes = (elementNum + oneRepeatNum - 1) / oneRepeatNum;
    uint32_t needSize = (repeatTimes - 1) * param.dstRepeatStride * unit + unit;
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param.dstSize, tensorName, apiName, ModeType::COUNTER_MODE));
    return true;
}

static bool CheckTensorWhlOverflowLowNorm(const VecReduceWhlApiParams& param, const uint64_t unit,
    const std::string& tensorName, const std::string& apiName)
{
    uint32_t needSize = (param.repeatTimes - 1) * param.dstRepeatStride * unit;
    if (param.order == ReduceOrder::ORDER_VALUE_INDEX || param.order == ReduceOrder::ORDER_INDEX_VALUE) {
        needSize = needSize + param.dstDtypeBytes + sizeof(uint32_t);  // the DtypeBytes of index
    } else if (param.order == ReduceOrder::ORDER_ONLY_VALUE) {
        needSize = needSize + param.dstDtypeBytes;
    } else if (param.order == ReduceOrder::ORDER_ONLY_INDEX) {
        needSize = needSize + sizeof(uint32_t);
    }
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param.dstSize, tensorName, apiName, ModeType::NORM_MODE));
    return true;
}

// the unit of dstRepStride is Byte
bool TikcppVecReduceOtherWhlCheck::CheckTensorWhlOverflowLow(std::vector<uint64_t>& maskArray,
    const uint64_t unit, const std::string& tensorName)
{
    if (ModelFactoryGetMaskMode() == 1) { // counter mode
        return CheckTensorWhlOverflowLowCounter(maskArray, params_, unit, tensorName, apiName);
    }
    return CheckTensorWhlOverflowLowNorm(params_, unit, tensorName, apiName);
}

bool TikcppVecReduceOtherWhlCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(params_.dstDtypeBytes, params_.src0DtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));

    if ((apiName == "WholeReduceMax") || (apiName == "WholeReduceMin")) {
        ASCENDC_CHECK(CheckWholeReduceDtypeBytes("Check Whole Reduce data type"));
        if (params_.order == ReduceOrder::ORDER_VALUE_INDEX || params_.order == ReduceOrder::ORDER_INDEX_VALUE) {
            constexpr uint32_t MULTIPLIE = 2; // The unit of dstRepStride is twice the length of bytes
            ASCENDC_CHECK(CheckTensorWhlOverflowLow(maskArray, MULTIPLIE * params_.dstDtypeBytes, "dstLocal"));
        } else if (params_.order == ReduceOrder::ORDER_ONLY_VALUE) {
            ASCENDC_CHECK(CheckTensorWhlOverflowLow(maskArray, params_.dstDtypeBytes, "dstLocal"));
        } else if (params_.order == ReduceOrder::ORDER_ONLY_INDEX) {
            ASCENDC_CHECK(CheckTensorWhlOverflowLow(maskArray, sizeof(uint32_t), "dstLocal"));
        }
    }

    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(params_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(params_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));

    ASCENDC_CHECK(CheckAddrAlign());

    ASCENDC_CHECK(CheckBufferSizeOverFlow(params_.dstSize, GlobalParams::Instance().bufferSizeMap.at(params_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(params_.src0Size, GlobalParams::Instance().bufferSizeMap.at(params_.src0Pos),
        "check src tensor buffersize failed"));

    TensorOverflowParams params = {params_.src0Size, params_.src0DtypeBytes, static_cast<uint64_t>(params_.repeatTimes),
        static_cast<uint64_t>(params_.src0BlockStride), static_cast<uint64_t>(params_.src0RepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcLocal"));
    return true;
}
}  // namespace check
}  // namespace AscendC