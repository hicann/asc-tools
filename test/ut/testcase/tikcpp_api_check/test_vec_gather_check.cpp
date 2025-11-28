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

INSTANTIATE_TEST_CASE_P(TEST_VEC_GATHER_CHECK, TestVecGatherCheckSuite,
    ::testing::Values(TestVecGatherCheckParams { 256, TPosition::VECCALC, 1, 8, 1, 16, true },
    TestVecGatherCheckParams { 256, TPosition::A1, 1, 8, 1, 16, false },
    TestVecGatherCheckParams { 32 * 1024 + 1, TPosition::VECCALC, 1, 8, 1, 16, true },
    TestVecGatherCheckParams { 128, TPosition::VECCALC, 1, 8, 4, 64, false },
    TestVecGatherCheckParams { 1024, TPosition::VECCALC, 1, 8, 8, 32, false }));

TEST_P(TestVecGatherCheckSuite, TestCaseGatherb)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;
    uint32_t offsetLen = param.offsetLen;

    LocalTensor<uint16_t> input0_local;
    LocalTensor<uint32_t> offset_local;
    LocalTensor<uint16_t> output_local;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0_local = tbuf.Get<uint16_t>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, offsetLen * sizeof(uint32_t));
        offset_local = tbuf1.Get<uint32_t>();

        TBuf<TPosition::VECCALC> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output_local = tbuf2.Get<uint16_t>();
    } else {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        input0_local = tbuf.Get<uint16_t>();

        TBuf<TPosition::A1> tbuf1;
        tpipe.InitBuffer(tbuf1, offsetLen * sizeof(uint32_t));
        offset_local = tbuf1.Get<uint32_t>();

        TBuf<TPosition::A1> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
        output_local = tbuf2.Get<uint16_t>();
    }

    uint16_t dstBlockStride = param.dstBlkStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecGatherApiParams chkParams { (uint64_t)output_local.GetPhyAddr(),
        (uint64_t)input0_local.GetPhyAddr(),
        (uint64_t)offset_local.GetPhyAddr(),
        repeatTimes,
        (uint16_t)(dstBlockStride),
        (uint16_t)(dstRepeatStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint32_t)),
        (uint64_t)(output_local.GetLength()),
        (uint64_t)(input0_local.GetLength()),
        (uint64_t)(offset_local.GetLength()),
        (uint8_t)(output_local.GetPosition()),
        (uint8_t)(input0_local.GetPosition()),
        (uint8_t)(offset_local.GetPosition()) };
    check::TikcppVecGatherbCheck chkIns { "vgatherb", chkParams };
    uint64_t mask = 128;
    MaskSetter::Instance().SetMask(true);
    bool flag = chkIns.CheckAllLowLevel({ mask });
    EXPECT_EQ(flag, param.expect);
}