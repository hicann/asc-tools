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
 * \file kernel_vec_gather_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_gather_check.h"

namespace AscendC {
namespace check {

bool TikcppVecGatherCheck::CommonCheck()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.srcLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.offsetLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "offset", supportPos));
#if defined (__NPU_ARCH__) && __NPU_ARCH__ == 2002
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, param_.dstDtypeBytes, "dst")); // 200: dtype align
#else
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst"));  // 220: 32B aligned
#endif
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.offsetAddr, param_.offsetPos, ONE_BLK_SIZE, "offset"));

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.srcSize, GlobalParams::Instance().bufferSizeMap.at(param_.srcPos),
        "check src tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.offsetSize,
        GlobalParams::Instance().bufferSizeMap.at(param_.offsetPos), "check offset tensor buffersize failed"));
    // srcLocal tensor size is unknown. Only check srcBaseAddr will not exceed srcLocal size
    ASCENDC_CHECK_AND_LOG(param_.srcBaseAddr <= param_.srcSize, {CHECK_LOG_ERROR("Failed to check srcBaseAddr value in "
        "%s, its valid range is 0 ~ %lu, current value is %u", apiName.c_str(), param_.srcSize, param_.srcBaseAddr);});
    return true;
}

bool TikcppVecGatherCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(std::max(param_.dstDtypeBytes, param_.srcDtypeBytes), param_.offsetDtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));
    ASCENDC_CHECK(CommonCheck());

    TensorOverflowParams params = {param_.dstSize, param_.dstDtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.dstBlockStride), static_cast<uint64_t>(param_.dstRepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "dstLocal"));
    // gather: srcBlkStride, srcRepStride must be 1
    params = {param_.srcSize, param_.srcDtypeBytes, static_cast<uint64_t>(param_.repeatTimes), 1, 1, false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcOffsetLocal"));
    return true;
}

bool TikcppVecGatherCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CommonCheck());
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.calCount, "dstLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.offsetDtypeBytes, param_.offsetSize, param_.calCount,
        "srcOffsetLocal"));
    return true;
}

}
}