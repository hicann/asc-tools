#!/usr/bin/python
# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

EXPONENT_BIAS_BF16 = 127


def decode_bfloat16(num : int) -> float:
    if num == 0:
        return 0
    elif num == 0xFF80:
        return float("-inf")
    elif num == 0x7F80:
        return float("inf")
    if num >= 0x8000:
        signal_val = -1
    else:
        signal_val = 1
    exponent_val = (num % 0x8000 // 0x0080)
    mantissa_val = (num % 0x8000 % 0x0080) / 0x0080
    if exponent_val == 0xFF and mantissa_val > 0:
        return float("nan")
    if exponent_val == 0:
        return signal_val * 2 ** (-EXPONENT_BIAS_BF16 + 1) * mantissa_val
    else:
        return signal_val * 2 ** (exponent_val - EXPONENT_BIAS_BF16) * (1 + mantissa_val)