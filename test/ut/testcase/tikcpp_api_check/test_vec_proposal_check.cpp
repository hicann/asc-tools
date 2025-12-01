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
#include "kernel_operator.h"
#include "api_check/kernel_cpu_check.h"
#include "test_utils.h"

using namespace std;
using namespace AscendC;

struct TestVecProposalCheckParams {
    uint32_t dataSize;
    uint32_t dstSize;
    TPosition pos;
    uint8_t repeat;
    bool expect;
    std::string apiName;
};

class TestVecProposalCheckSuite : public testing::Test, public testing::WithParamInterface<TestVecProposalCheckParams> {
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

INSTANTIATE_TEST_CASE_P(TEST_VEC_PROPOSAL_CHECK, TestVecProposalCheckSuite,
    ::testing::Values(
    TestVecProposalCheckParams { 32, 256, TPosition::VECCALC, 2, true, "ProposalConcat" },
    TestVecProposalCheckParams { 32, 256, TPosition::A1, 2, false, "ProposalConcat" },
    TestVecProposalCheckParams { 32, 32 * 1024 + 1, TPosition::VECCALC, 2, true, "ProposalConcat" },
    TestVecProposalCheckParams { 32, 128, TPosition::VECCALC, 2, false, "ProposalConcat" },
    TestVecProposalCheckParams { 128, 16, TPosition::VECCALC, 1, true, "ProposalExtract" },
    TestVecProposalCheckParams { 128, 16, TPosition::A1, 1, false, "ProposalExtract" },
    TestVecProposalCheckParams { 128, 16, TPosition::VECCALC, 2, false, "ProposalExtract" },

    TestVecProposalCheckParams { 128, 128, TPosition::VECCALC, 1, true, "RpSort16" },
    TestVecProposalCheckParams { 128, 128, TPosition::A1, 1, false, "RpSort16" },        // pos不对
    TestVecProposalCheckParams { 128, 128, TPosition::VECCALC, 2, false, "RpSort16" },   // src够大，dst不够大
    TestVecProposalCheckParams { 128, 256, TPosition::VECCALC, 2, false, "RpSort16" },   // dst够大，src不够大

    TestVecProposalCheckParams { 64, 128, TPosition::VECCALC, 2, true, "Sort32" },
    TestVecProposalCheckParams { 64, 128, TPosition::A2, 2, false, "Sort32" },           // pos不对
    TestVecProposalCheckParams { 64, 128, TPosition::VECCALC, 4, false, "Sort32" },      // dst不够大
    TestVecProposalCheckParams { 64, 256, TPosition::VECCALC, 4, false, "Sort32" },      // src0 + src1不够大

    TestVecProposalCheckParams { 256, 1024, TPosition::VECCALC, 1, true, "MrgSort4" },
    TestVecProposalCheckParams { 256, 1024, TPosition::A1, 1, false, "MrgSort4" },
    TestVecProposalCheckParams { 128, 1024, TPosition::A1, 1, false, "MrgSort4" },
    TestVecProposalCheckParams { 256, 512, TPosition::VECCALC, 1, false, "MrgSort4" },
    TestVecProposalCheckParams { 256, 128, TPosition::VECCALC, 1, false, "MrgSort" }));

TEST_P(TestVecProposalCheckSuite, TestCaseGatherb)
{
    auto param = GetParam();
    uint32_t dataSize = param.dataSize;

    LocalTensor<float> input0_local;
    LocalTensor<int32_t> input1_local;
    LocalTensor<float> output_local;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, dataSize * sizeof(float));
        input0_local = tbuf.Get<float>();

        TBuf<TPosition::VECCALC> tbuf1;
        tpipe.InitBuffer(tbuf1, dataSize * sizeof(int32_t));
        input1_local = tbuf1.Get<int32_t>();

        TBuf<TPosition::VECCALC> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(param.dstSize * sizeof(float)));
        output_local = tbuf2.Get<float>();
    } else if (param.pos == TPosition::A1) {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, dataSize * sizeof(float));
        input0_local = tbuf.Get<float>();

        TBuf<TPosition::A1> tbuf1;
        tpipe.InitBuffer(tbuf1, dataSize * sizeof(int32_t));
        input1_local = tbuf1.Get<int32_t>();

        TBuf<TPosition::A1> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(param.dstSize * sizeof(float)));
        output_local = tbuf2.Get<float>();
    } else {
        TBuf<TPosition::A2> tbuf;
        tpipe.InitBuffer(tbuf, dataSize * sizeof(float));
        input0_local = tbuf.Get<float>();

        TBuf<TPosition::A2> tbuf1;
        tpipe.InitBuffer(tbuf1, dataSize * sizeof(int32_t));
        input1_local = tbuf1.Get<int32_t>();

        TBuf<TPosition::A2> tbuf2;
        tpipe.InitBuffer(tbuf2, ALIGN_ADDR(param.dstSize * sizeof(float)));
        output_local = tbuf2.Get<float>();
    }
    uint8_t repeatTimes = param.repeat;
    if (param.apiName != "MrgSort4" && param.apiName != "MrgSort") {
        check::VecProposalApiParams chkParams { (uint64_t)output_local.GetPhyAddr(),
            (uint64_t)input0_local.GetPhyAddr(),
            (uint64_t)input1_local.GetPhyAddr(),
            repeatTimes,
            (uint32_t)(sizeof(float)),
            (uint32_t)(sizeof(float)),
            (uint32_t)(sizeof(uint32_t)),
            (uint64_t)(output_local.GetLength()),
            (uint64_t)(input0_local.GetLength()),
            (uint64_t)(input1_local.GetLength()),
            (uint8_t)(output_local.GetPosition()),
            (uint8_t)(input0_local.GetPosition()),
            (uint8_t)(input1_local.GetPosition()) };
        check::TikcppVecProposalCheck chkIns { param.apiName, chkParams };
        uint64_t mask = 128;
        bool flag = chkIns.CheckAllHighLevel();
        EXPECT_EQ(flag, param.expect);
    } else {
        uint16_t element[4] = {32, 32, 32, 32};
        check::VecProposalApiParams chkParams { (uint64_t)output_local.GetPhyAddr(),
            (uint64_t)input0_local.GetPhyAddr(),
            repeatTimes,
            (uint32_t)(sizeof(float)),
            (uint32_t)(sizeof(float)),
            (uint64_t)(output_local.GetLength()),
            (uint64_t)(input0_local.GetLength()),
            (uint8_t)(output_local.GetPosition()),
            (uint8_t)(input0_local.GetPosition()),
            (uint16_t)(0xf),
            element,
            0 };
        check::TikcppVecProposalCheck chkIns { param.apiName, chkParams };
        bool flag = chkIns.CheckAllHighLevel();
        EXPECT_EQ(flag, param.expect);
    }
}
