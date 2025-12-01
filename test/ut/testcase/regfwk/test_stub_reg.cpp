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
#include <fcntl.h>
#include <fstream>
#include "stub_reg.h"

using namespace std;

extern const char* g_regStubs[INTRI_TYPE_MAX];

class TEST_STUB_REG : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(TEST_STUB_REG, StubRegTest)
{
    IntriTypeT type = INTRI_TYPE_USER1;
    string stub = "user1";
    AscendC::StubReg(type, stub.c_str());
    EXPECT_EQ(g_regStubs[type], stub);
    g_regStubs[type] = NULL;
}

TEST_F(TEST_STUB_REG, StubInitTest)
{
    AscendC::StubInit();
    ifstream resultFile;
    string fileName = "stub_reg.log";
    EXPECT_TRUE(access(fileName.c_str(), (O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)));
}

TEST_F(TEST_STUB_REG, SetKernelModeTest)
{
    auto tmp = g_taskRation;
    AscendC::SetKernelMode(KernelMode::MIX_AIC_1_1);
    EXPECT_EQ(g_taskRation, 1);
    g_taskRation = tmp;
}
