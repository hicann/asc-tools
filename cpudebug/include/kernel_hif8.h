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
 * \file kernel_hif8.h
 * \brief
 */
#ifndef ASCENDC_HIF8_H
#define ASCENDC_HIF8_H
#include <cstdint>

namespace hifloat8 {
/*
 * @ingroup Hif8T
 Type           Algo                       Coding                   Value
                                   S      D       E         M       Sv   Ev      Mv
subnormal   Sv * 2^(Mv - 23)     0 ~ 1  0000      -     000 ~ 111   ±1   -     [0,7]
----------------------------------------------------------------------------------------
normal                           0 ~ 1  0001      -     000 ~ 111   ±1   0     [0/8,7/8]
                                 -------------------------------------------------------
            Sv * 2^Ev*(1 + Mv)   0 ~ 1   001    0 ~ 1   000 ~ 111   ±1   ±1    [0/8,7/8]
                                 -------------------------------------------------------
                                 0 ~ 1   01    00 ~ 11  000 ~ 111   ±1 ±[2,3]  [0/8,7/8]
                                 -------------------------------------------------------
                                 0 ~ 1   10   000 ~ 111  00 ~ 11    ±1 ±[4,7]  [0/4,3/4]
                                 --------------------------------------------------------
                                 0 ~ 1   11  0000 ~ 1111  0 ~ 1     ±1 ±[8,15]  [0/2,1/2]
-----------------------------------------------------------------------------------------
 */
struct Hif8T {
    int8_t val;
public:
    Hif8T(void) : val(0x0u) {}
    Hif8T(const Hif8T& fp8) : val(fp8.val) {}
    Hif8T(const float src) : val(FloatToHif8(src)) {}
    Hif8T& operator=(const Hif8T& fp8)
    {
        if (this != &fp8) {
            this->val = fp8.val;
        }
        return *this;
    }
    int8_t FloatToHif8(const float src) const;
    operator float() const;
    float ToFloat() const;
};
} // namespace hifloat8
using hifloat8_t = hifloat8::Hif8T;
#endif // ASCENDC_HIF8_H
