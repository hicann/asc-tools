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
#include <sys/mman.h>
#define private public
#define protected public
#include "kernel_print_lock.h"
#include "mockcpp/mockcpp.hpp"


using namespace std;
using namespace AscendC;

class TestKernelPrintLock : public testing::Test {
protected:
    void SetUp()
    {
        KernelPrintLock::printLock = nullptr;
    }
    void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestKernelPrintLock, KernelPrintLockConstruct)
{
    KernelPrintLock::CreateLock();
    EXPECT_TRUE(KernelPrintLock::printLock != nullptr);
}

TEST_F(TestKernelPrintLock, KernelPrintLockFree)
{
    KernelPrintLock::CreateLock();
    EXPECT_TRUE(KernelPrintLock::printLock != nullptr);
    KernelPrintLock::FreeLock();
    EXPECT_TRUE(KernelPrintLock::printLock == nullptr);
    KernelPrintLock::FreeLock();
    EXPECT_TRUE(KernelPrintLock::printLock == nullptr);
    KernelPrintLock::GetLock();
    EXPECT_TRUE(KernelPrintLock::printLock != nullptr);
    KernelPrintLock::GetLock();
    EXPECT_TRUE(KernelPrintLock::printLock != nullptr);
    KernelPrintLock::FreeLock();
    EXPECT_TRUE(KernelPrintLock::printLock == nullptr);
    KernelPrintLock::FreeLock();
    EXPECT_TRUE(KernelPrintLock::printLock == nullptr);
}
/*
void* MmapStub1(void* start,size_t length,int prot,int flags,int fd,off_t offset)
{
    return ((void*)-1);
}
int MunmapStub1(void* start,size_t length)
{
    return -1;
}
int32_t RaiseStub1(int32_t i)
{
    return 0;
}
TEST_F(TestKernelPrintLock, KernelPrintLockUnmapFail)
{
    MOCKER(munmap, int (*)(void*, size_t)).times(1).will(invoke(MunmapStub1));
    KernelPrintLock::GetLock(); // 需要先申请printLock内存，不然走不到munmap
    EXPECT_TRUE(KernelPrintLock::printLock != nullptr);
    KernelPrintLock::FreeLock();
}
TEST_F(TestKernelPrintLock, KernelPrintLockMapFail)
{
    MOCKER(mmap, void* (*)(void*, size_t, int, int, int, off_t)).times(1).will(invoke(MmapStub1));
    MOCKER(raise, int32_t (*)(int32_t)).times(1).will(invoke(RaiseStub1));
    KernelPrintLock::GetLock();
    EXPECT_TRUE(static_cast<int>(reinterpret_cast<intptr_t>(KernelPrintLock::printLock)) == -1);
    KernelPrintLock::printLock = nullptr;
}
*/