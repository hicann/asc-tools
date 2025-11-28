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
 * \file kernel_base_check.h
 * \brief
 */

#ifndef ASCENDC_BASE_CHECK_H
#define ASCENDC_BASE_CHECK_H

#include <vector>
#include <string>
#include "kernel_check_util.h"

namespace AscendC {
namespace check {
const uint32_t MASK_ARRAY_LEN = 2;
const uint32_t NAME_MAX_LEN = 128;

enum class ModeType : uint8_t {
    NONE_MODE = 0,
    NORM_MODE = 1,
    COUNTER_MODE = 2
};

struct TensorOverflowParams {
    TensorOverflowParams() = default;

    TensorOverflowParams(const uint64_t bufferSizeIn, const uint64_t dtypeSizeIn, const uint64_t repeatTimesIn,
        const uint64_t blkStrideIn, const uint64_t repStrideIn, const bool isCounterIn) : bufferSize(bufferSizeIn),
        dtypeSize(dtypeSizeIn), repeatTimes(repeatTimesIn), blkStride(blkStrideIn), repStride(repStrideIn),
        isCounter(isCounterIn)
    {}

    uint64_t bufferSize = 0;
    uint64_t dtypeSize = 1;
    uint64_t repeatTimes = 1;
    uint64_t blkStride = DEFAULT_BLK_STRIDE;
    uint64_t repStride = DEFAULT_REPEAT_STRIDE;
    bool isCounter = false;     // only used for gathermask
};

/*
* @funcname: GetMaskLength
* @brief: the maximum number of elements calculated this repeat.
*         For example mask is 0x30fff + 0xffffffff. The maximum num per repeat is 64 + 18 = 82
* @return: uint64_t
*/
uint64_t GetMaskLength(std::vector<uint64_t> &maskArray, const uint32_t dtypeSize);

/*
* @funcname: CheckTensorSizeOverflow
* @brief: common method used to compare tensor size and minimum needed size, and report error if overflow
*         mode 0: none, mode 1: norm mode, mode 2: counter mode
* @return: true/false
*/
bool CheckTensorSizeOverflow(uint64_t expectedSize, uint64_t tensorSize, const std::string& tensorName,
    const std::string& apiName, const ModeType mode = ModeType::NONE_MODE);

void CounterSplitMainTail(std::vector<uint64_t>& maskArray, const uint32_t dtypeBytes, uint64_t& mainRepeatTimes,
    uint64_t& tailRepeatTimes, std::vector<uint64_t>& mainMaskArray, std::vector<uint64_t>& tailMaskArray);

uint64_t CalculateVectorMaxOffset(const uint64_t repeatTimes, const uint64_t blkStride, const uint64_t repStride,
    const uint64_t maskLen, const uint64_t blockLen);

class TikcppBaseCheck {
public:
    explicit TikcppBaseCheck(const std::string& name) : apiName(name) {}
    virtual ~TikcppBaseCheck() {}

    /*
    * @funcname: CheckTensorScope
    * @brief: check whether the tensor hardware position equal to the expect position.
    * @params: tensorPos, tensor physical position
    *          expectedPos, expected tensor physical position
    *          tensorInfo, src0 / src1/ dst for print information
    *          posInfo, expected position info for print information
    * @return: true/false
    */
    bool CheckTensorScope(const uint8_t logicPos, const uint8_t expectedPos, const std::string& tensorInfo,
        const std::string& posInfo) const;

    /*
    * @funcname: CheckBufferSizeOverFlow
    * @brief: check whether the tensor allocate size equal to the buffer limited size.
    * @return: true/false
    */
    bool CheckBufferSizeOverFlow(const uint64_t localSize, const uint64_t bufferSize, const std::string& errMsg) const;

    /*
    * @funcname: CheckMaskArray
    * @brief: check the mask in bits mode
    * @return: true/false
    */
    bool CheckMaskArray(std::vector<uint64_t> maskArray) const;

    /*
    * @funcname: CheckMaskImm
    * @brief: check the mask in continuous mode
    * @return: true/false
    */
    bool CheckMaskImm(const uint64_t mask) const;

    /*
    * @funcname: CheckTensorOverflowLow
    * @brief: check whether tensor used size over the allocated size in low api level
    * @return: true/false
    */
    bool CheckTensorOverflowLow(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
        const std::string& tensorName) const;

    /*
    * @funcname: CheckTensorOverflowLowGatherMask
    * @brief: check whether tensor used size over the allocated size in low api level for GatherMask
    * @return: true/false
    */
    bool CheckTensorOverflowLowGathermask(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
        const std::string& tensorName) const;

    /*
    * @funcname: CheckTensorOverflowLowBrcb
    * @brief: check whether tensor used size over the allocated size in low api level for Brcb
    *         Note that there is no counter mode for brcb + mask is not used
    * @return: true/false
    */
    bool CheckTensorOverflowLowBrcb(const TensorOverflowParams& params, const std::string& tensorName) const;

    /*
    * @funcname: CheckTensorOverflowHigh
    * @brief: check whether tensor used size over the allocated size in high api level
    * @return: true/false
    */
    bool CheckTensorOverflowHigh(const uint32_t dtypeSize, const uint64_t bufferSize, const uint32_t calCount,
        const std::string& tensorName) const;

    /*
    * @funcname: UpdateMaskArrayAndCheck
    * @brief: If isSetMask = false, replace maskArray with maskHigh and maskLow value in registers.
    *         Check the latest maskArray value is valid with given dtype
    * @params: maskArray, mask value given by user. Can be len 1 or len 2
    *          maxByteLen, among all dtypes given by function, the largest value of sizeof(dtype)
    * @return: true/false
    */
    bool UpdateMaskArrayAndCheck(std::vector<uint64_t>& maskArray, const uint32_t maxByteLen) const;

    /*
    * @funcname: CheckTensorAddrAlign
    * @brief: Check tensor start address is aligned with alignBytes
    * @params: tensorAddr, tensor address
    *          phyPos, tensor physical position, used to calculate real offset of tensor
    *          alignBytes, 32B aligned / 512B aligned etc
    *          tensorInfo, src0 / src1/ dst for print information
    * @return: true/false
    */
    bool CheckTensorAddrAlign(const uint64_t tensorAddr, const uint8_t phyPos, const uint64_t alignBytes,
        const std::string& tensorInfo) const;
protected:
    std::string apiName = "";
};

} // namespace check
} // namespace AscendC
#endif