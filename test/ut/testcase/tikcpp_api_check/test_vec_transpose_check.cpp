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
#include "kernel_operator.h"
#include "api_check/kernel_cpu_check.h"
#include "test_utils.h"

using namespace std;
using namespace AscendC;

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
    void SetUp() {
        g_coreType = AIV_TYPE;
    }
    void TearDown() {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
public:
    TPipe tpipe;
};

INSTANTIATE_TEST_CASE_P(TEST_VEC_TRANSPOSE_CHECK, TestVecTransposeCheckSuite,
    ::testing::Values(
    TestVecTransposeCheckParams { 256, TPosition::VECCALC, 16, 16, 1, true },
    TestVecTransposeCheckParams { 256, TPosition::A1, 16, 16, 1, false },                                          // position错误
    TestVecTransposeCheckParams { 32 * 1024 + 1, TPosition::VECCALC, 16, 16, 1, true },
    TestVecTransposeCheckParams { 200, TPosition::VECCALC, 16, 16, 1, false, TransposeType::TRANSPOSE_ND2ND_B16 },   // tensor needs to be 512B at least
    TestVecTransposeCheckParams { 200, TPosition::VECCALC, 16, 16, 1, true}                                         // 因为为type_none，所以不用校验
));

TEST_P(TestVecTransposeCheckSuite, TestCaseVecTranspose)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;

    LocalTensor<uint16_t> input0_local;
    LocalTensor<uint16_t> output_local;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0_local = tbuf.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output_local = tbuf1.Get<uint16_t>();
    } else {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0_local = tbuf.Get<uint16_t>();

        TBuf<TPosition::A1> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output_local = tbuf1.Get<uint16_t>();
    }

    uint16_t srcRepeatStride = param.srcRptStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecTransposeApiParams chkParams { (uint64_t)output_local.GetPhyAddr(),
        (uint64_t)input0_local.GetPhyAddr(),
        repeatTimes,
        (uint16_t)(dstRepeatStride),
        (uint16_t)(srcRepeatStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output_local.GetLength()),
        (uint64_t)(input0_local.GetLength()),
        (uint8_t)(output_local.GetPosition()),
        (uint8_t)(input0_local.GetPosition()) };
    chkParams.transposeType = param.transposeType;
    check::TikcppVecTransposeCheck chkIns { "Transpose", chkParams };
    bool flag = chkIns.CheckAllLowLevel();
    EXPECT_EQ(flag, param.expect);
}
