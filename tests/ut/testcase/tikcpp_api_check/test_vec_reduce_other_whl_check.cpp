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

using namespace AscendC;

struct TestReduceOtherWhlApiCheckParams {
    ReduceOrder order;
    uint32_t dstDataSize;
    bool expect;
};

class TestVecReduceOtherWhlCheckSuite : public testing::Test,
    public testing::WithParamInterface<TestReduceOtherWhlApiCheckParams> {
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

namespace {
check::VecReduceWhlApiParams BuildReduceWhlParams(uint64_t dstAddr, uint64_t srcAddr, uint32_t dstDtypeSize,
    uint32_t srcDtypeSize, uint8_t repeatTimes, uint16_t dstRepStride, uint16_t srcBlkStride, uint16_t srcRepStride,
    ReduceOrder order, uint64_t dstSize, uint64_t srcSize, uint8_t dstPos, uint8_t srcPos)
{
    return check::VecReduceWhlApiParams {dstAddr, srcAddr, dstDtypeSize, srcDtypeSize, repeatTimes, dstRepStride,
        srcBlkStride, srcRepStride, order, dstSize, srcSize, dstPos, srcPos};
}
}

INSTANTIATE_TEST_CASE_P(TEST_VEC_REDUCE_OTHER_WHL_CHECK, TestVecReduceOtherWhlCheckSuite,
    ::testing::Values(
        TestReduceOtherWhlApiCheckParams{ReduceOrder::ORDER_VALUE_INDEX, 32, true},
        TestReduceOtherWhlApiCheckParams{ReduceOrder::ORDER_INDEX_VALUE, 32, true},
        TestReduceOtherWhlApiCheckParams{ReduceOrder::ORDER_VALUE_INDEX, 30, false}
    )
);

TEST_P(TestVecReduceOtherWhlCheckSuite, TestCaseWholeReduceOther)
{
    auto param = GetParam();
    constexpr uint32_t srcDataSize = 1024;
    constexpr uint32_t srcDtypeSize = sizeof(half);
    constexpr uint32_t dstDtypeSize = sizeof(half);
    constexpr uint8_t repeatTimes = 8;
    constexpr uint16_t dstRepStride = 1;
    constexpr uint16_t srcBlkStride = 1;
    constexpr uint16_t srcRepStride = 8;
    constexpr uint64_t mask = 128;

    TBuf<TPosition::VECCALC> srcBuf;
    tpipe.InitBuffer(srcBuf, ALIGN_ADDR(srcDataSize * srcDtypeSize));
    LocalTensor<half> srcLocal = srcBuf.Get<half>();

    TBuf<TPosition::VECCALC> dstBuf;
    tpipe.InitBuffer(dstBuf, ALIGN_ADDR(param.dstDataSize));
    LocalTensor<half> dstLocal = dstBuf.Get<half>();

    auto chkParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocal.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), dstDtypeSize, srcDtypeSize, repeatTimes, dstRepStride,
        srcBlkStride, srcRepStride, param.order, static_cast<uint64_t>(param.dstDataSize),
        static_cast<uint64_t>(srcLocal.GetLength()), static_cast<uint8_t>(dstLocal.GetPosition()),
        static_cast<uint8_t>(srcLocal.GetPosition()));
    EXPECT_EQ(CheckFunReduceOtherWhlImpl(chkParams, mask, "WholeReduceMax"), param.expect);
}

TEST_F(TestVecReduceOtherWhlCheckSuite, WholeReduceOnlyValueBoundary)
{
    constexpr uint32_t srcDataSize = 1024;
    constexpr uint64_t mask = 128;
    constexpr uint8_t repeatTimes = 8;
    constexpr uint16_t dstRepStride = 1;
    constexpr uint16_t srcBlkStride = 1;
    constexpr uint16_t srcRepStride = 8;

    TBuf<TPosition::VECCALC> srcBuf;
    tpipe.InitBuffer(srcBuf, ALIGN_ADDR(srcDataSize * sizeof(half)));
    LocalTensor<half> srcLocal = srcBuf.Get<half>();

    TBuf<TPosition::VECCALC> dstBufPass;
    tpipe.InitBuffer(dstBufPass, ALIGN_ADDR(16));
    LocalTensor<half> dstLocalPass = dstBufPass.Get<half>();
    auto passParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocalPass.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(half), sizeof(half), repeatTimes, dstRepStride,
        srcBlkStride, srcRepStride, ReduceOrder::ORDER_ONLY_VALUE, 16, srcLocal.GetLength(),
        static_cast<uint8_t>(dstLocalPass.GetPosition()), static_cast<uint8_t>(srcLocal.GetPosition()));
    EXPECT_TRUE(CheckFunReduceOtherWhlImpl(passParams, mask, "WholeReduceMax"));

    TBuf<TPosition::VECCALC> dstBufFail;
    tpipe.InitBuffer(dstBufFail, ALIGN_ADDR(14));
    LocalTensor<half> dstLocalFail = dstBufFail.Get<half>();
    auto failParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocalFail.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(half), sizeof(half), repeatTimes, dstRepStride,
        srcBlkStride, srcRepStride, ReduceOrder::ORDER_ONLY_VALUE, 14, srcLocal.GetLength(),
        static_cast<uint8_t>(dstLocalFail.GetPosition()), static_cast<uint8_t>(srcLocal.GetPosition()));
    EXPECT_FALSE(CheckFunReduceOtherWhlImpl(failParams, mask, "WholeReduceMax"));
}

TEST_F(TestVecReduceOtherWhlCheckSuite, WholeReduceOnlyIndexBoundary)
{
    constexpr uint32_t srcDataSize = 1024;
    constexpr uint64_t mask = 128;
    constexpr uint8_t repeatTimes = 8;
    constexpr uint16_t dstRepStride = 1;
    constexpr uint16_t srcBlkStride = 1;
    constexpr uint16_t srcRepStride = 8;

    TBuf<TPosition::VECCALC> srcBuf;
    tpipe.InitBuffer(srcBuf, ALIGN_ADDR(srcDataSize * sizeof(half)));
    LocalTensor<half> srcLocal = srcBuf.Get<half>();

    TBuf<TPosition::VECCALC> dstBufPass;
    tpipe.InitBuffer(dstBufPass, ALIGN_ADDR(32));
    LocalTensor<half> dstLocalPass = dstBufPass.Get<half>();
    auto passParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocalPass.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(half), sizeof(half), repeatTimes, dstRepStride,
        srcBlkStride, srcRepStride, ReduceOrder::ORDER_ONLY_INDEX, 32, srcLocal.GetLength(),
        static_cast<uint8_t>(dstLocalPass.GetPosition()), static_cast<uint8_t>(srcLocal.GetPosition()));
    EXPECT_TRUE(CheckFunReduceOtherWhlImpl(passParams, mask, "WholeReduceMax"));

    TBuf<TPosition::VECCALC> dstBufFail;
    tpipe.InitBuffer(dstBufFail, ALIGN_ADDR(30));
    LocalTensor<half> dstLocalFail = dstBufFail.Get<half>();
    auto failParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocalFail.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(half), sizeof(half), repeatTimes, dstRepStride,
        srcBlkStride, srcRepStride, ReduceOrder::ORDER_ONLY_INDEX, 30, srcLocal.GetLength(),
        static_cast<uint8_t>(dstLocalFail.GetPosition()), static_cast<uint8_t>(srcLocal.GetPosition()));
    EXPECT_FALSE(CheckFunReduceOtherWhlImpl(failParams, mask, "WholeReduceMax"));
}

TEST_F(TestVecReduceOtherWhlCheckSuite, WholeReduceDtypeMismatch)
{
    constexpr uint32_t srcDataSize = 1024;
    constexpr uint64_t mask = 128;

    TBuf<TPosition::VECCALC> srcBuf;
    tpipe.InitBuffer(srcBuf, ALIGN_ADDR(srcDataSize * sizeof(half)));
    LocalTensor<half> srcLocal = srcBuf.Get<half>();

    TBuf<TPosition::VECCALC> dstBuf;
    tpipe.InitBuffer(dstBuf, ALIGN_ADDR(32));
    LocalTensor<half> dstLocal = dstBuf.Get<half>();

    auto chkParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocal.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(float), sizeof(half), 8, 1, 1, 8,
        ReduceOrder::ORDER_VALUE_INDEX, 32, srcLocal.GetLength(), static_cast<uint8_t>(dstLocal.GetPosition()),
        static_cast<uint8_t>(srcLocal.GetPosition()));
    EXPECT_FALSE(CheckFunReduceOtherWhlImpl(chkParams, mask, "WholeReduceMax"));
}

TEST_F(TestVecReduceOtherWhlCheckSuite, WholeReduceAddrAlignCheck)
{
    constexpr uint32_t srcDataSize = 1024;

    TBuf<TPosition::VECCALC> srcBuf;
    tpipe.InitBuffer(srcBuf, ALIGN_ADDR(srcDataSize * sizeof(float)));
    LocalTensor<float> srcLocal = srcBuf.Get<float>();

    TBuf<TPosition::VECCALC> dstBuf;
    tpipe.InitBuffer(dstBuf, ALIGN_ADDR(32));
    LocalTensor<float> dstLocal = dstBuf.Get<float>();

    auto chkParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocal.GetPhyAddr()) + 2,
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(float), sizeof(float), 8, 1, 1, 8,
        ReduceOrder::ORDER_ONLY_VALUE, 32, srcLocal.GetLength(), static_cast<uint8_t>(dstLocal.GetPosition()),
        static_cast<uint8_t>(srcLocal.GetPosition()));
    check::TikcppVecReduceOtherWhlCheck chkIns("WholeReduceMax", chkParams);
    EXPECT_FALSE(chkIns.CheckAddrAlign());
}

TEST_F(TestVecReduceOtherWhlCheckSuite, WholeReduceCounterMode)
{
    constexpr uint32_t srcDataSize = 1024;

    TBuf<TPosition::VECCALC> srcBuf;
    tpipe.InitBuffer(srcBuf, ALIGN_ADDR(srcDataSize * sizeof(half)));
    LocalTensor<half> srcLocal = srcBuf.Get<half>();

    TBuf<TPosition::VECCALC> dstBuf;
    tpipe.InitBuffer(dstBuf, ALIGN_ADDR(4));
    LocalTensor<half> dstLocal = dstBuf.Get<half>();

    auto chkParams = BuildReduceWhlParams(reinterpret_cast<uintptr_t>(dstLocal.GetPhyAddr()),
        reinterpret_cast<uintptr_t>(srcLocal.GetPhyAddr()), sizeof(half), sizeof(half), 8, 1, 1, 8,
        ReduceOrder::ORDER_ONLY_VALUE, 4, srcLocal.GetLength(), static_cast<uint8_t>(dstLocal.GetPosition()),
        static_cast<uint8_t>(srcLocal.GetPosition()));
    check::TikcppVecReduceOtherWhlCheck chkIns("WholeReduceMax", chkParams);
    std::vector<uint64_t> maskArray = {129};

    AscendCUtils::SetMaskCount<half>();
    EXPECT_TRUE(chkIns.CheckTensorWhlOverflowLow(maskArray, sizeof(half), "dstLocal"));
    AscendCUtils::SetMaskNorm<half>();
}
