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
 * \file kernel_utils.h
 * \brief
 */
#ifndef ASCENDC_MODULE_UTILS_H
#define ASCENDC_MODULE_UTILS_H
#include "utils/kernel_utils_ceil_oom_que.h"
#include "utils/kernel_utils_constants.h"
#include "utils/kernel_utils_struct_norm_sort.h"
#include "utils/kernel_utils_mode_cpu.h"
#include "utils/kernel_utils_ceil_oom_que.h"
#include "utils/kernel_utils_constants.h"
#include "utils/kernel_utils_struct_norm_sort.h"

namespace AscendC {
#if defined(__NPU_ARCH__) &&                                        \
    ((__NPU_ARCH__ == 2201) || (__NPU_ARCH__ == 3002) ||            \
     (__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))

#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))

namespace FPTranslation {
// HiFloat8 -> Fp32
#define HIF8_SIGN_INDEX (7)
#define HIF8_BIT6_INDEX (6)
#define HIF8_BIT5_INDEX (5)
#define HIF8_BIT4_INDEX (4)
#define HIF8_BIT3_INDEX (3)
#define HIF8_EXTRACT_SIGN(x) (((x) >> HIF8_SIGN_INDEX) & 0x1)
#define HIF8_EXTRACT_BIT6(x) (((x) >> HIF8_BIT6_INDEX) & 0x1)
#define HIF8_EXTRACT_BIT5(x) (((x) >> HIF8_BIT5_INDEX) & 0x1)
#define HIF8_EXTRACT_BIT4(x) (((x) >> HIF8_BIT4_INDEX) & 0x1)
#define HIF8_EXTRACT_BIT3(x) (((x) >> HIF8_BIT3_INDEX) & 0x1)

constexpr int8_t HIF8_NAN = 0x80;
constexpr int8_t HIF8_POS_INF = 0x6F;
constexpr int8_t HIF8_NEG_INF = 0xEF;
constexpr int8_t HIF8_BIT_LEN = 8;
constexpr uint32_t FP32_NAN = 0x7FFFFFFF;
constexpr uint32_t FP32_EXP_BIAS = 127;

#define FP32_MAX_MAN (0x7FFFFF)
#define FP32_POS_INF (0x7F800000)
#define FP32_NEG_INF (0xff800000)
constexpr uint32_t FP32_SIGN_INDEX = 31;
constexpr uint32_t FP32_MAN_LEN = 23;

// FP8 (E5M2) -> Fp32
#define FP8_SIGN_INDEX (7)
#define FP8_T_NAN (0x7F)
#define FP8_MAX_MAN (0x7)

constexpr int16_t FP8E5M2_EXP_MASK = 0x7C;
constexpr int16_t FP8E5M2_MAN_MASK = 0x3;
constexpr uint32_t FP8E5M2_MAN_LEN = 2;
constexpr uint32_t FP8E5M2_EXP_BIAS = 15;

#define FP8E5M2_MAN_HIDE_BIT (0x4)
#define FP8E5M2_MAX_EXP (0x1F)
#define FP8E5M2_MAX_MAN (0x3)
#define FP8E5M2_INF (0X7C)

// FP8 (E4M3) -> Fp32
#define FP8_SIGN_INDEX (7)
#define FP8_T_NAN (0x7F)
constexpr uint32_t FP8E4M3_EXP_BIAS = 7;
constexpr uint32_t FP8E4M3_MAN_LEN = 3;
#define FP8_MAX_EXP (0xF)
#define FP8_MAX_MAN (0x7)
#define FP8_MAN_HIDE_BIT (0x8)

} // FPTranslation
#endif
#endif
} // namespace AscendC

namespace AscendC {
struct SliceInfo {
    __aicore__ SliceInfo() {}

    __aicore__ SliceInfo(const uint32_t startIndexIn, const uint32_t endIndexIn, const uint32_t strideIn,
        const uint32_t burstLenIn, const uint32_t shapeValueIn = 0)
        : startIndex(startIndexIn),
          endIndex(endIndexIn),
          stride(strideIn),
          burstLen(burstLenIn),
          shapeValue(shapeValueIn)
    {}

    uint32_t startIndex = 0;
    uint32_t endIndex = ONE_BLK_SIZE - 1;
    uint32_t stride = 0;
    uint32_t burstLen = ONE_BLK_SIZE;
    uint32_t shapeValue = 0;
};

class AscendCUtils {
public:
    __aicore__ static inline int32_t GetC0Size()
    {
        return DEFAULT_C0_SIZE;
    }

    __aicore__ static inline int32_t GetC0Count(const int32_t dtypeSize)
    {
        ASCENDC_ASSERT((dtypeSize != 0), { KERNEL_LOG(KERNEL_ERROR, "dtypeSize can not be 0"); });
        return GetC0Size() / dtypeSize;
    }
};

#ifdef ASCENDC_CPU_DEBUG
enum AtomicType {
    SUM,
    MAX,
    MIN
};
extern bool g_isAtomic;
extern AtomicType g_atomicType;

#endif // ASCENDC_CPU_DEBUG

} // namespace AscendC
#endif // ASCENDC_MODULE_UTILS_H
