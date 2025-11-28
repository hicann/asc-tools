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
 * \file kernel_fp32.h
 * \brief
 */
#ifndef ASCENDC_FP32_H
#define ASCENDC_FP32_H

#include <cstdint>

// FP32
constexpr uint32_t FP32_SIGN_INDEX = 31;
constexpr uint32_t FP32_MAN_LEN = 23;
constexpr uint32_t FP32_EXP_BIAS = 127;
/*
 * @ingroup fp32 basic parameter
 * @brief   sign mask of fp32         (1 0000 0000  0000 0000 0000 0000 000)
 */
#define FP32_SIGN_MASK (0x80000000u)
/*
 * @ingroup fp32 basic parameter
 * @brief   exponent mask of fp32     (  1111 1111  0000 0000 0000 0000 000)
 */
#define FP32_EXP_MASK (0x7F800000u)
/*
 * @ingroup fp32 basic parameter
 * @brief   mantissa mask of fp32     (             1111 1111 1111 1111 111)
 */
#define FP32_MAN_MASK (0x007FFFFFu)
/*
 * @ingroup fp32 basic parameter
 * @brief   hidd bit of mantissa of fp32      (  1  0000 0000 0000 0000 000)
 */
#define FP32_MAN_HIDE_BIT (0x00800000u)
/**
 * @ingroup fp32 basic parameter
 * @brief   maximum exponent value of fp32 is 255(1111 1111)
 */
#define FP32_MAX_EXP (0xFF)
/*
 * @ingroup fp32 basic parameter
 * @brief   maximum mantissa value of fp32    (1111 1111 1111 1111 1111 111)
 */
#define FP32_MAX_MAN (0x7FFFFF)
#define FP32_MAX_MAN (0x7FFFFF)
#define FP32_ABS_MAX (0x7FFFFFFFu)
#define FP32_POS_INF (0x7F800000)
#define FP32_NEG_INF (0xff800000)
constexpr uint32_t FP32_NAN = 0x7FFFFFFF;

inline uint32_t Fp32Constructor(uint32_t s, uint32_t e, uint32_t m)
{
    return (((s) << FP32_SIGN_INDEX) | ((e) << FP32_MAN_LEN) | ((m) & FP32_MAX_MAN));
}

inline bool Fp32IsInf(const uint32_t x)
{
    return ((((x) & FP32_EXP_MASK) == FP32_EXP_MASK) && (((x) & FP32_MAN_MASK) == 0));
}
/*
 * @ingroup fp32 special value judgment
 * @brief   whether a fp32 is NaN
 */
inline bool Fp32IsNan(const uint32_t x)
{
    return ((((x) & FP32_EXP_MASK) == FP32_EXP_MASK) && (((x) & FP32_MAN_MASK) != 0));
}

inline bool Fp32IsZero(const uint32_t x)
{
    return (((x) & FP32_ABS_MAX) == 0);
}
/*
 * @ingroup fp32 basic operator
 * @brief   get sign of fp32
 */
inline uint16_t Fp32ExtracSign(uint32_t x)
{
    return (((x) >> FP32_SIGN_INDEX) & 1);
}
/*
 * @ingroup fp32 basic operator
 * @brief   get exponent of fp32
 */
inline uint32_t Fp32ExtracExp(uint32_t x)
{
    return (((x) & FP32_EXP_MASK) >> FP32_MAN_LEN);
}

/**
 * @ingroup fp32 basic operator
 * @brief   get mantissa of fp16
 */
inline uint32_t Fp32ExtracMan(uint32_t x)
{
    return (((x) & FP32_MAN_MASK) | (((((x) >> FP32_MAN_LEN) & FP32_MAX_EXP) > 0 ? 1 : 0) * FP32_MAN_HIDE_BIT));
}
#endif // ASCENDC_FP32_H
