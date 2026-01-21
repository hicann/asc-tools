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

struct TestDataCopySliceApiCheckParams {
    uint32_t srcdataSize;
    uint32_t dstdataSize;
    TPosition pos;
    uint32_t typeSize;
    SliceInfo srcSliceInfo[2];
    SliceInfo dstSliceInfo[2];
    bool isGm2Ub;
    bool expect;
};

class TestDataCopySliceApiCheckSuite : public testing::Test,
    public testing::WithParamInterface<TestDataCopySliceApiCheckParams> {
protected:
    void SetUp()
    {
        g_coreType = AIV_TYPE;
    }
    void TearDown()
    {
        AscendC::CheckSyncState();
        g_coreType = MIX_TYPE;
    }
};

INSTANTIATE_TEST_CASE_P(TEST_DATA_COPY_SLICE_API_CHECK, TestDataCopySliceApiCheckSuite,
    ::testing::Values(TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    true },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 7, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    false,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 1, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 72, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 49, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 88, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 54, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 71, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::A1,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::A1,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    false,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 0, 47, 0, 1, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 2, 3 } },
    { { 0, 47, 0, 3, 48 }, { 0, 1, 0, 2, 2 } },
    true,
    false },
    TestDataCopySliceApiCheckParams{ 264,
    96,
    TPosition::VECCALC,
    4,
    { { 16, 71, 8, 3, 88 }, { 0, 2, 1, 1, 3 } },
    { { 47, 47, 0, 3, 48 }, { 0, 1, 0, 1, 2 } },
    true,
    false }));

TEST_P(TestDataCopySliceApiCheckSuite, DataCopySliceApiHighLevel)
{
    TPipe tpipe;
    auto param = GetParam();

    uint32_t hGM = 3;
    uint32_t wGM = param.srcdataSize / hGM;
    uint32_t srcShape[2] = {wGM, hGM};
    uint32_t dimValue = 2;

    uint32_t hUB = 2;
    uint32_t wUB = param.dstdataSize / hUB;
    uint32_t dstShape[2] = {wUB, hUB};

    LocalTensor<uint32_t> inputLocal;
    if (param.pos == TPosition::VECCALC) {
        TBuf<TPosition::VECCALC> tbuf;
        tpipe.InitBuffer(tbuf, param.dstdataSize * sizeof(uint32_t));
        inputLocal = tbuf.Get<uint32_t>();
    } else {
        TBuf<TPosition::A1> tbuf;
        tpipe.InitBuffer(tbuf, param.dstdataSize * sizeof(uint32_t));
        inputLocal = tbuf.Get<uint32_t>();
    }

    check::DataCopySliceApiParams chkParams{ static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(inputLocal.GetPhyAddr())),
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(inputLocal.GetPhyAddr())),
        static_cast<uint32_t>(param.typeSize),
        static_cast<uint32_t>(param.typeSize),
        static_cast<uint64_t>(inputLocal.GetLength()),
        static_cast<uint8_t>(inputLocal.GetPosition()),
        dimValue,
        dstShape,
        srcShape,
        param.dstSliceInfo,
        param.srcSliceInfo,
        param.isGm2Ub };
    check::TikcppDataCopySliceCheck chkIns{ "mov_align", chkParams };
    bool flag = CheckFuncDataCopySliceImpl(chkParams, "mov_align");

    EXPECT_EQ(flag, param.expect);
}

// test data copy api
struct TestDataCopyApiCheckParams {
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint32_t dstDtypeBytes = 0;
    uint32_t srcDtypeBytes = 0;
    uint8_t dstPos = 0;
    uint8_t srcPos = 0;
    uint16_t blockCount = 0;
    uint16_t blockLen = 0;
    uint16_t srcStride = 0;
    uint16_t dstStride = 0;
    bool expect;
};

class TestDataCopyApiCheckSuite : public testing::Test,
    public testing::WithParamInterface<TestDataCopyApiCheckParams> {
protected:
    void SetUp()
    {
        g_coreType = AIV_TYPE;
    }
    void TearDown()
    {
        g_coreType = MIX_TYPE;
        AscendC::CheckSyncState();
    }
public:
    TPipe tpipe;
};

INSTANTIATE_TEST_CASE_P(TEST_DATA_COPY_API_CHECK, TestDataCopyApiCheckSuite,
    ::testing::Values(
    TestDataCopyApiCheckParams{ 0x12345, 0x6789, 4, 4,
    static_cast<uint8_t>(Hardware::GM), static_cast<uint8_t>(Hardware::UB),
    1, 1, 8, 8, false }
    ));

TEST_P(TestDataCopyApiCheckSuite, DataCopyApiHighLevel)
{
    auto param = GetParam();

    check::DataCopyApiParams chkParams{
        param.dstAddr, param.srcAddr, param.dstDtypeBytes, param.srcDtypeBytes, param.dstPos, 
        param.srcPos, param.blockCount, param.blockLen, param.srcStride, param.dstStride
    };
    check::TikcppDataCopyCheck chkIns{ "DataCopy", chkParams };
    bool flag = CheckFuncDataCopyImpl(chkParams, "DataCopy");

    EXPECT_EQ(flag, param.expect);
}
