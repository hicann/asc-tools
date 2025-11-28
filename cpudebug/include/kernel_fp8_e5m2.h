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
 * \file kernel_fp8_e5m2.h
 * \brief
 */
#ifndef ASCENDC_FP8_E5M2_H
#define ASCENDC_FP8_E5M2_H
#include <cstdint>

namespace float8_e5m2 {
/*
 * @ingroup Fp8e5m2T
 * bit7:        1 bit SIGN      +---+-----+----+
 * bit6-2:      5 bit EXP       | S |EEEEE| MM |
 * bit0-1:      2 bit MAN       +---+-----+----+
 */
struct Fp8e5m2T {
    int8_t val;
public:
    Fp8e5m2T(void) : val(0x0u) {}
    Fp8e5m2T(const Fp8e5m2T& fp8) : val(fp8.val) {}
    Fp8e5m2T(const float src) : val(FloatToFp8e5m2(src)) {}
    Fp8e5m2T& operator=(const Fp8e5m2T& fp8)
    {
        if (this != &fp8) {
            this->val = fp8.val;
        }
        return *this;
    }
    int8_t FloatToFp8e5m2(const float src) const;
    operator float() const;
    float ToFloat() const;
};
} // namespace float8_e5m2
using fp8_e5m2_t = float8_e5m2::Fp8e5m2T;
#endif // ASCENDC_FP8_E5M2_H
