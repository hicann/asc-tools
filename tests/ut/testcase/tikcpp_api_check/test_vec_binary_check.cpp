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
#define private public
#define protected public
#include "api_check_test_utils.h"
#include "api_check/kernel_cpu_check.h"

using namespace std;
using namespace AscendC;
using AscToolsUt::LogicPos;
using AscToolsUt::MakeTensor;

class TestBinaryCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() { AscendC::CheckSyncState(); }
};

struct TestBinaryApiCheckParams {
    uint32_t dataSize;
    TPosition pos;
    uint16_t dstBlkStride;
    uint16_t srcBlkStride;
    uint16_t dstRptStride;
    uint16_t srcRptStride;
    uint8_t repeat;
    uint32_t calSize;
    bool expect;
};

class TestBinaryApiCheckSuite : public testing::Test, public testing::WithParamInterface<TestBinaryApiCheckParams> {
protected:
    void SetUp() { g_coreType = AIV_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

class TestBinaryApiHighCheckSuite : public testing::Test, public testing::WithParamInterface<TestBinaryApiCheckParams> {
protected:
    void SetUp() { g_coreType = AIV_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(
    TEST_BINARY_API_CHECK, TestBinaryApiCheckSuite,
    ::testing::Values(
        TestBinaryApiCheckParams{512, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true},
        TestBinaryApiCheckParams{512, TPosition::A1, 1, 1, 8, 8, 1, 256, false},
        TestBinaryApiCheckParams{24 * 1024 + 1, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true},
        TestBinaryApiCheckParams{128, TPosition::VECCALC, 1, 1, 8, 8, 4, 256, false}));

INSTANTIATE_TEST_CASE_P(
    TEST_BINARY_API_HIGH_CHECK, TestBinaryApiHighCheckSuite,
    ::testing::Values(
        TestBinaryApiCheckParams{512, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true},
        TestBinaryApiCheckParams{512, TPosition::B1, 1, 1, 8, 8, 1, 256, false},
        TestBinaryApiCheckParams{30 * 1024, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true},
        TestBinaryApiCheckParams{128, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, false}));

TEST_P(TestBinaryApiCheckSuite, BiApiCheckLowLevel)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto input1 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint8_t repeatTimes = param.repeat;
    uint64_t maskFull = 0xffffffffffffffff;
    check::VecBinaryApiParams chkParams{
        output.addr,
        input0.addr,
        input1.addr,
        repeatTimes,
        param.dstBlkStride,
        param.srcBlkStride,
        param.srcBlkStride,
        param.dstRptStride,
        param.srcRptStride,
        param.srcRptStride,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        input1.length,
        LogicPos(output),
        LogicPos(input0),
        LogicPos(input1)};
    check::TikcppVecBinaryCheck chkIns{"test_intri", chkParams};
    MaskSetter::Instance().SetMask(true);
    bool flag = chkIns.CheckAllLowLevel({maskFull, maskFull});
    EXPECT_EQ(flag, param.expect);
}

TEST_P(TestBinaryApiHighCheckSuite, BiApiCheckHighLevel)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto input1 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    check::VecBinaryApiParams chkParams{
        output.addr,
        input0.addr,
        input1.addr,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        input1.length,
        LogicPos(output),
        LogicPos(input0),
        LogicPos(input1),
        (uint32_t)(param.calSize)};
    check::TikcppVecBinaryCheck chkIns{"test_intri", chkParams};
    bool flag = chkIns.CheckAllHighLevel();
    EXPECT_EQ(flag, param.expect);
}

TEST_F(TestBinaryCheck, CmpCheckHighLevel)
{
    uint32_t dataSize = 256;
    auto input0 = MakeTensor(TPosition::VECCALC, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto input1 = MakeTensor(TPosition::VECCALC, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(TPosition::VECCALC, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    check::VecBinaryApiParams chkParams{
        output.addr,
        input0.addr,
        input1.addr,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        input1.length,
        LogicPos(output),
        LogicPos(input0),
        LogicPos(input1),
        (uint32_t)(256)};
    check::TikcppVecBinaryCheck chkIns{"vcmp", chkParams};
    bool flag = chkIns.CheckAllHighLevel();
    EXPECT_EQ(flag, true);
}

TEST_F(TestBinaryCheck, CmpCheckLowLevel)
{
    uint32_t dataSize = 256;
    auto input0 = MakeTensor(TPosition::VECCALC, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto input1 = MakeTensor(TPosition::VECCALC, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(TPosition::VECCALC, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint8_t repeatTimes = 1;
    uint64_t maskFull = 0xffffffffffffffff;
    check::VecBinaryApiParams chkParams{
        output.addr,
        input0.addr,
        input1.addr,
        repeatTimes,
        1,
        1,
        1,
        8,
        8,
        8,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        input1.length,
        LogicPos(output),
        LogicPos(input0),
        LogicPos(input1)};
    check::TikcppVecBinaryCheck chkIns{"cmp", chkParams};
    bool flag = chkIns.CheckAllLowLevel({maskFull, maskFull});
    EXPECT_EQ(flag, true);
}
