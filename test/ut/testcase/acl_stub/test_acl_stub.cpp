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
#include <gmock/gmock.h>
#include <cstring>
#include <memory>
#include "acl/acl.h"
#include "acl/acl_prof.h"

#define ASCENDC_CPU_DEBUG

class TEST_ACL_STUB : public ::testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(TEST_ACL_STUB, AclProfInitSuccess)
{
    EXPECT_EQ(aclprofInit(nullptr, 0), ACL_SUCCESS);
}

TEST_F(TEST_ACL_STUB, AclProfSetConfigSuccess)
{
    EXPECT_EQ(aclprofSetConfig(ACL_PROF_STORAGE_LIMIT, nullptr, 0), ACL_SUCCESS);
}

TEST_F(TEST_ACL_STUB, AclProfStartStopFinalizeSuccess)
{
    EXPECT_EQ(aclprofStart(nullptr), ACL_SUCCESS);
    EXPECT_EQ(aclprofStop(nullptr), ACL_SUCCESS);
    EXPECT_EQ(aclprofFinalize(), ACL_SUCCESS);
}

TEST_F(TEST_ACL_STUB, AclInitFinalizeTest)
{
    EXPECT_EQ(aclInit(nullptr), ACL_SUCCESS);
    EXPECT_EQ(aclFinalize(), ACL_SUCCESS);
}

TEST_F(TEST_ACL_STUB, AclrtGetVersionSuccess)
{
    EXPECT_EQ(aclrtGetVersion(nullptr, nullptr, nullptr), ACL_SUCCESS);
}

TEST_F(TEST_ACL_STUB, AclDataTypeSizeCorrect)
{
    EXPECT_EQ(aclDataTypeSize(ACL_FLOAT), 4);
    EXPECT_EQ(aclDataTypeSize(ACL_FLOAT16), 2);
    EXPECT_EQ(aclDataTypeSize(ACL_INT8), 1);
    EXPECT_EQ(aclDataTypeSize(ACL_INT16), 2);
    EXPECT_EQ(aclDataTypeSize(ACL_INT32), 4);
    EXPECT_EQ(aclDataTypeSize(ACL_INT64), 8);
    EXPECT_EQ(aclDataTypeSize(ACL_UINT8), 1);
    EXPECT_EQ(aclDataTypeSize(ACL_UINT16), 2);
    EXPECT_EQ(aclDataTypeSize(ACL_UINT32), 4);
    EXPECT_EQ(aclDataTypeSize(ACL_UINT64), 8);
    EXPECT_EQ(aclDataTypeSize(ACL_DOUBLE), 8);
    EXPECT_EQ(aclDataTypeSize(ACL_BOOL), 1);
    EXPECT_EQ(aclDataTypeSize(static_cast<aclDataType>(999)), 0);
}

TEST_F(TEST_ACL_STUB, AclFloat16AndFloatConvert)
{
    float f_val = 123.0f;
    aclFloat16 f16_val = aclFloatToFloat16(f_val);
    float f_convert = aclFloat16ToFloat(f16_val);
    EXPECT_NEAR(f_convert, f_val, 0.1f);

    EXPECT_EQ(aclFloat16ToFloat(aclFloatToFloat16(0.0f)), 0.0f);
    EXPECT_NEAR(aclFloat16ToFloat(aclFloatToFloat16(100.0f)), 100.0f, 0.1f);
}

TEST_F(TEST_ACL_STUB, AclrtDeviceStreamContextSuccess)
{
    EXPECT_EQ(aclrtSetDevice(0), ACL_SUCCESS);
    EXPECT_EQ(aclrtSetDevice(-1), ACL_SUCCESS);
    EXPECT_EQ(aclrtResetDevice(0), ACL_SUCCESS);

    aclrtStream stream = nullptr;
    EXPECT_EQ(aclrtCreateStream(&stream), ACL_SUCCESS);
    EXPECT_EQ(aclrtCreateStreamWithConfig(&stream, 10, 1), ACL_SUCCESS);
    EXPECT_EQ(aclrtDestroyStream(stream), ACL_SUCCESS);
    EXPECT_EQ(aclrtDestroyStreamForce(stream), ACL_SUCCESS);
    EXPECT_EQ(aclrtSynchronizeStream(stream), ACL_SUCCESS);

    aclrtContext ctx = nullptr;
    EXPECT_EQ(aclrtCreateContext(&ctx, 0), ACL_SUCCESS);
    EXPECT_EQ(aclrtDestroyContext(ctx), ACL_SUCCESS);
}

TEST_F(TEST_ACL_STUB, AclrtMallocFreeSuccess)
{
    void* dev_ptr = nullptr;
    EXPECT_EQ(aclrtMalloc(&dev_ptr, 1024, ACL_MEM_MALLOC_HUGE_FIRST), ACL_SUCCESS);
    EXPECT_NE(dev_ptr, nullptr);
    EXPECT_EQ(aclrtFree(dev_ptr), ACL_SUCCESS);

    EXPECT_EQ(aclrtMalloc(nullptr, 1024, ACL_MEM_MALLOC_HUGE_FIRST), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtMalloc(&dev_ptr, 0, ACL_MEM_MALLOC_HUGE_FIRST), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtFree(nullptr), ACL_ERROR_INVALID_PARAM);
}

TEST_F(TEST_ACL_STUB, AclrtMallocHostFreeHostSuccess)
{
    void* host_ptr = nullptr;
    EXPECT_EQ(aclrtMallocHost(&host_ptr, 2048), ACL_SUCCESS);
    EXPECT_NE(host_ptr, nullptr);
    EXPECT_EQ(aclrtFreeHost(host_ptr), ACL_SUCCESS);

    EXPECT_EQ(aclrtMallocHost(nullptr, 2048), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtMallocHost(&host_ptr, 0), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtFreeHost(nullptr), ACL_ERROR_INVALID_PARAM);
}

TEST_F(TEST_ACL_STUB, AclrtMemsetSuccess)
{
    char buf[1024] = {0};
    EXPECT_EQ(aclrtMemset(buf, sizeof(buf), 0x55, sizeof(buf) / 2), ACL_SUCCESS);
    for (int i = 0; i < static_cast<int>(sizeof(buf) / 2); ++i) {
        EXPECT_EQ(buf[i], 0x55);
    }

    EXPECT_EQ(aclrtMemset(nullptr, 1024, 0, 512), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtMemset(buf, 1024, 0, 2048), ACL_ERROR_BAD_ALLOC);
}

TEST_F(TEST_ACL_STUB, AclrtMemsetAsyncSuccess)
{
    char buf[1024] = {0};
    aclrtStream stream = nullptr;
    EXPECT_EQ(aclrtMemsetAsync(buf, sizeof(buf), 0x66, sizeof(buf), stream), ACL_SUCCESS);
    for (int i = 0; i < static_cast<int>(sizeof(buf)); ++i) {
        EXPECT_EQ(buf[i], 0x66);
    }

    EXPECT_EQ(aclrtMemsetAsync(nullptr, 1024, 0, 512, stream), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtMemsetAsync(buf, 1024, 0, 2048, stream), ACL_ERROR_BAD_ALLOC);
}

TEST_F(TEST_ACL_STUB, AclrtMemcpySuccess)
{
    char src[1024] = "test_memcpy";
    char dst[1024] = {0};
    EXPECT_EQ(aclrtMemcpy(dst, sizeof(dst), src, strlen(src) + 1, ACL_MEMCPY_HOST_TO_HOST), ACL_SUCCESS);
    EXPECT_STREQ(dst, src);

    EXPECT_EQ(aclrtMemcpy(dst, 5, src, 10, ACL_MEMCPY_HOST_TO_HOST), ACL_ERROR_BAD_ALLOC);
}

TEST_F(TEST_ACL_STUB, AclrtMemcpyAsyncSuccess)
{
    char src[1024] = "test_memcpy_async";
    char dst[1024] = {0};
    aclrtStream stream = nullptr;
    EXPECT_EQ(aclrtMemcpyAsync(dst, sizeof(dst), src, strlen(src) + 1, ACL_MEMCPY_HOST_TO_HOST, stream), ACL_SUCCESS);
    EXPECT_STREQ(dst, src);

    EXPECT_EQ(aclrtMemcpyAsync(dst, 5, src, 10, ACL_MEMCPY_HOST_TO_HOST, stream), ACL_ERROR_BAD_ALLOC);
}

TEST_F(TEST_ACL_STUB, AclrtMemcpy2dSuccess)
{
    char src[1024] = "test_memcpy2d";
    char dst[1024] = {0};
    EXPECT_EQ(aclrtMemcpy2d(dst, sizeof(dst), src, sizeof(src), strlen(src) + 1, 1, ACL_MEMCPY_HOST_TO_HOST), ACL_SUCCESS);
    EXPECT_STREQ(dst, src);

    EXPECT_EQ(aclrtMemcpy2d(dst, sizeof(dst), nullptr, sizeof(src), 10, 1, ACL_MEMCPY_HOST_TO_HOST), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtMemcpy2d(dst, 5, src, sizeof(src), 10, 1, ACL_MEMCPY_HOST_TO_HOST), ACL_ERROR_BAD_ALLOC);
}

TEST_F(TEST_ACL_STUB, AclrtMemcpy2dAsyncSuccess)
{
    char src[1024] = "test_memcpy2d_async";
    char dst[1024] = {0};
    aclrtStream stream = nullptr;
    EXPECT_EQ(aclrtMemcpy2dAsync(dst, sizeof(dst), src, sizeof(src), strlen(src) + 1, 1, ACL_MEMCPY_HOST_TO_HOST, stream), ACL_SUCCESS);
    EXPECT_STREQ(dst, src);

    EXPECT_EQ(aclrtMemcpy2dAsync(dst, sizeof(dst), nullptr, sizeof(src), 10, 1, ACL_MEMCPY_HOST_TO_HOST, stream), ACL_ERROR_INVALID_PARAM);
    EXPECT_EQ(aclrtMemcpy2dAsync(dst, 5, src, sizeof(src), 10, 1, ACL_MEMCPY_HOST_TO_HOST, stream), ACL_ERROR_BAD_ALLOC);
}
