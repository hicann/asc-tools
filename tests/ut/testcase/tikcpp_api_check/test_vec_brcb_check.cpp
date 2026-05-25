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
    auto input0 = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dstLen * sizeof(uint16_t)));

    uint16_t dstBlockStride = param.dstBlkStride;
    uint8_t repeatTimes = param.repeat;
    uint8_t dstRepeatStride = param.dstRptStride;
    check::VecBroadCastApiParams chkParams { output.addr,
        input0.addr,
        repeatTimes,
        (uint16_t)(dstBlockStride),
        (uint16_t)(dstRepeatStride),
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input0.length,
        LogicPos(output),
        LogicPos(input0) };
    check::TikcppVecBroadCastCheck chkIns { "vbrcb", chkParams };
    uint64_t mask = 128;
    bool flag = chkIns.CheckAllLowLevel({ mask });
    EXPECT_EQ(flag, param.expect);
}
