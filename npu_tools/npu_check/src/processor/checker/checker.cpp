// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/checker.h"
#include "checker/memcheck.h"
#include "checker/synccheck.h"
#include <algorithm>
#include <set>

namespace npucheck {
bool Checker::Accepts(AclsanCallbackDomain domain, AclsanCallbackId cbid) const
{
    const auto& callbacks = Callbacks();
    return std::any_of(callbacks.begin(), callbacks.end(), [=](const auto& callback) {
        return callback.domain == domain && callback.cbid == cbid;
    });
}

std::unique_ptr<Checker> CreateChecker(npucheck::ipc::ToolId tool)
{
    switch (tool) {
        case npucheck::ipc::ToolId::MEMCHECK:
            return std::make_unique<Memcheck>(true);
        case npucheck::ipc::ToolId::SYNCCHECK:
            return std::make_unique<npucheck::Synccheck>();
        default:
            return nullptr;
    }
}

std::vector<CallbackSpec> RequiredCallbacks(const std::vector<std::unique_ptr<Checker>>& checkers)
{
    std::set<CallbackSpec> callbacks;
    for (const auto& checker : checkers) {
        callbacks.insert(checker->Callbacks().begin(), checker->Callbacks().end());
    }
    return {callbacks.begin(), callbacks.end()};
}
} // namespace npucheck
