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

class TestBroadCastToMMCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() { AscendC::CheckSyncState(); }
};

struct TestBroadCastToMMApiCheckParams {
    uint64_t srcSize_ele;
    uint64_t dstSize_ele;
    TPosition srcpos;
    TPosition dstpos;
    uint32_t blockCount;
    uint8_t blockLen;
    uint8_t srcGap;
    uint8_t dstGap;
    bool expect;
};

class TestBroadCastToMMApiCheckSuite : public testing::Test,
                                       public testing::WithParamInterface<TestBroadCastToMMApiCheckParams> {
protected:
    void SetUp() { g_coreType = AIV_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(
    TEST_BROADCAST_TO_MM_CHECK, TestBroadCastToMMApiCheckSuite,
    ::testing::Values(
        TestBroadCastToMMApiCheckParams{16, 256, TPosition::VECCALC, TPosition::CO1, 1, 1, 0, 0, true},
        TestBroadCastToMMApiCheckParams{16, 256, TPosition::A1, TPosition::CO1, 1, 1, 0, 0, false},
        TestBroadCastToMMApiCheckParams{16, 256, TPosition::VECCALC, TPosition::B1, 1, 1, 0, 0, false},
        TestBroadCastToMMApiCheckParams{32769, 64, TPosition::VECCALC, TPosition::CO1, 1, 1, 0, 0, true},
        TestBroadCastToMMApiCheckParams{4, 32769, TPosition::VECCALC, TPosition::CO1, 1, 1, 0, 0, true}));

TEST_P(TestBroadCastToMMApiCheckSuite, BroadCastToMMApiCheckAllHighLevel)
{
    auto param = GetParam();
    uint64_t srcSize_ele = param.srcSize_ele;
    uint64_t dstSize_ele = param.dstSize_ele;

    auto input = MakeTensor(param.srcpos, ALIGN_ADDR(srcSize_ele * sizeof(half)));
    auto output = MakeTensor(param.dstpos, ALIGN_ADDR(dstSize_ele * sizeof(half)));

    uint32_t blockCount = param.blockCount;
    uint8_t blockLen = param.blockLen;
    uint8_t srcGap = param.srcGap;
    uint8_t dstGap = param.dstGap;

    check::VecBroadCastToMMApiParams chkParams{
        output.addr,
        input.addr,
        (uint32_t)(sizeof(half)),
        (uint32_t)(sizeof(half)),
        output.length,
        input.length,
        LogicPos(output),
        LogicPos(input),
        blockCount,
        blockLen,
        srcGap,
        dstGap};
    check::TikcppBroadCastToMMCheck chkIns{"broadcast_to_mm", chkParams};
    bool flag = CheckFuncBroadCastToMMImpl(chkParams, "broadcast_to_mm");

    EXPECT_EQ(flag, param.expect);
}
