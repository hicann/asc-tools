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

class TestBinaryDupCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() { AscendC::CheckSyncState(); }
};

struct TestVecDupCheckParams {
    uint32_t dataSize;
    TPosition pos;
    uint16_t dstBlkStride;
    uint16_t dstRptStride;
    uint8_t repeat;
    uint32_t calSize;
    bool expect;
};

class TestVecDupCheckSuite : public testing::Test, public testing::WithParamInterface<TestVecDupCheckParams> {
protected:
    void SetUp() { g_coreType = AIV_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(
    TEST_VEC_DUP_CHECK, TestVecDupCheckSuite,
    ::testing::Values(
        TestVecDupCheckParams{256, TPosition::VECCALC, 1, 8, 1, 256, true},
        TestVecDupCheckParams{256, TPosition::A1, 1, 8, 1, 256, false},
        TestVecDupCheckParams{32 * 1024 + 1, TPosition::VECCALC, 1, 8, 1, 256, true},
        TestVecDupCheckParams{128, TPosition::VECCALC, 1, 8, 4, 256, false}));

TEST_P(TestVecDupCheckSuite, TestCaseDup)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint16_t dstBlockStride = param.dstBlkStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecDupApiParams chkParams{input0.addr,
                                     repeatTimes,
                                     (uint16_t)(dstBlockStride),
                                     (uint16_t)(dstRepeatStride),
                                     (uint32_t)(sizeof(uint16_t)),
                                     input0.length,
                                     LogicPos(input0)};
    check::VecDupApiParams chkParams1{
        input0.addr, (uint32_t)(sizeof(uint16_t)), input0.length, LogicPos(input0), (uint32_t)param.calSize};
    check::TikcppVecDupCheck chkIns{"vdup", chkParams};
    check::TikcppVecDupCheck chkIns1{"vdup", chkParams1};
    uint64_t mask = 128;
    MaskSetter::Instance().SetMask(true);
    bool flag = chkIns.CheckAllLowLevel({mask});
    EXPECT_EQ(flag, param.expect);
    flag = chkIns1.CheckAllHighLevel();
    EXPECT_EQ(flag, param.expect);
}
