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
 * \file kernel_fp4_e2m1.h
 * \brief
 */
#ifndef ASCENDC_FP4_E2M1_H
#define ASCENDC_FP4_E2M1_H
#include <cstdint>
#include "kernel_bf16.h"

namespace float4_e2m1 {
// FP4 (E2M1)
constexpr uint32_t FP4_E2M1_MAN_HIDE_BIT = 0x2;
constexpr uint32_t FP4_SIGN_INDEX = 3;
constexpr uint32_t FP4_T_POS_MAX = 0x7;
constexpr uint32_t FP4_T_NEG_MAX = 0xf;
constexpr uint32_t FP4_T_NAN = 0;
constexpr uint32_t FP4_E2M1_MAX_MAN = 0x1;
constexpr uint32_t FP4_E2M1_MAX_EXP = 0x3;
constexpr uint32_t FP4_E2M1_EXP_LEN = 2;
constexpr uint32_t FP4_E2M1_MAN_LEN = 1;
inline uint8_t FP4E2M1_CONSTRUCTOR(uint16_t s, uint16_t e, uint16_t m)
{
    return (((s) << FP4_SIGN_INDEX) | ((e) << FP4_E2M1_MAN_LEN) | ((m) & FP4_E2M1_MAX_MAN));
}

/*
 * @ingroup Fp4e2m1T
 * bit3:        1 bit SIGN      +---+----+---+
 * bit2-1:      2 bit EXP       | S | EE | M |
 * bit0:        1 bit MAN       +---+----+---+
 */
struct Fp4e2m1T {
    uint8_t val = 0;
public:
    Fp4e2m1T(void) : val(0x0u) {}
    Fp4e2m1T(const Fp4e2m1T& fp4) : val(fp4.val) {}
    Fp4e2m1T(const bfloat16::Bf16T src) : val(BfloatToFp4e2m1(src.val)) {}
    Fp4e2m1T& operator=(const Fp4e2m1T& fp4)
    {
        if (this != &fp4) {
            this->val = fp4.val;
        }
        return *this;
    }
    uint8_t BfloatToFp4e2m1(const uint16_t src) const;
    operator bfloat16::Bf16T() const;
};
} // namespace float4_e2m1
using fp4x2_e2m1_t = float4_e2m1::Fp4e2m1T;
#endif
