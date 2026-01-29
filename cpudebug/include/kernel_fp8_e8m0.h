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
 * \file kernel_fp8_e8m0.h
 * \brief
 */
#ifndef ASCENDC_FP8_E8M0_H
#define ASCENDC_FP8_E8M0_H
#include <cstdint>

namespace float8_e8m0 {
/*
 * @ingroup Fp8e8m0T
 * bit:         0 bit SIGN      +---+-----+---+
 * bit7-0:      8 bit EXP       |   EEEEEEEE  |
 * bit:         0 bit MAN       +---+-----+---+
 */
struct Fp8e8m0T {
    uint8_t val;
public:
    Fp8e8m0T(void) : val(0x0u) {}
    Fp8e8m0T(const Fp8e8m0T& fp8) : val(fp8.val) {}
    Fp8e8m0T(const float src) : val(FloatToFp8e8m0(src)) {}
    Fp8e8m0T& operator=(const Fp8e8m0T& fp8)
    {
        if (this != &fp8) {
            this->val = fp8.val;
        }
        return *this;
    }
    uint8_t FloatToFp8e8m0(const float src) const;
    operator float() const;
    float ToFloat() const;
};
} // namespace float8_e8m0
using fp8_e8m0_t = float8_e8m0::Fp8e8m0T;
#endif // ASCENDC_FP8_E8M0_H