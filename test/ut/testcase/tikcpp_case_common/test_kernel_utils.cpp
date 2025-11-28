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
#include <string>
#include <fstream>
#include "stub_def.h"
#include "stub_fun.h"
#include "kernel_utils.h"
#include "kernel_tensor_impl.h"
#include <mockcpp/mockcpp.hpp>
using namespace std;
using namespace AscendC;
namespace AscendC {
int32_t TensorWriteFile(const std::string& fileName, const void* buffer, size_t size);
std::string FormatEvent(int32_t w);
} // namespace AscendC

struct TensorWriteTestParams {
    string fileName;
    string buffer;
    int32_t retGolden;
};

class TensorWriteTestsuite : public testing::Test, public testing::WithParamInterface<TensorWriteTestParams> {
protected:
    void SetUp() {}
    void TearDown() {}
};

INSTANTIATE_TEST_CASE_P(TEST_TENSOR_WRITE, TensorWriteTestsuite,
    ::testing::Values(
#if __CCE_AICORE__ == 200
#if (__ST_CHIP_VER__ == 710)
    TensorWriteTestParams{ "ut_710_test_tensor_write_v200_1.bin", "111111111", 0 },
    TensorWriteTestParams{ "ut_710_test_tensor_write_v200_2.bin", "", -1 },
#elif (__ST_CHIP_VER__ == 610)
    TensorWriteTestParams{ "ut_610_test_tensor_write_v200_3.bin", "111111111", 0 },
    TensorWriteTestParams{ "ut_610_test_tensor_write_v200_4.bin", "", -1 },
#endif
#elif (__CCE_AICORE__ == 100)
    TensorWriteTestParams{ "ut_910_test_tensor_write_other_1.bin", "111111111", 0 },
    TensorWriteTestParams{ "ut_910_test_tensor_write_other_2.bin", "", -1 },
#elif (__CCE_AICORE__ == 220)
#if defined(__DAV_C220_VEC__)
    TensorWriteTestParams{ "ut_920_vec_test_tensor_write_other_1.bin", "111111111", 0 },
    TensorWriteTestParams{ "ut_920_vec_test_tensor_write_other_2.bin", "", -1 },
#else
    TensorWriteTestParams{ "ut_920_cube_test_tensor_write_other_1.bin", "111111111", 0 },
    TensorWriteTestParams{ "ut_920_cube_test_tensor_write_other_2.bin", "", -1 },
#endif
#endif
    TensorWriteTestParams{ "", "", -1 }));

TEST_P(TensorWriteTestsuite, TensorWriteTestCase)
{
    auto param = GetParam();

    int32_t ret = TensorWriteFile(param.fileName, (const void*)param.buffer.c_str(), param.buffer.length());
    EXPECT_EQ(ret, param.retGolden);
    if (ret != -1) {
        // check file param.fileName
        ifstream resultFile;
        std::stringstream streambuffer;
        resultFile.open(param.fileName, ios::in);
        EXPECT_TRUE(resultFile.is_open());
        streambuffer << resultFile.rdbuf();
        string resultString(streambuffer.str());
        EXPECT_EQ(resultString.length(), param.buffer.length());
        EXPECT_EQ(resultString, param.buffer);
        EXPECT_EQ(remove(param.fileName.c_str()), 0);
    }
}

template <typename T> struct ScalarStringPair {
    T input;
    string golden;
};

template <typename T> string ConvScalar2Str(uint8_t* input)
{
    T scalarValue = *((T*)input);
    return ScalarToString(scalarValue);
}

struct ScalarConvStringParams {
    uint8_t* input;
    string golden;
    string (*calc_func)(uint8_t*);
};

class ScalarConvStrTestsuite : public testing::Test, public testing::WithParamInterface<ScalarConvStringParams> {
protected:
    void SetUp() {}
    void TearDown() {}
};
namespace {
ScalarStringPair<int8_t> dataInt8{ -8, "-8" };
ScalarStringPair<uint8_t> dataUInt8{ 8, "8" };
ScalarStringPair<int16_t> dataInt16{ -188, "-188" };
ScalarStringPair<uint16_t> dataUInt16{ 18, "18" };
ScalarStringPair<float> dataFloat{ -88.888, "-88.888000" };
ScalarStringPair<int32_t> dataInt32{ -88888, "-88888" };
ScalarStringPair<uint32_t> dataUInt32{ 0x88888, "559240" };
ScalarStringPair<int64_t> dataInt64{ -88888888888, "-88888888888" };
ScalarStringPair<uint64_t> dataUInt64{ 0xFFFFFFFFFFFFFFFF, "18446744073709551615" };
ScalarStringPair<half> dataHalf{ 1.325, "1.325195" };
} // namespace

INSTANTIATE_TEST_CASE_P(TEST_OPEARATION_HALF_EQ, ScalarConvStrTestsuite,
    ::testing::Values(ScalarConvStringParams{ (uint8_t*)(&dataInt8.input), dataInt8.golden, ConvScalar2Str<int8_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataUInt8.input), dataUInt8.golden, ConvScalar2Str<uint8_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataInt16.input), dataInt16.golden, ConvScalar2Str<int16_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataUInt16.input), dataUInt16.golden, ConvScalar2Str<uint16_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataFloat.input), dataFloat.golden, ConvScalar2Str<float> },
    ScalarConvStringParams{ (uint8_t*)(&dataInt32.input), dataInt32.golden, ConvScalar2Str<int32_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataUInt32.input), dataUInt32.golden, ConvScalar2Str<uint32_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataInt64.input), dataInt64.golden, ConvScalar2Str<int64_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataUInt64.input), dataUInt64.golden, ConvScalar2Str<uint64_t> },
    ScalarConvStringParams{ (uint8_t*)(&dataHalf.input), dataHalf.golden, ConvScalar2Str<half> }));

TEST_P(ScalarConvStrTestsuite, ScalarConvTestCase)
{
    auto param = GetParam();
    string ret = param.calc_func(param.input);
    EXPECT_EQ(ret, param.golden);
}

class ModelRegisterTest : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

class TEST_UTILS : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(TEST_UTILS, UTILS_PARAM_TEST)
{
#if (__CCE_AICORE__ <= 200)
    const uint32_t TMP_UB_OFFSETGolden = 248 * 1024;
#elif (__CCE_AICORE__ == 220)
    const uint32_t TMP_UB_OFFSETGolden = 184 * 1024;
#endif
    EXPECT_EQ(TMP_UB_OFFSET, TMP_UB_OFFSETGolden);
}

TEST_F(TEST_UTILS, TEST_WORK_SPACE_PTR)
{
    g_sysWorkspaceReserved = nullptr;
    g_sysWorkspaceReserved = GetSysWorkSpacePtr();
    EXPECT_EQ(g_sysWorkspaceReserved, nullptr);
}

TEST_F(TEST_UTILS, TEST_TENSOR_SHAPE)
{
    LocalTensor<half> localTensor;
    localTensor.SetShapeInfo(ShapeInfo(ASCENDC_SHAPE(2, 128, 256)));
    ShapeInfo newShapeInfo = localTensor.GetShapeInfo();
    EXPECT_EQ(newShapeInfo.shapeDim, 2);
    EXPECT_EQ(GetShapeSize(newShapeInfo), 128 * 256);
    uint32_t* shapeValue = newShapeInfo.shape;
    int shapeArray[] = {128, 256};
    for (int index = 0; index < newShapeInfo.shapeDim; ++index) {
        EXPECT_EQ(shapeValue[index], shapeArray[index]);
    }
    localTensor.SetShapeInfo(ShapeInfo(ASCENDC_SHAPE(3, 32, 32, 128), DataFormat::NZ));
    ShapeInfo newShapeInfo2 = localTensor.GetShapeInfo();
    EXPECT_EQ(newShapeInfo2.shapeDim, 3);
    EXPECT_EQ(newShapeInfo2.dataFormat, DataFormat::NZ);
    EXPECT_EQ(GetShapeSize(newShapeInfo2), 32 * 32 * 128);
    shapeValue = newShapeInfo2.shape;
    uint32_t shapeArrayGolden[] = {32, 32, 128};
    for (int index = 0; index < newShapeInfo2.shapeDim; ++index) {
        EXPECT_EQ(shapeValue[index], shapeArrayGolden[index]);
    }
    localTensor.SetShapeInfo(ShapeInfo(ASCENDC_SHAPE(3, 32, 32, 128), ASCENDC_SHAPE(2, 32, 3840), DataFormat::NZ));
    ShapeInfo newShapeInfo3 = localTensor.GetShapeInfo();
    EXPECT_EQ(newShapeInfo3.shapeDim, 3);
    EXPECT_EQ(newShapeInfo3.dataFormat, DataFormat::NZ);
    EXPECT_EQ(newShapeInfo3.originalShapeDim, 2);
    EXPECT_EQ(GetShapeSize(newShapeInfo3), 32 * 32 * 128);
    shapeValue = newShapeInfo3.originalShape;
    uint32_t shapeArrayGolden1[] = {32, 3840};
    for (int index = 0; index < newShapeInfo3.originalShapeDim; ++index) {
        EXPECT_EQ(shapeValue[index], shapeArrayGolden1[index]);
    }
    const uint32_t shapeDim = 3;
    uint32_t a = 16;
    uint32_t b = 17;
    uint32_t c;
    uint32_t array[] = {a, b, c};
    localTensor.SetShapeInfo(ShapeInfo(shapeDim, array));
    ShapeInfo newShapeInfo4 = localTensor.GetShapeInfo();
    EXPECT_EQ(newShapeInfo4.shapeDim, shapeDim);
    shapeValue = newShapeInfo4.shape;
    uint32_t shapeArrayGolden2[] = {a, b, c};
    EXPECT_EQ(shapeValue[0], shapeArrayGolden2[0]);
    EXPECT_EQ(shapeValue[1], shapeArrayGolden2[1]);
    EXPECT_EQ(shapeValue[2], shapeArrayGolden2[2]);
    // Test global set Shape
    GlobalTensor<half> globalTensor;
    globalTensor.SetShapeInfo(ShapeInfo(ASCENDC_SHAPE(3, 128, 128, 24)));
    ShapeInfo globalShape = globalTensor.GetShapeInfo();
    EXPECT_EQ(globalShape.shapeDim, 3);
    EXPECT_EQ(GetShapeSize(globalShape), 128 * 128 * 24);
}

TEST_F(TEST_UTILS, TEST_SUBINTEGER_INT4)
{
    int a = 5;
    AscendC::int4b_t val_pos = a;
    AscendC::int4b_t val_neg = -2;

    EXPECT_EQ((int)val_pos, 5);
    EXPECT_EQ((int)val_neg, -2);

    EXPECT_EQ(val_pos > (AscendC::int4b_t)0, true);
    EXPECT_EQ(val_pos > (AscendC::int4b_t)-1, true);
    EXPECT_EQ(val_pos >= (AscendC::int4b_t)0, true);
    EXPECT_EQ(val_pos >= (AscendC::int4b_t)-1, true);
    EXPECT_EQ(val_pos >= (AscendC::int4b_t)5, true);
    EXPECT_EQ(val_pos < (AscendC::int4b_t)7, true);
    EXPECT_EQ(val_pos <= (AscendC::int4b_t)7, true);

    EXPECT_EQ(val_neg < (AscendC::int4b_t)0, true);
    EXPECT_EQ(val_neg < (AscendC::int4b_t)-1, true);
    EXPECT_EQ(val_neg < (AscendC::int4b_t)1, true);
    EXPECT_EQ(val_neg > (AscendC::int4b_t)-3, true);
    EXPECT_EQ(val_neg <= (AscendC::int4b_t)0, true);
    EXPECT_EQ(val_neg <= (AscendC::int4b_t)-1, true);
    EXPECT_EQ(val_neg <= (AscendC::int4b_t)1, true);
    EXPECT_EQ(val_neg >= (AscendC::int4b_t)-2, true);

    EXPECT_EQ(val_pos == (AscendC::int4b_t)5, true);
    EXPECT_EQ(val_neg == (AscendC::int4b_t)-2, true);
}

TEST_F(TEST_UTILS, TEST_MC2_CONTEXT)
{
    __gm__ uint8_t* context1 = new uint8_t[16];
    __gm__ uint8_t* context2 = new uint8_t[16];
    SetHcclContext<0>(context1);
    SetHcclContext<1>(context2);
    SetHcclContext<2>(context2);
    auto ret0 = GetHcclContext<0>();
    auto ret1 = GetHcclContext<1>();
    auto ret2 = GetHcclContext<2>();
    EXPECT_TRUE(g_hcclContextReserved[0] == context1);
    EXPECT_TRUE(g_hcclContextReserved[1] == context2);
    EXPECT_TRUE(ret0 == context1);
    EXPECT_TRUE(ret1 == context2);
    EXPECT_TRUE(ret2 == nullptr);

    delete[] context1;
    delete[] context2;
}

namespace AscendC{
uint64_t get_imm(uint64_t arg0) {
    return 0;
}

int64_t get_status() {
    return 0;
}
}

TEST_F(TEST_UTILS, TEST_WriteBackOverflow)
{
#if (__CCE_AICORE__ <= 200)
    MOCKER_CPP(&AscendC::get_status, int64_t()).expects(exactly(2)).will(returnValue(0x520));
#else
    MOCKER_CPP(&AscendC::get_status, int64_t()).expects(never()).will(returnValue(0x520));
#endif
    uint8_t overflow[8] = {0};
    uint64_t tmp[4] = {0};
    MOCKER_CPP(AscendC::get_imm, uint64_t(uint64_t)).stubs().will(returnValue((uint64_t)tmp));
    MOCKER_CPP(AscendC::copy_ubuf_to_gm, void(void*, void*, unsigned char, unsigned short, unsigned short, unsigned short, unsigned short)).stubs();
    WriteBackOverflow(overflow);

    uint64_t tmp0[4] = {0, 0, 0x520, 0};
    MOCKER_CPP(AscendC::get_imm, uint64_t(uint64_t)).stubs().will(returnValue((uint64_t)tmp));
    WriteBackOverflow(overflow);
    EXPECT_EQ(tmp[3], 0);
}