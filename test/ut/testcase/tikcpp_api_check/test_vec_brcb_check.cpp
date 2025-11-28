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

struct TestVecBrcbCheckParams {
    uint32_t dataSize;
    TPosition pos;
    uint16_t dstBlkStride;
    uint16_t dstRptStride;
    uint8_t repeat;
    bool expect;
};

class TestVecBrcbCheckSuite : public testing::Test, public testing::WithParamInterface<TestVecBrcbCheckParams> {
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

INSTANTIATE_TEST_CASE_P(TEST_VEC_BRCB_CHECK, TestVecBrcbCheckSuite,
    ::testing::Values(TestVecBrcbCheckParams { 256, TPosition::VECCALC, 1, 8, 1, true },
    TestVecBrcbCheckParams { 256, TPosition::A1, 1, 8, 1, false },
    TestVecBrcbCheckParams { 4 * 1024 + 1, TPosition::VECCALC, 1, 8, 1, true },
    TestVecBrcbCheckParams { 16, TPosition::VECCALC, 1, 8, 3, false },
    TestVecBrcbCheckParams { 64, TPosition::VECCALC, 1, 8, 4, true }));

TEST_P(TestVecBrcbCheckSuite, TestCaseBrcb)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    uint32_t dstLen = dataSize * 16;
    LocalTensor<uint16_t> input0_local;
    LocalTensor<uint16_t> output_local;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0_local = tbuf.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dstLen * sizeof(uint16_t)));
        output_local = tbuf1.Get<uint16_t>();
    } else {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0_local = tbuf.Get<uint16_t>();

        TBuf<TPosition::A1> tbuf1;
        tpipe.InitBuffer(tbuf1, ALIGN_ADDR(dstLen * sizeof(uint16_t)));
        output_local = tbuf1.Get<uint16_t>();
    }

    uint16_t dstBlockStride = param.dstBlkStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecBroadCastApiParams chkParams { (uint64_t)output_local.GetPhyAddr(),
        (uint64_t)input0_local.GetPhyAddr(),
        repeatTimes,
        (uint16_t)(dstBlockStride),
        (uint16_t)(dstRepeatStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint64_t)(output_local.GetLength()),
        (uint64_t)(input0_local.GetLength()),
        (uint8_t)(output_local.GetPosition()),
        (uint8_t)(input0_local.GetPosition()) };
    check::TikcppVecBroadCastCheck chkIns { "vbrcb", chkParams };
    uint64_t mask = 128;
    bool flag = chkIns.CheckAllLowLevel({ mask });
    EXPECT_EQ(flag, param.expect);
}