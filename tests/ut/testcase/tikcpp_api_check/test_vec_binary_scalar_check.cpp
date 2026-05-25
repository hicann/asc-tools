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

class TestBinaryScalarCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {
        AscendC::CheckSyncState();
    }
};


struct TestBinaryScalarApiCheckParams {
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

class TestBinaryScalarpiCheckSuite : public testing::Test,
    public testing::WithParamInterface<TestBinaryScalarApiCheckParams> {
protected:
    void SetUp() {
        g_coreType = AIV_TYPE;
    }
    void TearDown() {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

class TestBinaryScalarApiHighCheckSuite : public testing::Test,
    public testing::WithParamInterface<TestBinaryScalarApiCheckParams> {
protected:
    void SetUp() {
        g_coreType = AIV_TYPE;
    }
    void TearDown() {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(TEST_BINARY_SCALAR_API_CHECK, TestBinaryScalarpiCheckSuite,
    ::testing::Values(TestBinaryScalarApiCheckParams { 256, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryScalarApiCheckParams { 256, TPosition::A1, 1, 1, 8, 8, 1, 256, false },
    TestBinaryScalarApiCheckParams { 24 * 1024 + 1, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryScalarApiCheckParams { 128, TPosition::VECCALC, 1, 1, 8, 8, 4, 256, false }));

INSTANTIATE_TEST_CASE_P(TEST_BINARY_SCALAR_API_HIGH_CHECK, TestBinaryScalarApiHighCheckSuite,
    ::testing::Values(TestBinaryScalarApiCheckParams { 512, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryScalarApiCheckParams { 512, TPosition::B1, 1, 1, 8, 8, 1, 256, false },
    TestBinaryScalarApiCheckParams { 24 * 1024, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryScalarApiCheckParams { 128, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, false }));

TEST_P(TestBinaryScalarpiCheckSuite, BiScalarApiCheckLowLevel)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint8_t repeatTimes = param.repeat;
    uint64_t maskFull = 0xffffffffffffffff;
    check::VecBinaryScalarApiParams chkParams { output.addr,
        input0.addr,
        repeatTimes,
        param.dstBlkStride,
        param.srcBlkStride,
        param.dstRptStride,
        param.srcRptStride,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        LogicPos(output),
        LogicPos(input0) };
    check::TikcppVecBinaryScalarCheck chkIns { "test_intri", chkParams };
    MaskSetter::Instance().SetMask(true);
    bool flag = chkIns.CheckAllLowLevel({ maskFull, maskFull });
    EXPECT_EQ(flag, param.expect);
}

TEST_P(TestBinaryScalarApiHighCheckSuite, BiScalarApiCheckHighLevel)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    check::VecBinaryScalarApiParams chkParams { output.addr,
        input0.addr,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        LogicPos(output),
        LogicPos(input0),
        (uint32_t)(param.calSize) };
    check::TikcppVecBinaryScalarCheck chkIns { "test_intri", chkParams };
    bool flag = chkIns.CheckAllHighLevel();
    EXPECT_EQ(flag, param.expect);
}
