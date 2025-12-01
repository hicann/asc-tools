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
#include <cmath>
#include <fstream>
#include "stub_def.h"
#include <thread>

using namespace std;

extern std::map<std::string, uint64_t>& GetArgVal();

class TEST_STUB_BASE : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(TEST_STUB_BASE, AddNameArgTest)
{
    GetArgVal().clear();
    string kernelName = "test_stub_base";
    unsigned long val = 10;
    AscendC::AddNameArg(kernelName.c_str(), val);
    unsigned long ret = AscendC::GetNameArg(kernelName.c_str());
    EXPECT_EQ(GetArgVal().size(), 1);
    EXPECT_TRUE(GetArgVal().find(kernelName) != GetArgVal().end());
    EXPECT_TRUE(GetArgVal()[kernelName] == val);
    EXPECT_TRUE(ret == val);
}

TEST_F(TEST_STUB_BASE, BuildExpTest)
{
    GetArgVal().clear();
    string kernelName = "test_stub_base";
    int32_t len = 50;
    for (int32_t i = 0; i < len; i++) {
        string kernelName = "arg" + to_string(i);
        AscendC::AddNameArg(kernelName.c_str(), i * 1000);
    }
    string ret = AscendC::BuildExp(0x2000000000000000);
    EXPECT_EQ(ret, "(((uint64_t)) + 0x1000000000000000)");
}
