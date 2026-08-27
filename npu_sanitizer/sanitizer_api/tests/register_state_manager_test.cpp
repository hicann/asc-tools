/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_instr/arch/dav_3510/register_state_manager.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {

using aclsan::SetPaddingParamField;
using aclsan::VectorMaskParamField;
using aclsan::dav3510::Dav3510CoreKey;
using aclsan::dav3510::Dav3510RegisterStateManager;

constexpr Dav3510CoreKey VECTOR_BLOCK_3{ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR, 3};
constexpr Dav3510CoreKey VECTOR_BLOCK_4{ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR, 4};
constexpr Dav3510CoreKey CUBE_BLOCK_3{ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE, 3};

aclsan::SetL12DParamField MakeSetL12D(uint32_t instrId, uint64_t dstAddr)
{
    aclsan::SetL12DParamField params{};
    params.instrId = instrId;
    params.dataBits = 16;
    params.dstAddr = dstAddr;
    params.repeatTimes = 2;
    params.blockNum = 3;
    params.repeatGap = 4;
    return params;
}

template <typename Action>
std::string CaptureDebugLogs(Action action)
{
    assert(setenv("ASCEND_GLOBAL_LOG_LEVEL", "0", 1) == 0);
    assert(setenv("NPU_SAN_DEBUG", "1", 1) == 0);

    int pipeFds[2] = {-1, -1};
    assert(pipe(pipeFds) == 0);
    const int savedStdout = dup(STDOUT_FILENO);
    assert(savedStdout >= 0);
    assert(dup2(pipeFds[1], STDOUT_FILENO) >= 0);
    assert(close(pipeFds[1]) == 0);

    action();
    assert(std::fflush(stdout) == 0);
    assert(dup2(savedStdout, STDOUT_FILENO) >= 0);
    assert(close(savedStdout) == 0);

    std::string logs;
    char buffer[256] = {};
    ssize_t bytesRead = 0;
    while ((bytesRead = read(pipeFds[0], buffer, sizeof(buffer))) > 0) {
        logs.append(buffer, static_cast<size_t>(bytesRead));
    }
    assert(bytesRead == 0);
    assert(close(pipeFds[0]) == 0);
    assert(unsetenv("ASCEND_GLOBAL_LOG_LEVEL") == 0);
    assert(unsetenv("NPU_SAN_DEBUG") == 0);
    return logs;
}

size_t CountOccurrences(const std::string& text, const std::string& needle)
{
    size_t count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

void TestStoresDecodedRegisterState()
{
    Dav3510RegisterStateManager manager(17);
    manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{0x1234, 0x5678});
    manager.Update(VECTOR_BLOCK_3, MakeSetL12D(149, 0x2000));

    const auto state = manager.Get(VECTOR_BLOCK_3);
    assert(state.has_value());
    assert(state->vectorMask.has_value());
    assert(state->vectorMask->vectorMask0 == 0x1234);
    assert(state->vectorMask->vectorMask1 == 0x5678);
    assert(state->setL12D.has_value());
    assert(state->setL12D->instrId == 149);
    assert(state->setL12D->dataBits == 16);
    assert(state->setL12D->dstAddr == 0x2000);
    assert(state->setL12D->repeatTimes == 2);
    assert(state->setL12D->blockNum == 3);
    assert(state->setL12D->repeatGap == 4);
}

void TestIsolatesStateByBlockTypeAndBlockId()
{
    Dav3510RegisterStateManager manager(18);
    manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{1, 2});
    manager.Update(VECTOR_BLOCK_4, VectorMaskParamField{3, 4});
    manager.Update(CUBE_BLOCK_3, VectorMaskParamField{5, 6});

    assert(manager.Get(VECTOR_BLOCK_3)->vectorMask->vectorMask0 == 1);
    assert(manager.Get(VECTOR_BLOCK_4)->vectorMask->vectorMask0 == 3);
    assert(manager.Get(CUBE_BLOCK_3)->vectorMask->vectorMask0 == 5);
}

void TestKeepsOnlyLatestValueForEachRegister()
{
    Dav3510RegisterStateManager manager(19);
    manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{1, 2});
    manager.Update(VECTOR_BLOCK_3, MakeSetL12D(149, 0x2000));
    manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{7, 8});

    auto state = manager.Get(VECTOR_BLOCK_3);
    assert(state->vectorMask->vectorMask0 == 7);
    assert(state->vectorMask->vectorMask1 == 8);
    assert(state->setL12D->dstAddr == 0x2000);

    manager.Update(VECTOR_BLOCK_3, MakeSetL12D(150, 0x3000));
    state = manager.Get(VECTOR_BLOCK_3);
    assert(state->vectorMask->vectorMask0 == 7);
    assert(state->setL12D->instrId == 150);
    assert(state->setL12D->dstAddr == 0x3000);
}

void TestStoresLatestSetPaddingValueByCoreKey()
{
    Dav3510RegisterStateManager manager(21);
    manager.Update(VECTOR_BLOCK_3, SetPaddingParamField{UINT64_C(0xfedcba9876543210)});
    manager.Update(VECTOR_BLOCK_4, SetPaddingParamField{UINT64_C(0x1111222233334444)});
    manager.Update(CUBE_BLOCK_3, SetPaddingParamField{UINT64_C(0x5555666677778888)});
    manager.Update(VECTOR_BLOCK_3, SetPaddingParamField{UINT64_C(0x0123456789abcdef)});

    assert(manager.Get(VECTOR_BLOCK_3)->setPadding->value == UINT64_C(0x0123456789abcdef));
    assert(manager.Get(VECTOR_BLOCK_4)->setPadding->value == UINT64_C(0x1111222233334444));
    assert(manager.Get(CUBE_BLOCK_3)->setPadding->value == UINT64_C(0x5555666677778888));
}

void TestResetClearsStateAndKeepsLaunchIdentity()
{
    Dav3510RegisterStateManager manager(20);
    manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{1, 2});

    manager.Reset();

    assert(manager.GetLaunchId() == 20);
    assert(!manager.Get(VECTOR_BLOCK_3).has_value());
}

void TestLogsEveryRegisterUpdateWithFullNewValue()
{
    Dav3510RegisterStateManager manager(17);
    const std::string logs = CaptureDebugLogs([&manager] {
        manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{0x1234, 0x5678});
        manager.Update(VECTOR_BLOCK_3, VectorMaskParamField{0x9abc, 0xdef0});
        manager.Update(VECTOR_BLOCK_3, MakeSetL12D(149, 0x2000));
        manager.Update(VECTOR_BLOCK_3, SetPaddingParamField{UINT64_C(0xfedcba9876543210)});
    });

    assert(CountOccurrences(logs, "action=update register=vector_mask") == 2);
    assert(
        logs.find("[register] action=update register=vector_mask launchId=17 blockType=1 blockId=3 "
                  "vectorMask0=0x1234 vectorMask1=0x5678") != std::string::npos);
    assert(
        logs.find("[register] action=update register=vector_mask launchId=17 blockType=1 blockId=3 "
                  "vectorMask0=0x9abc vectorMask1=0xdef0") != std::string::npos);
    assert(
        logs.find("[register] action=update register=set_l1_2d launchId=17 blockType=1 blockId=3 "
                  "instrId=149 dataBits=16 dstAddr=0x2000 repeatTimes=2 blockNum=3 repeatGap=4") != std::string::npos);
    assert(
        logs.find("[register] action=update register=set_padding launchId=17 blockType=1 blockId=3 "
                  "value=0xfedcba9876543210") != std::string::npos);
    assert(logs.find("instrExecId") == std::string::npos);
    assert(logs.find("config0") == std::string::npos);
}

} // namespace

int main()
{
    TestStoresDecodedRegisterState();
    TestIsolatesStateByBlockTypeAndBlockId();
    TestKeepsOnlyLatestValueForEachRegister();
    TestStoresLatestSetPaddingValueByCoreKey();
    TestResetClearsStateAndKeepsLaunchIdentity();
    TestLogsEveryRegisterUpdateWithFullNewValue();
    return 0;
}
