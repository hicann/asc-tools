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
 * \file kernel_vec_compare_register_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_compare_register_check.h"

namespace AscendC {
namespace check {
bool TikcppVecCmpRgtCheck::CheckAllLowLevel(std::vector<uint64_t> maskArray)
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src0", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.src1LogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src1", supportPos));

    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src0Size, GlobalParams::Instance().bufferSizeMap.at(param_.src0Pos),
        "check src0 tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.src1Size, GlobalParams::Instance().bufferSizeMap.at(param_.src1Pos),
        "check src1 tensor buffersize failed"));
    ASCENDC_CHECK(CheckAddrAlign());

    TensorOverflowParams params = {param_.src0Size, param_.src0DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src0BlockStride), static_cast<uint64_t>(param_.src0RepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "src0Local"));
    params = {param_.src1Size, param_.src1DtypeBytes, static_cast<uint64_t>(param_.repeatTimes),
        static_cast<uint64_t>(param_.src1BlockStride), static_cast<uint64_t>(param_.src1RepeatStride), false};
    ASCENDC_CHECK(CheckTensorOverflowLow(maskArray, params, "src1Local"));

    return true;
}

bool TikcppVecCmpRgtCheck::CheckAddrAlign()
{
    bool src0Res = CheckTensorAddrAlign(param_.src0Addr, param_.src0Pos, ONE_BLK_SIZE, "src0");
    bool src1Res = CheckTensorAddrAlign(param_.src1Addr, param_.src1Pos, ONE_BLK_SIZE, "src1");
    return src0Res && src1Res;
}
} // namespace check
} // namespace AscendC