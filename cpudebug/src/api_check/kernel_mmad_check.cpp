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
 * \file kernel_mmad_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_mmad_check.h"

namespace AscendC {
namespace check {
bool TikcppMmadCheck::CheckMmadParamsRange(const uint32_t num, const std::string &paramName) const
{
    const uint32_t mmadParamRange = 4095;   // m, n, k only has 12 bits, thus range [0, 4095]
    ASCENDC_CHECK_AND_LOG((num <= mmadParamRange), {CHECK_LOG_ERROR("Failed to check %s value in %s, its valid range "
        "is 0 ~ 4095, current value is %u.", paramName.c_str(), apiName.c_str(), num);});
    return true;
}

bool TikcppMmadCheck::CheckMmadOverflow(const std::string& errMsg)
{
    uint32_t needElementL0c = static_cast<uint32_t>(param_.m * param_.n * param_.dstDtypeBytes);
    uint32_t needElementL0a = static_cast<uint32_t>(param_.m * param_.k * param_.src0DtypeBytes);
    uint32_t needElementL0b = static_cast<uint32_t>(param_.n * param_.k * param_.src1DtypeBytes);
    uint32_t totalL0CSize = static_cast<uint32_t>(PlatFormParams::L0C_SIZE);
    uint32_t totalL1Size = static_cast<uint32_t>(PlatFormParams::L1_SIZE);
    if (needElementL0c > totalL0CSize) {
        CHECK_LOG_ERROR("%s: ""needElementL0c(%u) is bigger than totalL0CSize(%u)", errMsg.c_str(), needElementL0c,
            totalL0CSize);
        return false;
    }

    if ((needElementL0b + needElementL0a) > totalL1Size) {
        CHECK_LOG_ERROR("%s: ""needElementL0b(%u) + needElementL0a(%u) is bigger than totalL1Size(%u)", errMsg.c_str(),
            needElementL0b, needElementL0a, totalL1Size);
        return false;
    }
    return true;
}

bool TikcppMmadCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::L0C), "dstLocal", "CO1"));
    ASCENDC_CHECK(CheckTensorScope(param_.src0LogicPos, static_cast<uint8_t>(HardWareIndex::L0A), "fmLocal", "A2"));
    ASCENDC_CHECK(CheckTensorScope(param_.src1LogicPos, static_cast<uint8_t>(HardWareIndex::L0B), "filterLocal", "B2"));

    ASCENDC_CHECK(CheckMmadParamsRange(param_.m, "m"));
    ASCENDC_CHECK(CheckMmadParamsRange(param_.n, "n"));
    ASCENDC_CHECK(CheckMmadParamsRange(param_.k, "k"));

    // check Mmad overflow
    ASCENDC_CHECK(CheckMmadOverflow("check mmad overflow failed"));
    return true;
}
}
}
