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
#define private public
#define protected public
#include "api_check_test_utils.h"
#include "api_check/kernel_cpu_check.h"

using namespace std;
using namespace AscendC;
using AscToolsUt::LogicPos;
using AscToolsUt::MakeTensor;
struct TestReduceOtherApiCheckParams {
    TPosition pos;
    uint32_t srcDataSize;
    uint32_t dstDataSize;
    uint16_t dstRepStride;
    uint16_t srcBlkStride;
    uint16_t srcRepStride;
    uint32_t srcdtypeSize; // float or half
    uint32_t dstdtypeSize; // float or half
    string apiName;
    bool expect;
};

class TestVecReduceOtherCheckSuite : public testing::Test,
    public testing::WithParamInterface<TestReduceOtherApiCheckParams> {
protected:
    void SetUp() {
        g_coreType = AIV_TYPE;
    }
    void TearDown() {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(TEST_VEC_REDUCE_OTHER_CHECK, TestVecReduceOtherCheckSuite,
    ::testing::Values(
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 16, 1, 1, 8, 2, 2, "WholeReduceMax", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 16, 1, 1, 8, 2, 2, "WholeReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 16, 1, 1, 8, 2, 2, "WholeReduceSum", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 16, 8, 1, 8, 2, 2, "BlockReduceMax", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 16, 8, 1, 8, 2, 2, "BlockReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 16, 8, 1, 8, 2, 2, "BlockReduceSum", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 32769, 4, 8, 1, 8, 2, 2, "PairReduceSum", false},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 64, 32769, 1, 1, 8, 2, 2, "WholeReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 16, 1, 1, 8, 2, 4, "WholeReduceSum", false},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 4, 1, 1, 8, 2, 2, "WholeReduceMax", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 4, 1, 1, 8, 2, 2, "WholeReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 3, 1, 1, 8, 2, 2, "WholeReduceMax", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 2, 1, 1, 8, 2, 2, "WholeReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 256, 2, 1, 1, 8, 2, 2, "WholeReduceSum", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 8, 8, 1, 8, 2, 2, "BlockReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 7, 8, 1, 8, 2, 2, "BlockReduceMin", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 64, 8, 1, 8, 2, 2, "PairReduceSum", true},
        TestReduceOtherApiCheckParams{TPosition::VECCALC, 128, 16, 8, 1, 8, 2, 2, "PairReduceSum", false}
    )
);

TEST_P(TestVecReduceOtherCheckSuite, TestCaseReduceOther)
{
    auto param = GetParam();
    uint32_t srcDataSizeIn = param.srcDataSize;
    uint32_t dstDataSizeIn = param.dstDataSize;

    auto src = MakeTensor(param.pos, ALIGN_ADDR(srcDataSizeIn * sizeof(half)));
    auto dst = MakeTensor(param.pos, ALIGN_ADDR(dstDataSizeIn * sizeof(half)));

    int32_t mask = 128;
    uint8_t repeatTimes = srcDataSizeIn / mask;
    uint32_t srcdtypeSizeIn = param.srcdtypeSize;
    uint32_t dstdtypeSizeIn = param.dstdtypeSize;

    check::VecReduceApiParams chkParams { dst.addr,
        src.addr,
        (uint32_t)(dstdtypeSizeIn),
        (uint32_t)(srcdtypeSizeIn),
        repeatTimes,
        (uint16_t)param.dstRepStride,
        (uint16_t)param.srcBlkStride,
        (uint16_t)param.srcRepStride,
        dst.length,
        src.length,
        LogicPos(dst),
        LogicPos(src) };
    check::TikcppVecReduceOtherCheck chkIns { param.apiName, chkParams };
    bool flag = chkIns.CheckAllLowLevel({ mask });
    EXPECT_EQ(flag, param.expect);
    return;
}
