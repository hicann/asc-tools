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
#include "api_check_test_utils.h"
#include "api_check/kernel_cpu_check.h"

using namespace std;
using namespace AscendC;
using AscToolsUt::LogicPos;
using AscToolsUt::MakeTensor;

struct ReduceCheckParams {
    TPosition pos;
    uint32_t srcDataSize;
    uint32_t dstDataSize;
    uint32_t workDataSize;
    uint32_t dtypeSize; // float or half
    uint32_t level;     // level 0 or level2
    string apiName;
    bool callIndex;
    bool expect;
    bool isReduce220 = false;
};

class TestVecReduceCheckSuite : public testing::Test, public testing::WithParamInterface<ReduceCheckParams> {
protected:
    void SetUp() {
        g_coreType = AIV_TYPE;
    }
    void TearDown() {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(TEST_VEC_REDUCE_CHECK, TestVecReduceCheckSuite,
    ::testing::Values(
        ReduceCheckParams { TPosition::VECCALC, 8320, 16, 64, 2, 0, "ReduceSum", false, false},
        ReduceCheckParams { TPosition::VECCALC, 8320, 16, 80, 2, 0, "ReduceSum", false, true},
        ReduceCheckParams { TPosition::VECCALC, 288, 16, 2, 2, 2, "ReduceSum", false, true},
        ReduceCheckParams { TPosition::VECCALC, 288, 16, 2, 2, 0, "ReduceSum", false, false, true},
        ReduceCheckParams { TPosition::VECCALC, 512, 16, 17, 2, 0, "ReduceMax", true, true},
        ReduceCheckParams { TPosition::VECCALC, 512, 16, 18, 2, 0, "ReduceMax", true, true},
        ReduceCheckParams { TPosition::VECCALC, 288, 16, 17, 2, 2, "ReduceMin", true, false},
        ReduceCheckParams { TPosition::VECCALC, 288, 16, 18, 2, 2, "ReduceMin", true, false},
        ReduceCheckParams { TPosition::VECCALC, 288, 2, 18, 2, 2, "ReduceMin", true, false},
        ReduceCheckParams { TPosition::VECCALC, 288, 1, 18, 2, 2, "ReduceMax", false, false},
        ReduceCheckParams { TPosition::VECCALC, 288, 1, 18, 2, 2, "ReduceMin", true, false}
    )
);

TEST_P(TestVecReduceCheckSuite, TestCaseReduce)
{
    auto param = GetParam();
    uint32_t srcDataSizeIn = param.srcDataSize;
    uint32_t workDataSizeIn = param.workDataSize;
    uint32_t dstDataSizeIn = param.dstDataSize;

    auto src = MakeTensor(param.pos, ALIGN_ADDR(srcDataSizeIn * sizeof(half)));
    auto work = MakeTensor(param.pos, ALIGN_ADDR(workDataSizeIn * sizeof(half)));
    auto dst = MakeTensor(param.pos, ALIGN_ADDR(dstDataSizeIn * sizeof(half)));

    uint16_t srcRepStride = 8;
    int32_t mask = 128;
    uint8_t repeatTimes = srcDataSizeIn / mask;
    uint32_t calCount = srcDataSizeIn;
    bool calIndex = param.callIndex;
    if (param.apiName == "ReduceSum") {
        if (param.level == 0) {
            if (param.isReduce220 == false) {
                check::VecReduceApiParams chkParams { dst.addr,
                    src.addr,
                    work.addr,
                    (uint32_t)(param.dtypeSize),
                    (uint32_t)(param.dtypeSize),
                    (uint32_t)(param.dtypeSize),
                    repeatTimes,
                    dst.length,
                    src.length,
                    work.length,
                    LogicPos(dst),
                    LogicPos(src),
                    LogicPos(work),
                    (uint16_t)(srcRepStride) };
                check::TikcppVecReduceCheck chkIns { param.apiName, chkParams };
                bool flag = chkIns.CheckAllLowLevel({ mask });
                EXPECT_EQ(flag, param.expect);
            } else {
                check::VecReduceApiParams chkParams { dst.addr,
                    src.addr,
                    (uint32_t)(param.dtypeSize),
                    4,
                    1,
                    dst.length,
                    src.length,
                    LogicPos(dst),
                    LogicPos(src) };
                check::TikcppVecReduceCheck chkIns { param.apiName, chkParams };
                bool flag = chkIns.CheckAllHighLevelMode2();
                EXPECT_EQ(flag, param.expect);
            }
        } else {
            check::VecReduceApiParams chkParams { dst.addr,
                src.addr,
                work.addr,
                (uint32_t)(param.dtypeSize),
                (uint32_t)(param.dtypeSize),
                (uint32_t)(param.dtypeSize),
                repeatTimes,
                calCount,
                dst.length,
                src.length,
                work.length,
                LogicPos(dst),
                LogicPos(src),
                LogicPos(work) };
            check::TikcppVecReduceCheck chkIns { param.apiName, chkParams };
            bool flag = chkIns.CheckAllHighLevel();
            EXPECT_EQ(flag, param.expect);
        }
    } else {
        if (param.level = 0) {
            check::VecReduceApiParams chkParams { dst.addr,
                src.addr,
                work.addr,
                (uint32_t)(param.dtypeSize),
                (uint32_t)(param.dtypeSize),
                (uint32_t)(param.dtypeSize),
                repeatTimes,
                calIndex,
                dst.length,
                src.length,
                work.length,
                LogicPos(dst),
                LogicPos(src),
                LogicPos(work),
                (uint16_t)(srcRepStride) };
            check::TikcppVecReduceCheck chkIns { param.apiName, chkParams };
            bool flag = chkIns.CheckAllLowLevel({ mask });
            EXPECT_EQ(flag, param.expect);
        } else {
            check::VecReduceApiParams chkParams { dst.addr,
                src.addr,
                work.addr,
                (uint32_t)(param.dtypeSize),
                (uint32_t)(param.dtypeSize),
                (uint32_t)(param.dtypeSize),
                repeatTimes,
                512,
                calIndex,
                dst.length,
                src.length,
                work.length,
                LogicPos(dst),
                LogicPos(src),
                LogicPos(work) };
            check::TikcppVecReduceCheck chkIns { param.apiName, chkParams };
            bool flag = chkIns.CheckAllHighLevel();
            EXPECT_EQ(flag, param.expect);
        }
    }
    return;
}
