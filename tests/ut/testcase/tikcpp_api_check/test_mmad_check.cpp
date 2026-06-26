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

class TestMmadCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() { AscendC::CheckSyncState(); }
};

struct TestMmadApiCheckParams {
    uint16_t m;
    uint16_t n;
    uint16_t k;
    bool expect;
};

class TestMmadApiCheckSuite : public testing::Test, public testing::WithParamInterface<TestMmadApiCheckParams> {
protected:
    void SetUp() { g_coreType = AIC_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(
    TEST_MMAD_API_CHECK, TestMmadApiCheckSuite,
    ::testing::Values(
        TestMmadApiCheckParams{10, 10, 10, true}, TestMmadApiCheckParams{32, 128, 128, true},
        TestMmadApiCheckParams{128, 128, 128, true}));

TEST_P(TestMmadApiCheckSuite, MmadApiCheckHighLevel)
{
    auto param = GetParam();
    uint32_t dstDataSizeIn = static_cast<uint32_t>(param.m * param.n);
    uint32_t src0DataSizeIn = static_cast<uint32_t>(param.m * param.k);
    uint32_t src1DataSizeIn = static_cast<uint32_t>(param.n * param.k);

    auto dst = MakeTensor(TPosition::CO1, ALIGN_ADDR(dstDataSizeIn * sizeof(half)));
    auto fm = MakeTensor(TPosition::A2, ALIGN_ADDR(src0DataSizeIn * sizeof(half)));
    auto filter = MakeTensor(TPosition::B2, ALIGN_ADDR(src1DataSizeIn * sizeof(half)));

    check::MmadApiParams chkParams{
        dst.addr,
        fm.addr,
        filter.addr,
        (uint32_t)(sizeof(half)),
        (uint32_t)(sizeof(half)),
        (uint32_t)(sizeof(half)),
        dst.length,
        fm.length,
        filter.length,
        LogicPos(dst),
        LogicPos(fm),
        LogicPos(filter),
        param.m,
        param.n,
        param.k,
        false,
        0,
        false,
        false,
        false};
    check::TikcppMmadCheck chkIns{"mmad", chkParams};
    bool flag = chkIns.CheckAllHighLevel();
    EXPECT_EQ(flag, param.expect);
}
