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

class TestBinaryCheck : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {
        AscendC::CheckSyncState();
    }
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

class TestBinaryApiHighCheckSuite : public testing::Test, public testing::WithParamInterface<TestBinaryApiCheckParams> {
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

INSTANTIATE_TEST_CASE_P(TEST_BINARY_API_CHECK, TestBinaryApiCheckSuite,
    ::testing::Values(TestBinaryApiCheckParams { 512, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryApiCheckParams { 512, TPosition::A1, 1, 1, 8, 8, 1, 256, false },
    TestBinaryApiCheckParams { 24 * 1024 + 1, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryApiCheckParams { 128, TPosition::VECCALC, 1, 1, 8, 8, 4, 256, false }));

INSTANTIATE_TEST_CASE_P(TEST_BINARY_API_HIGH_CHECK, TestBinaryApiHighCheckSuite,
    ::testing::Values(TestBinaryApiCheckParams { 512, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryApiCheckParams { 512, TPosition::B1, 1, 1, 8, 8, 1, 256, false },
    TestBinaryApiCheckParams { 30 * 1024, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, true },
    TestBinaryApiCheckParams { 128, TPosition::VECCALC, 1, 1, 8, 8, 1, 256, false }));

TEST_P(TestBinaryApiCheckSuite, BiApiCheckLowLevel)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    LocalTensor<uint16_t> input0;
    LocalTensor<uint16_t> input1;
    LocalTensor<uint16_t> output;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0 = tbuf.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input1 = tbuf1.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output = tbuf2.Get<uint16_t>();
    } else {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0 = tbuf.Get<uint16_t>();

        TBuf<TPosition::A1> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input1 = tbuf1.Get<uint16_t>();

        TBuf<TPosition::A1> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output = tbuf2.Get<uint16_t>();
    }

    BinaryRepeatParams repeatParams { param.dstBlkStride, param.srcBlkStride, param.srcBlkStride,
        param.dstRptStride, param.srcRptStride, param.srcRptStride };
    uint8_t repeatTimes = param.repeat;
    uint64_t maskFull = 0xffffffffffffffff;
    check::VecBinaryApiParams chkParams { (uint64_t)output.GetPhyAddr(),
        (uint64_t)input0.GetPhyAddr(),
        (uint64_t)input1.GetPhyAddr(),
        repeatTimes,
        (uint16_t)(repeatParams.dstBlkStride),
        (uint16_t)(repeatParams.src0BlkStride),
        (uint16_t)(repeatParams.src1BlkStride),
        (uint16_t)(repeatParams.dstRepStride),
        (uint16_t)(repeatParams.src0RepStride),
        (uint16_t)(repeatParams.src1RepStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output.GetLength()),
        (uint64_t)(input0.GetLength()),
        (uint64_t)(input1.GetLength()),
        (uint8_t)(output.GetPosition()),
        (uint8_t)(input0.GetPosition()),
        (uint8_t)(input1.GetPosition()) };
    check::TikcppVecBinaryCheck chkIns { "test_intri", chkParams };
    MaskSetter::Instance().SetMask(true);
    bool flag = chkIns.CheckAllLowLevel({ maskFull, maskFull });
    EXPECT_EQ(flag, param.expect);
}

TEST_P(TestBinaryApiHighCheckSuite, BiApiCheckHighLevel)
{
    TPipe tpipe;
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    LocalTensor<uint16_t> input0;
    LocalTensor<uint16_t> input1;
    LocalTensor<uint16_t> output;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0 = tbuf.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input1 = tbuf1.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output = tbuf2.Get<uint16_t>();
    } else {
        TBuf<TPosition::B1> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0 = tbuf.Get<uint16_t>();

        TBuf<TPosition::B1> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input1 = tbuf1.Get<uint16_t>();

        TBuf<TPosition::B1> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output = tbuf2.Get<uint16_t>();
    }

    check::VecBinaryApiParams chkParams { (uint64_t)output.GetPhyAddr(),
        (uint64_t)input0.GetPhyAddr(),
        (uint64_t)input1.GetPhyAddr(),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output.GetLength()),
        (uint64_t)(input0.GetLength()),
        (uint64_t)(input1.GetLength()),
        (uint8_t)(output.GetPosition()),
        (uint8_t)(input0.GetPosition()),
        (uint8_t)(input1.GetPosition()),
        (uint32_t)(param.calSize) };
    check::TikcppVecBinaryCheck chkIns { "test_intri", chkParams };
    bool flag = chkIns.CheckAllHighLevel();
    EXPECT_EQ(flag, param.expect);
}

TEST_F(TestBinaryCheck, CmpCheckHighLevel)
{
    TPipe tpipe;
    uint32_t dataSize = 256;
    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    LocalTensor<uint16_t> input0 = tbuf.Get<uint16_t>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    LocalTensor<uint16_t> input1 = tbuf1.Get<uint16_t>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    LocalTensor<uint16_t> output = tbuf2.Get<uint16_t>();

    check::VecBinaryApiParams chkParams { (uint64_t)output.GetPhyAddr(),
        (uint64_t)input0.GetPhyAddr(),
        (uint64_t)input1.GetPhyAddr(),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output.GetLength()),
        (uint64_t)(input0.GetLength()),
        (uint64_t)(input1.GetLength()),
        (uint8_t)(output.GetPosition()),
        (uint8_t)(input0.GetPosition()),
        (uint8_t)(input1.GetPosition()),
        (uint32_t)(256) };
    check::TikcppVecBinaryCheck chkIns { "vcmp", chkParams };
    bool flag = chkIns.CheckAllHighLevel();
    EXPECT_EQ(flag, true);
}

TEST_F(TestBinaryCheck, CmpCheckLowLevel)
{
    TPipe tpipe;
    uint32_t dataSize = 256;
    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    LocalTensor<uint16_t> input0 = tbuf.Get<uint16_t>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    LocalTensor<uint16_t> input1 = tbuf1.Get<uint16_t>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    LocalTensor<uint16_t> output = tbuf2.Get<uint16_t>();

    BinaryRepeatParams repeatParams { 1, 1, 1, 8, 8, 8 };
    uint8_t repeatTimes = 1;
    uint64_t maskFull = 0xffffffffffffffff;
    check::VecBinaryApiParams chkParams { (uint64_t)output.GetPhyAddr(),
        (uint64_t)input0.GetPhyAddr(),
        (uint64_t)input1.GetPhyAddr(),
        repeatTimes,
        (uint16_t)(repeatParams.dstBlkStride),
        (uint16_t)(repeatParams.src0BlkStride),
        (uint16_t)(repeatParams.src1BlkStride),
        (uint16_t)(repeatParams.dstRepStride),
        (uint16_t)(repeatParams.src0RepStride),
        (uint16_t)(repeatParams.src1RepStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output.GetLength()),
        (uint64_t)(input0.GetLength()),
        (uint64_t)(input1.GetLength()),
        (uint8_t)(output.GetPosition()),
        (uint8_t)(input0.GetPosition()),
        (uint8_t)(input1.GetPosition()) };
    check::TikcppVecBinaryCheck chkIns { "cmp", chkParams };
    bool flag = chkIns.CheckAllLowLevel({ maskFull, maskFull });
    EXPECT_EQ(flag, true);
}
