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
 * \file kernel_vec_sort_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_sort_check.h"

namespace AscendC {
namespace check {
const uint32_t ONE_REPEAT_CAL_NUM = 16;   // 1 repeat = 16 region proposals
const uint32_t PROPOSAL_SIZE = 8;         // 1 proposal = 8 element
constexpr uint32_t REGION_PROPOSAL_DATA_SIZE_V200 = 8;
constexpr uint32_t REGION_PROPOSAL_DATA_SIZE_HALF_V220 = 4;
constexpr uint32_t REGION_PROPOSAL_DATA_SIZE_FLOAT_V220 = 2;

bool TikcppVecSortCheck::Sort32Check()
{
    ASCENDC_CHECK_AND_LOG(param_.dstDtypeBytes != 0, {CHECK_LOG_ERROR("dstDtypeBytes should not be 0.");});
    // dst: 256 Bytes per repeat
    const uint32_t dstTotalElements = ONE_REPEAT_BYTE_SIZE / param_.dstDtypeBytes * param_.repeatTimes;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, dstTotalElements, "dstLocal"));
    // concat + index: 32 elements per repeat
    const uint32_t totalElements = 32 * param_.repeatTimes;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.concatDtypeBytes, param_.concatSize, totalElements, "concatLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.indexDtypeBytes, param_.indexSize, totalElements, "indexLocal"));

    if (param_.isFullSort) {
        uint64_t coefficient = (param_.tmpDtypeBytes == sizeof(float)) ? REGION_PROPOSAL_DATA_SIZE_FLOAT_V220 :
            REGION_PROPOSAL_DATA_SIZE_HALF_V220;
        uint64_t tmpElements = totalElements * coefficient;
        ASCENDC_CHECK(CheckTensorOverflowHigh(param_.tmpDtypeBytes, param_.tmpSize, tmpElements, "tmpLocal"));
    }
    return true;
}

bool TikcppVecSortCheck::RpSort16Check()
{
    // 1 repeat = 16 proposals, 1 proposals = 8 * element. Data are continuously stored.
    uint64_t calCount = param_.repeatTimes * ONE_REPEAT_CAL_NUM * PROPOSAL_SIZE;
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.dstDtypeBytes, param_.dstSize, calCount, "dstLocal"));
    ASCENDC_CHECK(CheckTensorOverflowHigh(param_.concatDtypeBytes, param_.concatSize, calCount, "concatLocal"));

    if (param_.isFullSort) {
        uint64_t tmpElements = (param_.repeatTimes * ONE_REPEAT_CAL_NUM) * REGION_PROPOSAL_DATA_SIZE_V200;
        ASCENDC_CHECK(CheckTensorOverflowHigh(param_.tmpDtypeBytes, param_.tmpSize, tmpElements, "tmpLocal"));
    }
    return true;
}

bool TikcppVecSortCheck::CheckAllHighLevel()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    const uint8_t ubPos = static_cast<uint8_t>(HardWareIndex::UB);
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, ubPos, "dstLocal", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.concatLogicPos, ubPos, "concatLocal", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.indexLogicPos, ubPos, "indexLocal", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.tmpLogicPos, ubPos, "tmpLocal", supportPos));

    ASCENDC_CHECK(CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dstLocal"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.concatAddr, param_.concatPos, ONE_BLK_SIZE, "concatLocal"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.indexAddr, param_.indexPos, ONE_BLK_SIZE, "indexLocal"));
    ASCENDC_CHECK(CheckTensorAddrAlign(param_.tmpAddr, param_.tmpPos, ONE_BLK_SIZE, "tmpLocal"));

#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
     (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
    ASCENDC_CHECK(Sort32Check());
#elif defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 1001) || (__NPU_ARCH__ == 2002))
    ASCENDC_CHECK(RpSort16Check());
#endif
    return true;
}

}
}