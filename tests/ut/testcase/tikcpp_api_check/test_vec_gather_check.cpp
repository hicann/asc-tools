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

struct TestVecGatherCheckParams {
    uint32_t dataSize;
    TPosition pos;
    uint16_t dstBlkStride;
    uint16_t dstRptStride;
    uint8_t repeat;
    uint32_t offsetLen;
    bool expect;
};

class TestVecGatherCheckSuite : public testing::Test, public testing::WithParamInterface<TestVecGatherCheckParams> {
protected:
    void SetUp() { g_coreType = AIV_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(
    TEST_VEC_GATHER_CHECK, TestVecGatherCheckSuite,
    ::testing::Values(
        TestVecGatherCheckParams{256, TPosition::VECCALC, 1, 8, 1, 16, true},
        TestVecGatherCheckParams{256, TPosition::A1, 1, 8, 1, 16, false},
        TestVecGatherCheckParams{32 * 1024 + 1, TPosition::VECCALC, 1, 8, 1, 16, true},
        TestVecGatherCheckParams{128, TPosition::VECCALC, 1, 8, 4, 64, false},
        TestVecGatherCheckParams{1024, TPosition::VECCALC, 1, 8, 8, 32, false}));

TEST_P(TestVecGatherCheckSuite, TestCaseGatherb)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    uint32_t offsetLen = param.offsetLen;

    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto offset = MakeTensor(param.pos, offsetLen * sizeof(uint32_t));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint16_t dstBlockStride = param.dstBlkStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecGatherApiParams chkParams{
        output.addr,
        input0.addr,
        offset.addr,
        repeatTimes,
        (uint16_t)(dstBlockStride),
        (uint16_t)(dstRepeatStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint32_t)),
        output.length,
        input0.length,
        offset.length,
        LogicPos(output),
        LogicPos(input0),
        LogicPos(offset)};
    check::TikcppVecGatherbCheck chkIns{"vgatherb", chkParams};
    uint64_t mask = 128;
    MaskSetter::Instance().SetMask(true);
    bool flag = chkIns.CheckAllLowLevel({mask});
    EXPECT_EQ(flag, param.expect);
}
