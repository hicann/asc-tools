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
 * \file kernel_fp8_e4m3.h
 * \brief
 */
#ifndef ASCENDC_FP8_E4M3_H
#define ASCENDC_FP8_E4M3_H
#include <cstdint>

namespace float8_e4m3 {
/*
 * @ingroup Fp8e4m3T
 * bit7:        1 bit SIGN      +---+----+----+
 * bit6-3:      4 bit EXP       | S |EEEE| MMM |
 * bit0-2:      3 bit MAN       +---+----+----+
 */
struct Fp8e4m3T {
    int8_t val;
public:
    Fp8e4m3T(void) : val(0x0u) {}
    Fp8e4m3T(const Fp8e4m3T& fp8) : val(fp8.val) {}
    Fp8e4m3T(const float src) : val(FloatToFp8e4m3(src)) {}
    Fp8e4m3T& operator=(const Fp8e4m3T& fp8)
    {
        if (this != &fp8) {
            this->val = fp8.val;
        }
        return *this;
    }
    int8_t FloatToFp8e4m3(const float src) const;
    operator float() const;
    float ToFloat() const;
};
} // namespace float8_e4m3
using fp8_e4m3fn_t = float8_e4m3::Fp8e4m3T;
#endif // ASCENDC_FP8_E4M3_H
