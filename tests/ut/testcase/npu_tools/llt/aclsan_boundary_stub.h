/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_TOOLS_TESTS_LLT_ACLSAN_BOUNDARY_STUB_H
#define NPU_TOOLS_TESTS_LLT_ACLSAN_BOUNDARY_STUB_H

#include "acl_san/aclsan_api.h"
#include "injection/injection_hook.h"

#include <cstdint>
#include <set>

// 统一大用例 npu_tools_utest 的 aclsan 边界桩层。
//
// 被测产品代码经三类边界与外部世界交互，全部收敛到这里：
//   1. aclsan C API（aclsanSubscribe 等）：默认转发 aclsan_api.cpp 的真实实现，
//      经 -Wl,--wrap 重定向；测试可按需替换为捕获桩。
//   2. aclsan::InvokeCallback：默认转发真实分发，测试可替换为采集桩。
//   3. 注入库边界（acltoolHookInit / ApplyRuntimeHooks / ResolveActiveDeviceCallStack /
//      acltoolGetOriginalRuntimeApi）：本层即唯一定义（默认为最小桩），测试可替换。
//
// 各测试用 RAII Guard 安装自己的行为，作用域结束自动恢复默认，用例间互不泄漏。

using AclrtApiIdSet = std::set<aclrtApiId>;

namespace aclsan {
namespace device_runtime {
struct CallStackResult;
} // namespace device_runtime
} // namespace aclsan

namespace aclsan_test {

struct Boundary {
    // 注入库边界（本层提供默认桩）
    int32_t (*hookInit)() = nullptr;                           // 默认：返回 0
    void (*applyRuntimeHooks)(const AclrtApiIdSet&) = nullptr; // 默认：空操作
    AclsanStatus (*resolveActiveDeviceCallStack)(uint64_t, aclsan::device_runtime::CallStackResult*) =
        nullptr;                                          // 默认：ERROR_NOT_FOUND
    void* (*getOriginalRuntimeApi)(aclrtApiId) = nullptr; // 默认：返回 nullptr

    // aclsan C API（默认：转发 aclsan_api.cpp 真实实现）
    AclsanStatus (*subscribe)(AclsanSubscriberHandle*, AclsanCallbackFunc, void*) = nullptr;
    AclsanStatus (*unsubscribe)(AclsanSubscriberHandle) = nullptr;
    AclsanStatus (*enableCallback)(uint32_t, AclsanSubscriberHandle, AclsanCallbackDomain, AclsanCallbackId) = nullptr;
    AclsanStatus (*getDeviceCallStack)(uint64_t, AclsanDeviceCallStack*) = nullptr;

    // aclsan::InvokeCallback（默认：转发真实分发）
    bool (*invokeCallback)(AclsanCallbackDomain, AclsanCallbackId, const void*) = nullptr;

    // 注入库注册面（acltoolRegisterAclrt*Callbacks / acltoolClearCallback）：
    // 默认转发 acl_tool_injection 库的真实实现。
    int32_t (*clearRuntimeCallback)(aclrtApiId) = nullptr;
    int32_t (*registerRuntimeCallback)(aclrtApiId, void*) = nullptr;
};

// 安装边界行为并在作用域结束时恢复之前的边界状态。
class BoundaryGuard {
public:
    explicit BoundaryGuard(const Boundary& boundary);
    ~BoundaryGuard();

    BoundaryGuard(const BoundaryGuard&) = delete;
    BoundaryGuard& operator=(const BoundaryGuard&) = delete;

private:
    Boundary previous_;
};

// 仅重置为默认边界（通常不需要：Guard 析构即恢复）。
void ResetBoundary();

} // namespace aclsan_test

#endif // NPU_TOOLS_TESTS_LLT_ACLSAN_BOUNDARY_STUB_H
