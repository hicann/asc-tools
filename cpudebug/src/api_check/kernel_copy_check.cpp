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
 * \file kernel_copy_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_copy_check.h"

namespace AscendC {
namespace check {

bool TikcppCopyCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst",
        "VECIN / VECOUT / VECCALC"));
    ASCENDC_CHECK(CheckTensorScope(param_.srcLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src",
        "VECIN / VECOUT / VECCALC"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.srcAddr, param_.srcPos, ONE_BLK_SIZE, "src"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.srcSize, GlobalParams::Instance().bufferSizeMap.at(param_.srcPos),
        "check src tensor buffersize failed"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.calCount, "dstLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.srcDtypeBytes, param_.srcSize, param_.calCount, "srcLocal"));
    return true;
}

bool TikcppCopyCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = std::max(param_.dstDtypeBytes, param_.srcDtypeBytes);
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));

    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.srcLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));

    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.srcAddr, param_.srcPos, ONE_BLK_SIZE, "src"));

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.srcSize, GlobalParams::Instance().bufferSizeMap.at(param_.srcPos),
        "check src tensor buffersize failed"));

    TensorOverflowParams params = {param_.dstSize, static_cast<uint64_t>(param_.dstDtypeBytes),
        static_cast<uint64_t>(param_.repeatTimes), static_cast<uint64_t>(param_.dstStride),
        static_cast<uint64_t>(param_.dstRepeatSize), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params,"dstLocal"));
    params = {param_.srcSize, static_cast<uint64_t>(param_.srcDtypeBytes), static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.srcStride), static_cast<uint64_t>(param_.srcRepeatSize), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "srcLocal"));

    return true;
}

}
}
