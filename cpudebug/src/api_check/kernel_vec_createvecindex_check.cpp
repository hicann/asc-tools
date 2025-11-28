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
 * \file kernel_vec_createvecindex_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_createvecindex_check.h"

namespace AscendC {
namespace check {

bool TikcppVecCreateVecIndexCheck::CommonCheck()
{
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dstLocal",
        "VECIN / VECOUT / VECCALC"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "Failed to check dstLocal tensor buffersize in CreateVecIndex"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst"));
    return true;
}

bool TikcppVecCreateVecIndexCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    uint32_t maxByteLen = param_.dstDtypeBytes;
    ASCENDC_CHECK(UpdateMaskArrayAndCheck(maskArray, maxByteLen));
    ASCENDC_CHECK(CommonCheck());
    TensorOverflowParams params = {param_.dstSize, param_.dstDtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.dstBlkStride), static_cast<uint64_t>(param_.dstRepStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "dstLocal"));
    return true;
}

bool TikcppVecCreateVecIndexCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CommonCheck());
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, param_.calCount, "dstLocal"));
    return true;
}

}
}