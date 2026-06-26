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

struct TestVecTransposeCheckParams {
    uint32_t dataSize;
    TPosition pos;
    uint16_t srcRptStride;
    uint16_t dstRptStride;
    uint8_t repeat;
    bool expect;
    TransposeType transposeType = TransposeType::TRANSPOSE_TYPE_NONE;
};

class TestVecTransposeCheckSuite : public testing::Test,
                                   public testing::WithParamInterface<TestVecTransposeCheckParams> {
protected:
    void SetUp() { g_coreType = AIV_TYPE; }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(
    TEST_VEC_TRANSPOSE_CHECK, TestVecTransposeCheckSuite,
    ::testing::Values(
        TestVecTransposeCheckParams{256, TPosition::VECCALC, 16, 16, 1, true},
        TestVecTransposeCheckParams{256, TPosition::A1, 16, 16, 1, false}, // position错误
        TestVecTransposeCheckParams{32 * 1024 + 1, TPosition::VECCALC, 16, 16, 1, true},
        TestVecTransposeCheckParams{
            200, TPosition::VECCALC, 16, 16, 1, false,
            TransposeType::TRANSPOSE_ND2ND_B16},                              // tensor needs to be 512B at least
        TestVecTransposeCheckParams{200, TPosition::VECCALC, 16, 16, 1, true} // 因为为type_none，所以不用校验
        ));

TEST_P(TestVecTransposeCheckSuite, TestCaseVecTranspose)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;

    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint16_t srcRepeatStride = param.srcRptStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecTransposeApiParams chkParams{
        output.addr,
        input0.addr,
        repeatTimes,
        (uint16_t)(dstRepeatStride),
        (uint16_t)(srcRepeatStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        LogicPos(output),
        LogicPos(input0)};
    chkParams.transposeType = param.transposeType;
    check::TikcppVecTransposeCheck chkIns{"Transpose", chkParams};
    bool flag = chkIns.CheckAllLowLevel();
    EXPECT_EQ(flag, param.expect);
}
