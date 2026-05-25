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

    auto input = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));
    auto output = MakeTensor(param.pos, ALIGN_ADDR(dataSize * sizeof(uint16_t)));

    uint8_t repeatTimes = param.repeat;
    uint64_t mask = 128;
    uint64_t maskFull = 0xffffffffffffffff;
    check::CopyApiParams chkParams { output.addr,
        input.addr,
        repeatTimes,
        param.dstBlkStride,
        param.srcBlkStride,
        param.dstRptStride,
        param.srcRptStride,
        (uint32_t)(sizeof(uint16_t)),
        (uint32_t)(sizeof(uint16_t)),
        output.length,
        input.length,
        LogicPos(output),
        LogicPos(input) };
    check::TikcppCopyCheck chkIns { "vcopy", chkParams };
    uint64_t mask2[2] = {maskFull, maskFull};
    MaskSetter::Instance().SetMask(true);
    bool flag2 = CheckFuncCopyImplForMaskArray(chkParams, mask2, "vcopy");
    bool flag1 = CheckFuncCopyImpl(chkParams, mask, "vcopy");

    EXPECT_EQ(flag1 && flag2, param.expect);
}
