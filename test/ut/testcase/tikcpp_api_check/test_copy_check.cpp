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

class TestCopyCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {
        AscendC::CheckSyncState();
    }
};

struct TestCopyApiCheckParams {
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

class TestCopyApiCheckSuite : public testing::Test, public testing::WithParamInterface<TestCopyApiCheckParams> {
protected:
    void SetUp() {
        g_coreType = AIV_TYPE;
    }
    void TearDown() {
        g_coreType = MIX_TYPE;
        AscendC::CheckSyncState();
    }
public:
    TPipe tpipe;
};

INSTANTIATE_TEST_CASE_P(TEST_COPY_API_CHECK, TestCopyApiCheckSuite,
    ::testing::Values(TestCopyApiCheckParams { 256, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestCopyApiCheckParams { 256, TPosition::A1, 1, 1, 8, 8, 1, 256, false },
    TestCopyApiCheckParams { 10 * 1024 + 1, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestCopyApiCheckParams { 128, TPosition::VECCALC, 1, 1, 8, 8, 4, 256, false }));

TEST_P(TestCopyApiCheckSuite, CopyApiCheckLowLevel)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;

    LocalTensor<uint16_t> input;
    LocalTensor<uint16_t> output;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input = tbuf.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output = tbuf1.Get<uint16_t>();
    } else {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input = tbuf.Get<uint16_t>();

        TBuf<TPosition::A1> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output = tbuf1.Get<uint16_t>();
    }

    CopyRepeatParams repeatParams { param.dstBlkStride, param.srcBlkStride, param.dstRptStride, param.srcRptStride };
    uint8_t repeatTimes = param.repeat;
    uint64_t mask = 128;
    uint64_t maskFull = 0xffffffffffffffff;
    check::CopyApiParams chkParams { (uint64_t)output.GetPhyAddr(),
        (uint64_t)input.GetPhyAddr(),
        repeatTimes,
        (uint16_t)(repeatParams.dstStride),
        (uint16_t)(repeatParams.srcStride),
        (uint16_t)(repeatParams.dstRepeatSize),
        (uint16_t)(repeatParams.srcRepeatSize),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output.GetLength()),
        (uint64_t)(input.GetLength()),
        (uint8_t)(output.GetPosition()),
        (uint8_t)(input.GetPosition()) };
    check::TikcppCopyCheck chkIns { "vcopy", chkParams };
    uint64_t mask2[2] = {maskFull, maskFull};
    MaskSetter::Instance().SetMask(true);
    bool flag2 = CheckFuncCopyImplForMaskArray(chkParams, mask2, "vcopy");
    bool flag1 = CheckFuncCopyImpl(chkParams, mask, "vcopy");

    EXPECT_EQ(flag1 && flag2, param.expect);
}