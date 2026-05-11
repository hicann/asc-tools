/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#include <gtest/gtest.h>
#include <cmath>
#include "kernel_bf16.h"
#include "kernel_fp4_e1m2.h"
#include "kernel_fp4_e2m1.h"
#include "kernel_fp8_e4m3.h"
#include "kernel_fp8_e5m2.h"
#include "kernel_fp8_e8m0.h"
#include "kernel_hif8.h"

using namespace std;

class TEST_DATA_TYPE : public testing::Test {
protected:
    void SetUp() {}

    void TearDown() {}
};

TEST_F(TEST_DATA_TYPE, DataTypeTest)
{
    int8_t ret = 0;
    bfloat16::Bf16T bf16Ttype;
    float4_e1m2::Fp4e1m2T fp4e1m2Ttype;
    float4_e2m1::Fp4e2m1T fp4e2m1Ttype;
    float8_e4m3::Fp8e4m3T fp8e4m3Ttype;
    float8_e5m2::Fp8e5m2T fp8e5m2Ttype;
    float8_e8m0::Fp8e8m0T fp8e8m0Ttype;
    hifloat8::Hif8T hif8Ttype;

    bf16Ttype.val = 0;
    ASSERT_EQ(ret, bf16Ttype.val);
    fp4e1m2Ttype.val = 0;
    ASSERT_EQ(ret, fp4e1m2Ttype.val);
    fp4e2m1Ttype.val = 0;
    ASSERT_EQ(ret, fp4e2m1Ttype.val);
    fp8e4m3Ttype.val = 0;
    ASSERT_EQ(ret, fp8e4m3Ttype.val);
    fp8e5m2Ttype.val = 0;
    ASSERT_EQ(ret, fp8e5m2Ttype.val);
    fp8e8m0Ttype.val = 0;
    ASSERT_EQ(ret, fp8e8m0Ttype.val);
    hif8Ttype.val = 0;
    ASSERT_EQ(ret, hif8Ttype.val);
}