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
 * \file kernel_struct_mm.h
 * \brief
 */
#ifndef ASCENDC_MODULE_STRUCT_MM_H
#define ASCENDC_MODULE_STRUCT_MM_H
#include "utils/kernel_utils_constants.h"

namespace AscendC {
// MM intr params
using LoadData2dParams = struct LoadData2DParams;
#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 2103) || (__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) || \
    (__NPU_ARCH__ == 3113))
struct LoadData2DParams {
    __aicore__ LoadData2DParams() {}

    __aicore__ LoadData2DParams(const uint16_t startIndexIn, const uint8_t repeatTimesIn, const uint16_t srcStrideIn,
        const uint8_t sidIn, const uint16_t dstGapIn, const bool ifTransposeIn, const uint8_t addrModeIn)
        : startIndex(startIndexIn),
          repeatTimes(repeatTimesIn),
          srcStride(srcStrideIn),
          sid(sidIn),
          dstGap(dstGapIn),
          ifTranspose(ifTransposeIn),
          addrMode(addrModeIn)
    {}
    __aicore__ inline void SetStartIndex(uint16_t value)
    {
        startIndex = value;
    }

    __aicore__ inline void SetDstGap(uint16_t value)
    {
        dstGap = value;
    }

    __aicore__ inline void SetSrcStride(uint16_t value)
    {
        srcStride = value;
    }

    __aicore__ inline void SetIfTranspose(bool value)
    {
        ifTranspose = value;
    }

    __aicore__ inline void SetRepeatTimes(uint8_t value)
    {
        repeatTimes = value;
    }

    __aicore__ inline void SetSid(uint8_t value)
    {
        sid = value;
    }

    __aicore__ inline void SetAddrMode(uint8_t value)
    {
        addrMode = value;
    }

    __aicore__ inline void SetMStartPosition(uint32_t mStartPosition_)
    {
        (void)mStartPosition_;
    }

    __aicore__ inline void SetKStartPosition(uint32_t kStartPosition_)
    {
        (void)kStartPosition_;
    }

    __aicore__ inline void SetMStep(uint16_t mStep_)
    {
        (void)mStep_;
    }

    __aicore__ inline void SetKStep(uint16_t kStep_)
    {
        (void)kStep_;
    }

    __aicore__ inline void SetSrcStride(int32_t srcStride_)
    {
        (void)srcStride_;
    }

    __aicore__ inline void SetDstStride(uint16_t dstStride_)
    {
        (void)dstStride_;
    }

    __aicore__ inline void SetQmode(uint8_t qmode_)
    {
        (void)qmode_;
    }

    uint16_t startIndex = 0;
    uint16_t dstGap = 0;
    uint16_t srcStride = 0;
    bool ifTranspose = 0;
    uint8_t repeatTimes = 0;

    uint8_t sid = 0;
    uint8_t addrMode = 0;
};
#else
struct LoadData2DParams {
    __aicore__ LoadData2DParams() {}

    __aicore__ LoadData2DParams(const uint16_t startIndexIn, const uint8_t repeatTimesIn, const uint16_t srcStrideIn,
        const uint8_t sidIn, const uint16_t dstGapIn, const bool ifTransposeIn, const uint8_t addrModeIn)
        : startIndex(startIndexIn),
          repeatTimes(repeatTimesIn),
          srcStride(srcStrideIn),
          sid(sidIn),
          dstGap(dstGapIn),
          ifTranspose(ifTransposeIn),
          addrMode(addrModeIn)
    {}

    uint16_t startIndex = 0;
    uint16_t dstGap = 0;
    uint16_t srcStride = 0;
    bool ifTranspose = 0;
    uint8_t repeatTimes = 0;

    uint8_t sid = 0;
    uint8_t addrMode = 0;
};
#endif

#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 3113) && defined(__DAV_L311__))
using LoadData2DParamsV2 = struct LoadData2DParamsV311Gen;
using LoadData2dTransposeParams = struct LoadData2dTransposeParamsV311Gen;
#elif defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 3103) && defined(__DAV_L310__))
using LoadData2DParamsV2 = struct LoadData2DParamsV311Gen;
using LoadData2dTransposeParams = struct LoadData2dTransposeParamsV311Gen;
#elif defined(__NPU_ARCH__) && (__NPU_ARCH__ == 3003)
using LoadData2DParamsV2 = struct LoadData2DParamsV311Gen;
using LoadData2dTransposeParams = struct LoadData2dTransposeParamsV300;
#elif defined(__NPU_ARCH__) && (__NPU_ARCH__ == 2103)
using LoadData2DParamsV2 = struct LoadData2DParamsV311Gen;
using LoadData2dTransposeParams = struct LoadData2dTransposeParamsV210;
#else // Turing versions
struct LoadData2DParamsV2 {
    __aicore__ LoadData2DParamsV2() {}

    __aicore__ LoadData2DParamsV2(const uint32_t mStartPositionIn, const uint32_t kStartPositionIn,
        const uint16_t mStepIn, const uint16_t kStepIn, const int32_t srcStrideIn, const uint16_t dstStrideIn,
        const bool ifTransposeIn, const uint8_t sidIn)
        : mStartPosition(mStartPositionIn),
          kStartPosition(kStartPositionIn),
          mStep(mStepIn),
          kStep(kStepIn),
          srcStride(srcStrideIn),
          dstStride(dstStrideIn),
          ifTranspose(ifTransposeIn),
          sid(sidIn)
    {}

    uint32_t mStartPosition = 0;
    uint32_t kStartPosition = 0;
    uint16_t mStep = 0;
    uint16_t kStep = 0;
    int32_t srcStride = 0;
    uint16_t dstStride = 0;
    bool ifTranspose = false;
    uint8_t sid = 0;
};

struct LoadData2dTransposeParams {
    __aicore__ LoadData2dTransposeParams() {}

    __aicore__ LoadData2dTransposeParams(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstfracGapIn, const uint8_t addrModeIn)
        : startIndex(startIndexIn),
          repeatTimes(repeatTimesIn),
          srcStride(srcStrideIn),
          dstGap(dstGapIn),
          dstFracGap(dstfracGapIn),
          addrMode(addrModeIn)
    {}

    __aicore__ LoadData2dTransposeParams(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstfracGapIn)
        : startIndex(startIndexIn),
          repeatTimes(repeatTimesIn),
          srcStride(srcStrideIn),
          dstGap(dstGapIn),
          dstFracGap(dstfracGapIn)
    {}

    uint16_t startIndex = 0;
    uint8_t repeatTimes = 0;
    uint16_t srcStride = 0;
    uint16_t dstGap = 0;
    uint16_t dstFracGap = 0;
    uint8_t addrMode = 0;
};
#endif

#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 2103) || (__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) || \
    (__NPU_ARCH__ == 3113))
struct LoadData2dTransposeParamsV210 {
    __aicore__ LoadData2dTransposeParamsV210()
    {
        startIndex = 0;
        repeatTimes = 0;
        srcStride = 0;
        dstGap = 0;
        dstFracGap = 0;
        addrMode = 0;
    }

    __aicore__ LoadData2dTransposeParamsV210(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstfracGapIn, const uint8_t addrModeIn)
    {
        startIndex = startIndexIn;
        repeatTimes = repeatTimesIn;
        srcStride = srcStrideIn;
        dstGap = dstGapIn;
        dstFracGap = dstfracGapIn;
        addrMode = addrModeIn;
    }

    __aicore__ LoadData2dTransposeParamsV210(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstfracGapIn)
    {
        startIndex = startIndexIn;
        repeatTimes = repeatTimesIn;
        srcStride = srcStrideIn;
        dstGap = dstGapIn;
        dstFracGap = dstfracGapIn;
    }

    __aicore__ inline void SetStartIndex(uint16_t startIndex_)
    {
        startIndex = startIndex_;
    }

    __aicore__ inline void SetRepeatTimes(uint8_t repeatTimes_)
    {
        repeatTimes = repeatTimes_;
    }

    __aicore__ inline void SetSrcStride(uint16_t srcStride_)
    {
        srcStride = srcStride_;
    }

    __aicore__ inline void SetDstGap(uint16_t dstGap_)
    {
        dstGap = dstGap_;
    }

    __aicore__ inline void SetDstFracGap(uint16_t dstFracGap_)
    {
        dstFracGap = dstFracGap_;
    }

    __aicore__ inline void SetAddrMode(uint8_t addrMode_)
    {
        addrMode = addrMode_;
    }

    __aicore__ inline void SetMStartPosition(uint32_t mStartPosition_)
    {
        (void)mStartPosition_;
    }

    __aicore__ inline void SetKStartPosition(uint32_t kStartPosition_)
    {
        (void)kStartPosition_;
    }

    __aicore__ inline void SetMStep(uint16_t mStep_)
    {
        (void)mStep_;
    }

    __aicore__ inline void SetKStep(uint16_t kStep_)
    {
        (void)kStep_;
    }

    __aicore__ inline void SetSrcStride(int32_t srcStride_)
    {
        (void)srcStride_;
    }

    __aicore__ inline void SetDstStride(uint16_t dstStride_)
    {
        (void)dstStride_;
    }

    __aicore__ inline void SetIfTranspose(bool ifTranspose_)
    {
        (void)ifTranspose_;
    }

    __aicore__ inline void SetSid(uint8_t sid_)
    {
        (void)sid_;
    }

    __aicore__ inline void SetQmode(uint8_t qmode_)
    {
        (void)qmode_;
    }

    uint16_t startIndex = 0;
    uint8_t repeatTimes = 0;
    uint16_t srcStride = 0;
    uint16_t dstGap = 0;
    uint16_t dstFracGap = 0;
    uint8_t addrMode = 0;
};

struct LoadData2dTransposeParamsV300 {
    __aicore__ LoadData2dTransposeParamsV300()
    {
        startIndex = 0;
        repeatTimes = 0;
        srcStride = 0;
        dstGap = 0;
        dstFracGap = 0;
        addrMode = 0;
    }

    __aicore__ LoadData2dTransposeParamsV300(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstfracGapIn, const uint8_t addrModeIn)
    {
        startIndex = startIndexIn;
        repeatTimes = repeatTimesIn;
        srcStride = srcStrideIn;
        dstGap = dstGapIn;
        dstFracGap = dstfracGapIn;
        addrMode = addrModeIn;
    }

    __aicore__ LoadData2dTransposeParamsV300(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstfracGapIn)
    {
        startIndex = startIndexIn;
        repeatTimes = repeatTimesIn;
        srcStride = srcStrideIn;
        dstGap = dstGapIn;
        dstFracGap = dstfracGapIn;
    }

    __aicore__ inline void SetStartIndex(uint16_t startIndex_)
    {
        startIndex = startIndex_;
    }

    __aicore__ inline void SetRepeatTimes(uint8_t repeatTimes_)
    {
        repeatTimes = repeatTimes_;
    }

    __aicore__ inline void SetSrcStride(uint16_t srcStride_)
    {
        srcStride = srcStride_;
    }

    __aicore__ inline void SetDstGap(uint16_t dstGap_)
    {
        dstGap = dstGap_;
    }

    __aicore__ inline void SetDstFracGap(uint16_t dstFracGap_)
    {
        dstFracGap = dstFracGap_;
    }

    __aicore__ inline void SetAddrMode(uint8_t addrMode_)
    {
        addrMode = addrMode_;
    }

    __aicore__ inline void SetMStartPosition(uint32_t mStartPosition_)
    {
        (void)mStartPosition_;
    }

    __aicore__ inline void SetKStartPosition(uint32_t kStartPosition_)
    {
        (void)kStartPosition_;
    }

    __aicore__ inline void SetMStep(uint16_t mStep_)
    {
        (void)mStep_;
    }

    __aicore__ inline void SetKStep(uint16_t kStep_)
    {
        (void)kStep_;
    }

    __aicore__ inline void SetSrcStride(int32_t srcStride_)
    {
        (void)srcStride_;
    }

    __aicore__ inline void SetDstStride(uint16_t dstStride_)
    {
        (void)dstStride_;
    }

    __aicore__ inline void SetIfTranspose(bool ifTranspose_)
    {
        (void)ifTranspose_;
    }

    __aicore__ inline void SetSid(uint8_t sid_)
    {
        (void)sid_;
    }

    __aicore__ inline void SetQmode(uint8_t qmode_)
    {
        (void)qmode_;
    }

    uint16_t startIndex = 0;
    uint8_t repeatTimes = 0;
    uint16_t srcStride = 0;
    uint16_t dstGap = 0;
    uint16_t dstFracGap = 0;
    uint8_t addrMode = 0;
};

struct LoadData2DParamsV311Gen {
    __aicore__ LoadData2DParamsV311Gen()
    {
        mStartPosition = 0;
        kStartPosition = 0;
        mStep = 0;
        kStep = 0;
        srcStride = 0;
        dstStride = 0;
        ifTranspose = false;
        sid = 0;
        qmode = 0;
    }

    __aicore__ LoadData2DParamsV311Gen(const uint32_t mStartPositionIn, const uint32_t kStartPositionIn,
        const uint16_t mStepIn, const uint16_t kStepIn, const uint32_t srcStrideIn, const uint16_t dstStrideIn,
        const bool ifTransposeIn, const uint8_t sidIn, const uint8_t qmodeIn)
    {
        mStartPosition = mStartPositionIn;
        kStartPosition = kStartPositionIn;
        mStep = mStepIn;
        kStep = kStepIn;
        srcStride = srcStrideIn;
        dstStride = dstStrideIn;
        ifTranspose = ifTransposeIn;
        sid = sidIn;
        qmode = qmodeIn;
    }

    __aicore__ inline void SetMStartPosition(uint32_t mStartPosition_)
    {
        mStartPosition = mStartPosition_;
    }

    __aicore__ inline void SetKStartPosition(uint32_t kStartPosition_)
    {
        kStartPosition = kStartPosition_;
    }

    __aicore__ inline void SetMStep(uint16_t mStep_)
    {
        mStep = mStep_;
    }

    __aicore__ inline void SetKStep(uint16_t kStep_)
    {
        kStep = kStep_;
    }

    __aicore__ inline void SetSrcStride(int32_t srcStride_)
    {
        srcStride = srcStride_;
    }

    __aicore__ inline void SetDstStride(uint16_t dstStride_)
    {
        dstStride = dstStride_;
    }

    __aicore__ inline void SetIfTranspose(bool ifTranspose_)
    {
        ifTranspose = ifTranspose_;
    }

    __aicore__ inline void SetSid(uint8_t sid_)
    {
        sid = sid_;
    }

    __aicore__ inline void SetQmode(uint8_t qmode_)
    {
        qmode = qmode_;
    }

    uint32_t mStartPosition = 0;
    uint32_t kStartPosition = 0;
    uint16_t mStep = 0;
    uint16_t kStep = 0;
    int32_t srcStride = 0;
    uint16_t dstStride = 0;
    bool ifTranspose = false;
    uint8_t sid = 0;
    uint8_t qmode = 0;
};

struct LoadData2dTransposeParamsV311Gen {
    __aicore__ LoadData2dTransposeParamsV311Gen()
    {
        mStartPosition = 0;
        kStartPosition = 0;
        mStep = 0;
        kStep = 0;
        srcStride = 0;
        dstStride = 0;
        ifTranspose = false;
        sid = 0;
        qmode = 0;
    }

    __aicore__ LoadData2dTransposeParamsV311Gen(const uint32_t mStartPositionIn, const uint32_t kStartPositionIn,
        const uint16_t mStepIn, const uint16_t kStepIn, const uint32_t srcStrideIn, const uint16_t dstStrideIn,
        const bool ifTransposeIn, const uint8_t sidIn, const uint8_t qmodeIn)
    {
        mStartPosition = mStartPositionIn;
        kStartPosition = kStartPositionIn;
        mStep = mStepIn;
        kStep = kStepIn;
        srcStride = srcStrideIn;
        dstStride = dstStrideIn;
        ifTranspose = ifTransposeIn;
        sid = sidIn;
        qmode = qmodeIn;
    }

    __aicore__ inline void SetMStartPosition(uint32_t mStartPosition_)
    {
        mStartPosition = mStartPosition_;
    }

    __aicore__ inline void SetKStartPosition(uint32_t kStartPosition_)
    {
        kStartPosition = kStartPosition_;
    }

    __aicore__ inline void SetMStep(uint16_t mStep_)
    {
        mStep = mStep_;
    }

    __aicore__ inline void SetKStep(uint16_t kStep_)
    {
        kStep = kStep_;
    }

    __aicore__ inline void SetSrcStride(int32_t srcStride_)
    {
        srcStride = srcStride_;
    }

    __aicore__ inline void SetDstStride(uint16_t dstStride_)
    {
        dstStride = dstStride_;
    }

    __aicore__ inline void SetIfTranspose(bool ifTranspose_)
    {
        ifTranspose = ifTranspose_;
    }

    __aicore__ inline void SetSid(uint8_t sid_)
    {
        sid = sid_;
    }

    __aicore__ inline void SetQmode(uint8_t qmode_)
    {
        qmode = qmode_;
    }

    __aicore__ inline void SetStartIndex(uint16_t startIndex_)
    {
        (void)startIndex_;
    }

    __aicore__ inline void SetRepeatTimes(uint8_t repeatTimes_)
    {
        (void)repeatTimes_;
    }

    __aicore__ inline void SetDstGap(uint16_t dstGap_)
    {
        (void)dstGap_;
    }

    __aicore__ inline void SetDstFracGap(uint16_t dstFracGap_)
    {
        (void)dstFracGap_;
    }

    __aicore__ inline void SetAddrMode(uint8_t addrMode_)
    {
        (void)addrMode_;
    }

    uint32_t mStartPosition = 0;
    uint32_t kStartPosition = 0;
    uint16_t mStep = 0;
    uint16_t kStep = 0;
    int32_t srcStride = 0;
    uint16_t dstStride = 0;
    bool ifTranspose = false;
    uint8_t sid = 0;
    uint8_t qmode = 0;
};

#endif // Kirin versions

#if (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102)
struct LoadData2DMxParams {
    __aicore__ LoadData2DMxParams() {}

    __aicore__ LoadData2DMxParams(const uint16_t xStartPositionIn, const uint16_t yStartPositionIn,
        const uint8_t xStepIn, const uint8_t yStepIn, const uint16_t srcStrideIn, const uint16_t dstStrideIn)
    {
        xStartPosition = xStartPositionIn;
        yStartPosition = yStartPositionIn;
        xStep = xStepIn;
        yStep = yStepIn;
        srcStride = srcStrideIn;
        dstStride = dstStrideIn;
    }

    uint16_t xStartPosition = 0;
    uint16_t yStartPosition = 0;
    uint8_t xStep = 0;
    uint8_t yStep = 0;
    uint16_t srcStride = 0;
    uint16_t dstStride = 0;
};
#endif

#if (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102)
template <typename TYPE>
struct LoadData3DParamsV1 {
    using T = typename GetPadValueType<TYPE>::Type;
#else
template <typename T>
struct LoadData3DParamsV1 {
#endif
    __aicore__ LoadData3DParamsV1()
    {
        for (int32_t i = 0; i < PAD_SIZE; ++i) {
            padList[i] = 0;
        }
    }

    __aicore__ LoadData3DParamsV1(const uint8_t padListIn[PAD_SIZE], const uint16_t l1HIn, const uint16_t l1WIn,
        const uint16_t c1IndexIn, const uint8_t fetchFilterWIn, const uint8_t fetchFilterHIn, const int16_t leftTopWIn,
        const int16_t leftTopHIn, const uint8_t strideWIn, const uint8_t strideHIn, const uint8_t filterWIn,
        const uint8_t filterHIn, const uint8_t dilationFilterWIn, const uint8_t dilationFilterHIn,
        const uint8_t jumpStrideIn, const uint8_t repeatModeIn, const uint8_t repeatTimeIn, const uint8_t cSizeIn,
        const T padValueIn)
        : l1H(l1HIn),
          l1W(l1WIn),
          c1Index(c1IndexIn),
          fetchFilterW(fetchFilterWIn),
          fetchFilterH(fetchFilterHIn),
          leftTopW(leftTopWIn),
          leftTopH(leftTopHIn),
          strideW(strideWIn),
          strideH(strideHIn),
          filterW(filterWIn),
          filterH(filterHIn),
          dilationFilterW(dilationFilterWIn),
          dilationFilterH(dilationFilterHIn),
          jumpStride(jumpStrideIn),
          repeatMode(repeatModeIn),
          repeatTime(repeatTimeIn),
          cSize(cSizeIn),
          padValue(padValueIn)
    {
        for (int32_t i = 0; i < PAD_SIZE; ++i) {
            padList[i] = padListIn[i];
        }
    }

    uint8_t padList[PAD_SIZE] = {0};
    uint8_t strideW = 0;
    uint8_t strideH = 0;
    uint8_t filterW = 0;
    uint8_t filterH = 0;
    uint8_t dilationFilterW = 0;
    uint8_t dilationFilterH = 0;
    uint8_t jumpStride = 0;
    uint8_t repeatMode = 0;
    uint8_t repeatTime = 0;
    uint8_t cSize = 0;
    T padValue = 0;
    uint8_t fetchFilterW = 0;
    uint8_t fetchFilterH = 0;
    uint16_t l1H = 0;
    uint16_t l1W = 0;
    uint16_t c1Index = 0;
    int16_t leftTopW = 0;
    int16_t leftTopH = 0;
};

#if (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102)
template <typename TYPE>
struct LoadData3DParamsV2 {
    using T = typename GetPadValueType<TYPE>::Type;
#else
template <typename T>
struct LoadData3DParamsV2 {
#endif
    __aicore__ LoadData3DParamsV2()
    {
        for (int32_t i = 0; i < PAD_SIZE; ++i) {
            padList[i] = 0;
        }
#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 2103) || (__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) || \
    (__NPU_ARCH__ == 3113))
        enDualSrc = BM_DISABLE;
#endif
    }

    __aicore__ LoadData3DParamsV2(const uint8_t padListIn[PAD_SIZE], const uint16_t l1HIn, const uint16_t l1WIn,
        const uint16_t channelSizeIn, const uint16_t kExtensionIn, const uint16_t mExtensionIn,
        const uint16_t kStartPtIn, const uint16_t mStartPtIn, const uint8_t strideWIn, const uint8_t strideHIn,
        const uint8_t filterWIn, const uint8_t filterHIn, const uint8_t dilationFilterWIn,
        const uint8_t dilationFilterHIn, const bool enTransposeIn, const bool enSmallKIn, const T padValueIn)
        : l1H(l1HIn),
          l1W(l1WIn),
          channelSize(channelSizeIn),
          kExtension(kExtensionIn),
          mExtension(mExtensionIn),
          kStartPt(kStartPtIn),
          mStartPt(mStartPtIn),
          strideW(strideWIn),
          strideH(strideHIn),
          filterW(filterWIn),
          filterH(filterHIn),
          dilationFilterW(dilationFilterWIn),
          dilationFilterH(dilationFilterHIn),
          enTranspose(enTransposeIn),
          enSmallK(enSmallKIn),
          padValue(padValueIn)
    {
        for (int32_t i = 0; i < PAD_SIZE; ++i) {
            padList[i] = padListIn[i];
        }
    }

    __aicore__ LoadData3DParamsV2(const uint8_t padListIn[PAD_SIZE], const uint16_t l1HIn, const uint16_t l1WIn,
        const uint16_t channelSizeIn, const uint16_t kExtensionIn, const uint16_t mExtensionIn,
        const uint16_t kStartPtIn, const uint16_t mStartPtIn, const uint8_t strideWIn, const uint8_t strideHIn,
        const uint8_t filterWIn, const uint8_t filterHIn, const uint8_t dilationFilterWIn,
        const uint8_t dilationFilterHIn, const bool enTransposeIn, const bool enSmallKIn, const T padValueIn,
        const bool filterSizeWIn, const bool filterSizeHIn, const bool fMatrixCtrlIn)
        : l1H(l1HIn),
          l1W(l1WIn),
          channelSize(channelSizeIn),
          kExtension(kExtensionIn),
          mExtension(mExtensionIn),
          kStartPt(kStartPtIn),
          mStartPt(mStartPtIn),
          strideW(strideWIn),
          strideH(strideHIn),
          filterW(filterWIn),
          filterH(filterHIn),
          dilationFilterW(dilationFilterWIn),
          dilationFilterH(dilationFilterHIn),
          enTranspose(enTransposeIn),
          enSmallK(enSmallKIn),
          padValue(padValueIn),
          filterSizeW(filterSizeWIn),
          filterSizeH(filterSizeHIn),
          fMatrixCtrl(fMatrixCtrlIn)
    {
        for (int32_t i = 0; i < PAD_SIZE; ++i) {
            padList[i] = padListIn[i];
        }
    }

    uint8_t padList[PAD_SIZE] = {0};
    uint16_t l1H = 0;
    uint16_t l1W = 0;
    uint16_t channelSize = 0;
    uint16_t kExtension = 0;
    uint16_t mExtension = 0;
    uint16_t kStartPt = 0;
    uint16_t mStartPt = 0;

    uint8_t strideW = 1;
    uint8_t strideH = 1;
    uint8_t filterW = 1;
    uint8_t filterH = 1;
    uint8_t dilationFilterW = 1;
    uint8_t dilationFilterH = 1;
    bool enTranspose = false;
    bool enSmallK = false;
    T padValue = 0;
    bool filterSizeW = false;
    bool filterSizeH = false;
    bool fMatrixCtrl = false;
#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 2103) || (__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) || \
    (__NPU_ARCH__ == 3113))
    bm_t enDualSrc = BM_DISABLE;
#endif
};

struct LoadData2dTransposeParamsV2 {
    __aicore__ LoadData2dTransposeParamsV2() {}

    __aicore__ LoadData2dTransposeParamsV2(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstFracGapIn,
        const uint16_t srcFracGapIn)
        : startIndex(startIndexIn),
          repeatTimes(repeatTimesIn),
          srcStride(srcStrideIn),
          dstGap(dstGapIn),
          dstFracGap(dstFracGapIn),
          srcFracGap(srcFracGapIn)
    {}

    __aicore__ LoadData2dTransposeParamsV2(const uint16_t startIndexIn, const uint8_t repeatTimesIn,
        const uint16_t srcStrideIn, const uint16_t dstGapIn, const uint16_t dstFracGapIn,
        const uint16_t srcFracGapIn, const uint8_t addrModeIn)
        : startIndex(startIndexIn),
          repeatTimes(repeatTimesIn),
          srcStride(srcStrideIn),
          dstGap(dstGapIn),
          dstFracGap(dstFracGapIn),
          srcFracGap(srcFracGapIn),
          addrMode(addrModeIn)
    {}

    uint16_t startIndex = 0;
    uint8_t repeatTimes = 0;
    uint16_t srcStride = 0;
    uint16_t dstGap = 0;
    uint16_t dstFracGap = 0;
    uint16_t srcFracGap = 0;
    uint8_t addrMode = 0;
};

struct LoadImageToLocalParams {
    __aicore__ LoadImageToLocalParams() {}

    __aicore__ LoadImageToLocalParams(const uint16_t horizSizeIn, const uint16_t vertSizeIn,
        const uint16_t horizStartPosIn, const uint16_t vertStartPosIn, const uint16_t srcHorizSizeIn,
        const uint8_t topPadSizeIn, const uint8_t botPadSizeIn, const uint16_t leftPadSizeIn,
        const uint16_t rightPadSizeIn)
        : horizSize(horizSizeIn),
          vertSize(vertSizeIn),
          horizStartPos(horizStartPosIn),
          vertStartPos(vertStartPosIn),
          srcHorizSize(srcHorizSizeIn),
          topPadSize(topPadSizeIn),
          botPadSize(botPadSizeIn),
          leftPadSize(leftPadSizeIn),
          rightPadSize(rightPadSizeIn)
    {}

    uint16_t horizSize = 0;
    uint16_t vertSize = 0;
    uint16_t horizStartPos = 0;
    uint16_t vertStartPos = 0;
    uint16_t srcHorizSize = 0;
    uint8_t topPadSize = 0;
    uint8_t botPadSize = 0;
    uint16_t leftPadSize = 0;
    uint16_t rightPadSize = 0;
    uint8_t sid = 0;
};

} // namespace AscendC

#endif // ASCENDC_MODULE_STRUCT_MM_H