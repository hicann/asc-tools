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
 * \file kernel_vec_transpose_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_vec_transpose_check.h"

namespace AscendC {
namespace check {
constexpr const uint32_t BLOCK_COUNT_16 = 16;
constexpr const uint32_t BLOCK_COUNT_32 = 8;

bool TikcppVecTransposeCheck::CheckAddrAlign()
{
    bool dstRes = CheckTensorAddrAlign(param_.dstAddr, param_.dstPos, ONE_BLK_SIZE, "dst");
    bool srcRes = CheckTensorAddrAlign(param_.srcAddr, param_.srcPos, ONE_BLK_SIZE, "src");
    return dstRes && srcRes;
}

bool TikcppVecTransposeCheck::CheckTempTensorSizeOverflow() const
{
    uint64_t needSize = 0;
    uint32_t baseSize = 0;
    uint8_t coef = 2;

    if (param_.transposeType == TransposeType::TRANSPOSE_NCHW2NHWC) {
        baseSize = param_.cSize + coef;
    } else if (param_.transposeType == TransposeType::TRANSPOSE_NHWC2NCHW) {
        baseSize = param_.cSize * coef + 1;
    }
    if (param_.srcDtypeBytes == sizeof(uint8_t)) { // int8_t / uint8_t
        needSize = baseSize * ONE_BLK_SIZE * ONE_BLK_SIZE;
    } else if (param_.srcDtypeBytes == sizeof(uint16_t) || param_.srcDtypeBytes == sizeof(uint32_t)) {
        needSize = baseSize * BLOCK_COUNT_16 * ONE_BLK_SIZE;
    }
    ASCENDC_CHECK(CheckTensorSizeOverflow(needSize, param_.tmpBufferSize, "sharedTmpBuffer", "Transpose"));
    return true;
}

bool TikcppVecTransposeCheck::TransdataCheckTensorSize()
{
    if (param_.repeatTimes == 0) {
        return true;
    }
    uint64_t srcExpectedSize = param_.repeatTimes * param_.srcRepeatStride * ONE_BLK_SIZE + ONE_BLK_SIZE;
    uint64_t dstExpectedSize = param_.repeatTimes * param_.dstRepeatStride * ONE_BLK_SIZE + ONE_BLK_SIZE;
    std::string tensorName = (param_.index >= 0) ? std::to_string(param_.index) + "Local" : "Local";
    ASCENDC_CHECK(CheckTensorSizeOverflow(srcExpectedSize, param_.srcSize, "src" + tensorName, "TransDataTo5HD"));
    ASCENDC_CHECK(CheckTensorSizeOverflow(dstExpectedSize, param_.dstSize, "dst" + tensorName, "TransDataTo5HD"));
    return true;
}

bool TikcppVecTransposeCheck::CheckAllLowLevel()
{
    const std::string supportPos = "VECIN / VECOUT / VECCALC";
    ASCENDC_CHECK(CheckTensorScope(param_.dstLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "dst", supportPos));
    ASCENDC_CHECK(CheckTensorScope(param_.srcLogicPos, static_cast<uint8_t>(HardWareIndex::UB), "src", supportPos));
    ASCENDC_CHECK(CheckAddrAlign());
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.dstSize, GlobalParams::Instance().bufferSizeMap.at(param_.dstPos),
        "check dst tensor buffersize failed"));
    ASCENDC_CHECK(CheckBufferSizeOverFlow(param_.srcSize, GlobalParams::Instance().bufferSizeMap.at(param_.srcPos),
        "check src tensor buffersize failed"));
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ != 3003) && (__NPU_ARCH__ != 3113))
    uint64_t expectedSize = 0;
    if (apiName == "Transpose") {
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
     (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
        ASCENDC_CHECK(CheckTempTensorSizeOverflow());
#endif
        TransposeType transType = param_.transposeType;
        ASCENDC_CHECK_AND_LOG((transType == TransposeType::TRANSPOSE_TYPE_NONE || transType == TransposeType::TRANSPOSE_ND2ND_B16 ||
                               transType == TransposeType::TRANSPOSE_NCHW2NHWC || transType == TransposeType::TRANSPOSE_NHWC2NCHW),
                               {CHECK_LOG_ERROR("Failed to check transposeType when it only supports TRANSPOSE_TYPE_NONE, TRANSPOSE_ND2ND_B16, "
                               "TRANSPOSE_NCHW2NHWC, TRANSPOSE_NHWC2NCHW in Transpose, while the current value is %u.", static_cast<uint32_t>(transType));});
        if (transType == TransposeType::TRANSPOSE_ND2ND_B16) {
            ASCENDC_CHECK_AND_LOG((param_.hSize == NCHW_CONV_ADDR_LIST_SIZE), {CHECK_LOG_ERROR("Failed to check hSize "
                "value when transposeType is TRANSPOSE_ND2ND_B16 in Transpose, its valid value is 16, current value "
                "is %u.", param_.hSize);});
            ASCENDC_CHECK_AND_LOG((param_.wSize == NCHW_CONV_ADDR_LIST_SIZE), {CHECK_LOG_ERROR("Failed to check wSize "
                "value when transposeType is TRANSPOSE_ND2ND_B16 in Transpose, its valid value is 16, current value "
                "is %u.", param_.wSize);});
        }

        if (transType == TransposeType::TRANSPOSE_ND2ND_B16) {
            expectedSize = VALUE_512;  // 16 * 16 B16 matrix do transpose -> src and local both 512B
        } else if (transType == TransposeType::TRANSPOSE_NCHW2NHWC || transType == TransposeType::TRANSPOSE_NHWC2NCHW) {
            expectedSize = param_.nSize * param_.cSize * param_.hSize * param_.wSize * param_.srcDtypeBytes; // NCHW
        }
        ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.srcSize, "srcLocal", "Transpose"));
        ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.dstSize, "dstLocal", "Transpose"));
    } else if (apiName == "TransDataTo5HD") {
        if (param_.index != -1) {  // api that directly pass tensors
            ASCENDC_CHECK(TransdataCheckTensorSize());
        } else {                                         // api that passes uint64_t tensor that stores address
            expectedSize = 16 * param_.srcDtypeBytes;    // tensor only needs to store 16 tensor address
            ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.srcSize, "srcLocal", "TransDataTo5HD"));
            ASCENDC_CHECK(CheckTensorSizeOverflow(expectedSize, param_.dstSize, "dstLocal", "TransDataTo5HD"));
        }
    }
#endif
    return true;
}
}
}