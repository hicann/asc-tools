// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_TOOLS_NPU_CHECK_SRC_PROCESSOR_CHECKER_CHECKER_H
#define NPU_TOOLS_NPU_CHECK_SRC_PROCESSOR_CHECKER_CHECKER_H

#include "acl_san/aclsan_api.h"
#include "diagnostic/report_message.h"
#include "wire_protocol.h"
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace npucheck {
struct CallbackSpec {
    AclsanCallbackDomain domain;
    AclsanCallbackId cbid;
    bool operator<(const CallbackSpec& other) const noexcept
    {
        return std::tie(domain, cbid) < std::tie(other.domain, other.cbid);
    }
};
using CheckerReport = std::variant<npucheck::NpuCheckMemcheckReport, npucheck::NpuCheckSynccheckReport>;
using CheckerReports = std::vector<CheckerReport>;

// Checkers declare their own events and state. ToolManager serializes callbacks,
// unions subscriptions, and aggregates reports without knowing concrete checkers.
class Checker {
public:
    virtual ~Checker() = default;
    virtual const std::vector<CallbackSpec>& Callbacks() const = 0;
    // False means malformed data. Unrelated events are filtered by Accepts().
    virtual bool OnCallback(
        AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* data, CheckerReports& reports) = 0;
    virtual std::string Summary() const = 0;
    virtual bool HasErrors() const = 0;
    virtual bool AnalysisComplete() const = 0;
    bool Accepts(AclsanCallbackDomain domain, AclsanCallbackId cbid) const;
};

std::unique_ptr<Checker> CreateChecker(npucheck::ipc::ToolId tool);
std::vector<CallbackSpec> RequiredCallbacks(const std::vector<std::unique_ptr<Checker>>& checkers);

template <class T>
const T* ValidateCommonCallback(const void* data)
{
    if (data == nullptr)
        return nullptr;
    const auto* typed = static_cast<const T*>(data);
    return typed->common.version == ACLSAN_API_VERSION && typed->common.size >= sizeof(T) ? typed : nullptr;
}

template <class T>
const T* ValidateDeviceCallback(const void* data)
{
    if (data == nullptr)
        return nullptr;
    const auto* typed = static_cast<const T*>(data);
    return typed->header.version == ACLSAN_API_VERSION && typed->header.size >= sizeof(T) ? typed : nullptr;
}

template <class T>
void AppendCheckerReports(std::vector<T> source, CheckerReports& target)
{
    for (auto& report : source)
        target.emplace_back(std::move(report));
}
} // namespace npucheck

#endif // NPU_TOOLS_NPU_CHECK_SRC_PROCESSOR_CHECKER_CHECKER_H
