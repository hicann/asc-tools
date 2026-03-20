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
#if defined (__NPU_ARCH__) && (__NPU_ARCH__ != 3101)
#include "kernel_fp16.h"
#include "kernel_fp4_e2m1.h"
#include "kernel_fp4_e1m2.h"

using namespace std;

class TEST_FP16 : public testing::Test {
protected:
    void SetUp() {}

    void TearDown() {}
};

TEST_F(TEST_FP16, ToFloatTest)
{
    uint16_t fpVal = 0x1234;
    half val(fpVal);
    float result = val.ToFloat();
    float golden = 4660;
    float delta = std::abs(result - golden);
    EXPECT_LT(delta, 0.00000001);
    uint16_t fpVal1 = 0x7C00;
    half val1;
    val1.val = fpVal1;
    float result1 = val1.ToFloat();
    EXPECT_TRUE(std::isinf(result1));
}

TEST_F(TEST_FP16, ToDoubleTest)
{
    uint16_t fpVal = 0x1234;
    half val(fpVal);
    double result = val.ToDouble();
    double golden = 4660;
    double delta = std::abs(result - golden);
    EXPECT_LT(delta, 0.00000001);
}

TEST_F(TEST_FP16, ToInt8Test)
{
    uint16_t fpVal = 0x4321;
    half val(fpVal);
    int8_t result = val.ToInt8();
    int8_t golden = 127;
    int8_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, ToUInt8Test)
{
    uint16_t fpVal = 0x4321;
    half val(fpVal);
    uint8_t result = val.ToUInt8();
    uint8_t golden = 255;
    int8_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, ToInt16Test)
{
    uint16_t fpVal = 0x3FFF;
    half val(fpVal);
    int16_t result = val.ToInt16();
    int16_t golden = 16384;
    int16_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, ToUInt16Test)
{
    uint16_t fpVal = 0x5FFF;
    half val(fpVal);
    uint16_t result = val.ToUInt16();
    uint16_t golden = 24576;
    int16_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, ToInt32Test)
{
    uint16_t fpVal = 0x7FFF;
    half val(fpVal);
    int32_t result = val.ToInt32();
    int32_t golden = 32768;
    int32_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, ToUInt32Test)
{
    uint16_t fpVal = 0x4321;
    half val(fpVal);
    uint32_t result = val.ToUInt32();
    uint32_t golden = 17184;
    int32_t delta = result - golden;
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, OpEQTest)
{
    double d_val = 123456789.1234;
    half val;
    val = d_val;
    int32_t result = val.ToInt32();
    int32_t golden = 65504;
    int32_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, OpMulEQTest)
{
    uint16_t fpVal = 0x4321;
    half val(fpVal);
    val *= val;
    int32_t result = val.ToInt32();
    int32_t golden = 0x7FFFFFFF;
    int32_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, OpMulTest)
{
    uint16_t fpVal = 0x11;
    half val(fpVal);
    half ret;
    ret = val * val;
    int32_t result = ret.ToInt32();
    int32_t golden = 289;
    int32_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, OpDivTest)
{
    uint16_t fpVal1 = 0x12;
    uint16_t fpVal2 = 0x7FF;
    half val1(fpVal1);
    half val2(fpVal2);
    half ret;
    ret = val2 / val1;
    int32_t result = ret.ToInt32();
    int32_t golden = 114;
    int32_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}

TEST_F(TEST_FP16, OpDivEQTest)
{
    uint16_t fpVal1 = 0x1234;
    uint16_t fpVal2 = 0x7FFF;
    half val1(fpVal1);
    half val2(fpVal2);
    val2 /= val1;
    int32_t result = val2.ToInt32();
    int32_t golden = 7;
    int32_t delta = std::abs(result - golden);
    EXPECT_EQ(delta, 0);
}
#endif