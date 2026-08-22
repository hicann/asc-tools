// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "npu_check.h"

#include "tool_manager/tool_manager.h"

#include <cstdlib>
#include <memory>
#include <mutex>

namespace {

std::mutex g_serviceMutex;
std::unique_ptr<npu::sanitizer::ToolManager> g_service;

void FinalizeService() noexcept
{
    try {
        std::lock_guard<std::mutex> lock(g_serviceMutex);
        g_service.reset();
    } catch (...) {
        // Process-exit cleanup cannot propagate through std::atexit.
        return;
    }
}

} // namespace

extern "C" NPU_CHECK_API int acltoolInitialize(void)
{
    try {
        std::lock_guard<std::mutex> lock(g_serviceMutex);
        if (g_service) {
            return g_service->IsInitialized() ? 0 : 1;
        }
        auto service = std::make_unique<npu::sanitizer::ToolManager>();
        const int result = service->Initialize();
        if (result != 0) {
            return result;
        }
        if (std::atexit(FinalizeService) != 0) {
            return 1;
        }
        g_service = std::move(service);
        return 0;
    } catch (...) {
        return 1;
    }
}
