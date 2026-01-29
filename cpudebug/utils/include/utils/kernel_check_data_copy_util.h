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
 * \file kernel_check_data_copy_util.h
 * \brief
 */

#ifndef ASCENDC_CHECK_DATA_COPY_UTIL_H
#define ASCENDC_CHECK_DATA_COPY_UTIL_H
#if ASCENDC_CPU_DEBUG
#include <string>
#include "kernel_utils.h"
namespace AscendC {
namespace check {
struct CopyApiParams {
    CopyApiParams() {}
    CopyApiParams(uint64_t dstAddrIn, uint64_t srcAddrIn, uint8_t repeatIn, uint16_t dstStrideIn, uint16_t srcStrideIn,
        uint16_t dstRepeatSizeIn, uint16_t srcRepeatSizeIn, uint32_t dstDtypeBytesIn, uint32_t srcDtypeBytesIn,
        uint64_t dstSizeIn, uint64_t srcSizeIn, uint8_t dstPosIn, uint8_t srcPosIn)
    {
        dstAddr = dstAddrIn;
        srcAddr = srcAddrIn;
        repeatTimes = repeatIn;
        dstStride = dstStrideIn;
        srcStride = srcStrideIn;
        dstRepeatSize = dstRepeatSizeIn;
        srcRepeatSize = srcRepeatSizeIn;
        dstDtypeBytes = dstDtypeBytesIn;
        srcDtypeBytes = srcDtypeBytesIn;
        dstSize = dstSizeIn;
        srcSize = srcSizeIn;
        dstLogicPos = dstPosIn;
        srcLogicPos = srcPosIn;
        dstPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(dstPosIn)));
        srcPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(srcPosIn)));
    }
    CopyApiParams(uint64_t dstAddrIn, uint64_t srcAddrIn, uint32_t dstDtypeBytesIn, uint32_t srcDtypeBytesIn,
        uint64_t dstSizeIn, uint64_t srcSizeIn, uint8_t dstPosIn, uint8_t srcPosIn, uint32_t calCountIn)
    {
        dstAddr = dstAddrIn;
        srcAddr = srcAddrIn;
        dstDtypeBytes = dstDtypeBytesIn;
        srcDtypeBytes = srcDtypeBytesIn;
        dstSize = dstSizeIn;
        srcSize = srcSizeIn;
        dstLogicPos = dstPosIn;
        srcLogicPos = srcPosIn;
        dstPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(dstPosIn)));
        srcPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(srcPosIn)));
        calCount = calCountIn;
    }

    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint8_t repeatTimes = 0;
    uint16_t dstStride = 0;
    uint16_t srcStride = 0;
    uint16_t dstRepeatSize = 0;
    uint16_t srcRepeatSize = 0;
    uint32_t dstDtypeBytes = 0;
    uint32_t srcDtypeBytes = 0;
    uint64_t dstSize = 0;
    uint64_t srcSize = 0;
    uint8_t dstLogicPos = 0;
    uint8_t srcLogicPos = 0;
    uint8_t dstPos = 0;
    uint8_t srcPos = 0;
    uint32_t calCount = 0;
};

struct DataCopyBaseParams {
    DataCopyBaseParams() {}
    DataCopyBaseParams(uint64_t dstAddrIn, uint64_t srcAddrIn, uint32_t dstDtypeBytesIn, uint32_t srcDtypeBytesIn,
        uint8_t dstPosIn, uint8_t srcPosIn, uint16_t blockCountIn, uint16_t blockLenIn, uint16_t srcStrideIn,
        uint16_t dstStrideIn)
    {
        dstAddr = dstAddrIn;
        srcAddr = srcAddrIn;
        dstDtypeBytes = dstDtypeBytesIn;
        srcDtypeBytes = srcDtypeBytesIn;
        dstLogicPos = dstPosIn;
        srcLogicPos = srcPosIn;
        dstPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(dstPosIn)));
        srcPos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(srcPosIn)));
        blockCount = blockCountIn;
        blockLen = blockLenIn;
        srcStride = srcStrideIn;
        dstStride = dstStrideIn;
    }

    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint32_t dstDtypeBytes = 0;
    uint32_t srcDtypeBytes = 0;
    uint8_t dstLogicPos = 0;
    uint8_t srcLogicPos = 0;
    uint8_t dstPos = 0;
    uint8_t srcPos = 0;
    uint16_t blockCount = 0;
    uint16_t blockLen = 0;
    uint16_t srcStride = 0;
    uint16_t dstStride = 0;
};

struct DataCopyApiParams : public DataCopyBaseParams {
    DataCopyApiParams() : DataCopyBaseParams() {}
    DataCopyApiParams(uint64_t dstAddrIn, uint64_t srcAddrIn, uint32_t dstDtypeBytesIn, uint32_t srcDtypeBytesIn,
        uint8_t dstPosIn, uint8_t srcPosIn, uint16_t blockCountIn, uint16_t blockLenIn, uint16_t srcStrideIn,
        uint16_t dstStrideIn)
        : DataCopyBaseParams(dstAddrIn, srcAddrIn, dstDtypeBytesIn, srcDtypeBytesIn,
                            dstPosIn, srcPosIn, blockCountIn, blockLenIn, srcStrideIn, dstStrideIn) {}
};

struct DataCopyPadApiParams : public DataCopyBaseParams {
    DataCopyPadApiParams() : DataCopyBaseParams(), isPad(false), leftPadding(0), rightPadding(0), paddingValue(0) {}
    DataCopyPadApiParams(uint64_t dstAddrIn, uint64_t srcAddrIn, uint32_t dstDtypeBytesIn, uint32_t srcDtypeBytesIn,
        uint8_t dstPosIn, uint8_t srcPosIn, uint16_t blockCountIn, uint16_t blockLenIn, uint16_t srcStrideIn,
        uint16_t dstStrideIn, bool isPadIn, uint8_t leftPaddingIn, uint8_t rightPaddingIn, uint64_t paddingValueIn)
        : DataCopyBaseParams(dstAddrIn, srcAddrIn, dstDtypeBytesIn, srcDtypeBytesIn,
                            dstPosIn, srcPosIn, blockCountIn, blockLenIn, srcStrideIn, dstStrideIn),
          isPad(isPadIn), leftPadding(leftPaddingIn), rightPadding(rightPaddingIn), paddingValue(paddingValueIn) {}

    bool isPad = false;
    uint8_t leftPadding = 0;
    uint8_t rightPadding = 0;
    uint64_t paddingValue = 0;
};

struct DataCopySliceApiParams {
    DataCopySliceApiParams() {}
    DataCopySliceApiParams(uint64_t dstAddrIn, uint64_t srcAddrIn, uint32_t dstDtypeBytesIn, uint32_t srcDtypeBytesIn,
        uint64_t sizeIn, uint8_t posIn, uint32_t dimValueIn, uint32_t shapeDstIn[], uint32_t shapeSrcIn[],
        const SliceInfo dstSliceInfoIn[], const SliceInfo srcSliceInfoIn[], bool isGM2UBIn)
    {
        dstAddr = dstAddrIn;
        srcAddr = srcAddrIn;
        dstDtypeBytes = dstDtypeBytesIn;
        srcDtypeBytes = srcDtypeBytesIn;
        sizeNum = sizeIn;
        logicPos = posIn;
        pos = static_cast<uint8_t>(GetPhyType(static_cast<TPosition>(posIn)));
        dimValue = dimValueIn;
        isGM2UB = isGM2UBIn;
        for (uint32_t i = 0; i < dimValueIn; i++) {
            srcShape[i] = shapeSrcIn[i];
            dstShape[i] = shapeDstIn[i];
            dstSliceInfo[i] = dstSliceInfoIn[i];
            srcSliceInfo[i] = srcSliceInfoIn[i];
        }
    }

    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint32_t dstDtypeBytes = 0;
    uint32_t srcDtypeBytes = 0;
    uint64_t sizeNum = 0;
    uint8_t pos = 0;
    uint8_t logicPos = 0;
    uint32_t dimValue = 0;
    bool isGM2UB = false;
    uint32_t srcShape[K_MAX_SHAPE_DIM];
    uint32_t dstShape[K_MAX_SHAPE_DIM];
    SliceInfo dstSliceInfo[K_MAX_SHAPE_DIM];
    SliceInfo srcSliceInfo[K_MAX_SHAPE_DIM];
};

template <typename T, typename U>
inline bool IsConv(DeqScale deqScale) {
    (void)(deqScale);
    return false;
}

template <>
inline bool IsConv<int32_t, half>(DeqScale deqScale) {
    return (deqScale == DeqScale::DEQ || deqScale == DeqScale::DEQ16 ||
        deqScale == DeqScale::VDEQ || deqScale == DeqScale::VDEQ16);
}

template <>
inline bool IsConv<float, half>(DeqScale deqScale) {
    (void)(deqScale);
    return true;
}

template <>
inline bool IsConv<half, half>(DeqScale deqScale) {
    (void)(deqScale);
    return false;
}

template <>
inline bool IsConv<int32_t, int8_t>(DeqScale deqScale) {
    return (deqScale == DeqScale::DEQ8 || deqScale == DeqScale::VDEQ8);
}

template <>
inline bool IsConv<int32_t, uint8_t>(DeqScale deqScale) {
    return (deqScale == DeqScale::DEQ8 || deqScale == DeqScale::VDEQ8);
}

template <>
inline bool IsConv<int32_t, int16_t>(DeqScale deqScale) {
    return (deqScale == DeqScale::DEQ16 || deqScale == DeqScale::VDEQ16);
}

template <typename T>
inline std::string GetSrcIDString(Hardware srcScope, BlockMode blockMode)
{
    std::string srcIDString = "";
    if ((srcScope == Hardware::UB) || (srcScope == Hardware::L1) || (srcScope == Hardware::GM)) {
        srcIDString += ConstDefiner::Instance().hardwareMap.at(srcScope);
    } else if ((std::is_same<T, float>::value || std::is_same<T, half>::value) &&
       (blockMode == BlockMode::BLOCK_MODE_DEPTHWISE)) {
        srcIDString += ConstDefiner::Instance().hardwareMap.at(srcScope) +
            ConstDefiner::Instance().blockModeMap.at(blockMode) + "f" +
            std::to_string(sizeof(T) * ONE_BYTE_BIT_SIZE);
    } else {
        srcIDString += ConstDefiner::Instance().hardwareMap.at(srcScope) +
            ConstDefiner::Instance().blockModeMap.at(blockMode) +
            std::to_string(sizeof(T) * ONE_BYTE_BIT_SIZE);
    }
    return srcIDString;
}

template <typename T>
inline std::string GetDstIDString(Hardware dstScope, BlockMode blockMode)
{
    std::string dstIDString = "";
#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||                       \
    (__NPU_ARCH__ == 3102) || (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102) ||                       \
	(__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) || (__NPU_ARCH__ == 3113))
    if ((dstScope == Hardware::UB) || (dstScope == Hardware::L1) || (dstScope == Hardware::GM) ||
        (dstScope == Hardware::BIAS) || (dstScope == Hardware::FIXBUF)) {
#else
    if ((dstScope == Hardware::UB) || (dstScope == Hardware::L1) || (dstScope == Hardware::GM)) {
#endif
        dstIDString += ConstDefiner::Instance().hardwareMap.at(dstScope);
    } else {
        dstIDString += ConstDefiner::Instance().hardwareMap.at(dstScope) +
            ConstDefiner::Instance().blockModeMap.at(blockMode) +
            std::to_string(sizeof(T) * ONE_BYTE_BIT_SIZE);
    }
    return dstIDString;
}

// get unit of dst src busrt length, unit is byte
inline uint16_t GetBurstLenUnit(std::string srcDstId, bool isConv, bool isSrc)
{
    uint16_t burstLenUnit = 0;
    auto burstLenUnitMap = isSrc ? ConstDefiner::Instance().srcBurstLenUnitMap :
        ConstDefiner::Instance().dstBurstLenUnitMap;
    if (burstLenUnitMap.find(srcDstId) != burstLenUnitMap.end()) {
        burstLenUnit = burstLenUnitMap.at(srcDstId);
        if (isConv) {
            burstLenUnit /= HALF_FACTOR;
        }
    } else {
        burstLenUnit = DEFAULT_C0_SIZE;
    }
    return burstLenUnit;
}

// get unit of dst src stride, unit is byte
inline uint16_t GetStrideUnit(std::string srcDstId, bool isSrc)
{
    auto strideUnitMap = isSrc ? ConstDefiner::Instance().srcStrideUnitMap :
        ConstDefiner::Instance().dstStrideUnitMap;
    if (strideUnitMap.find(srcDstId) != strideUnitMap.end()) {
        return strideUnitMap.at(srcDstId);
    }
    return DEFAULT_C0_SIZE;
}

bool CheckFuncCopyImplForMaskArray(CopyApiParams& chkParams, const uint64_t mask[], const char* intriName);
bool CheckFuncCopyImpl(CopyApiParams& chkParams, const uint64_t mask, const char* intriName);
bool CheckFuncCopyImpl(CopyApiParams& chkParams, const char* intriName);

bool CheckFuncDataCopyImpl(DataCopyApiParams& chkParams, const char* intriName);
bool CheckFuncDataCopyPadImpl(DataCopyPadApiParams& chkParams, const char* intriName);
bool CheckFuncDataCopySliceImpl(DataCopySliceApiParams &chkParams, const char* intriName);
} // namespace check
} // namespace AscendC
#endif
#endif