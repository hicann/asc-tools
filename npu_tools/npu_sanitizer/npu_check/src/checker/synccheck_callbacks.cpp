// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/synccheck.h"
#include "tool_manager/kernel_attributes.h"
#include "plog_sink.h"
#include <sstream>

namespace npucheck {
const std::vector<npucheck::CallbackSpec>& Synccheck::Callbacks() const
{
    static const std::vector<npucheck::CallbackSpec> callbacks{
        {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC},
        {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
        {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_KERNEL}};
    return callbacks;
}
bool Synccheck::OnCallback(
    AclsanCallbackDomain domain, AclsanCallbackId, const void* cbdata, npucheck::CheckerReports& reports)
{
    if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION) {
        const auto* data = npucheck::ValidateDeviceCallback<AclsanDeviceSyncData>(cbdata);
        if (!data)
            return false;
        OnDeviceSync(*data);
    } else if (domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE) {
        const auto* data = npucheck::ValidateCommonCallback<AclsanSynchronizeData>(cbdata);
        if (!data)
            return false;
        npucheck::AppendCheckerReports(OnSynchronization(), reports);
    } else if (domain == ACLSAN_CB_DOMAIN_LAUNCH) {
        const auto* data = npucheck::ValidateCommonCallback<AclsanLaunchData>(cbdata);
        if (!data)
            return false;
        const npucheck::KernelAttributes attributes = npucheck::QueryKernelAttributes(data->function);
        const char* functionName = data->functionName == nullptr ? "<unknown>" : data->functionName;
        std::ostringstream message;
        message << "kernel attributes launch=" << data->launchId << " function=" << data->function
                << " function_name=" << functionName << " num_blocks=" << data->numBlocks
                << " kernel_type=" << attributes.kernelType << " kernel_type_status=" << attributes.kernelTypeStatus
                << " aic_ratio=" << attributes.aicRatio << " aiv_ratio=" << attributes.aivRatio
                << " kernel_ratio_status=" << attributes.kernelRatioStatus
                << " kernel_sched_mode=" << attributes.kernelSchedMode
                << " kernel_sched_mode_status=" << attributes.kernelSchedModeStatus
                << " launch_result=" << data->common.result;
        npucheck::WritePlog(npucheck::PlogLevel::kDebug, message.str());

        const auto logFailure = [data, functionName](const char* attribute, aclError status) {
            if (status == ACL_SUCCESS) {
                return;
            }
            std::ostringstream warning;
            warning << "kernel attribute query failed launch=" << data->launchId << " function=" << data->function
                    << " function_name=" << functionName << " attribute=" << attribute << " result=" << status;
            npucheck::WritePlog(npucheck::PlogLevel::kWarning, warning.str());
        };
        logFailure("ACL_FUNC_ATTR_KERNEL_TYPE", attributes.kernelTypeStatus);
        logFailure("ACL_FUNC_ATTR_KERNEL_RATIO", attributes.kernelRatioStatus);
        logFailure("ACL_FUNC_ATTR_KERNEL_SCHED_MODE", attributes.kernelSchedModeStatus);
    }
    return true;
}
bool Synccheck::HasErrors() const
{
    const auto stats = Stats();
    return stats.duplicateOpens != 0 || stats.unmatchedCloses != 0 || stats.unconsumedOpens != 0;
}
bool Synccheck::AnalysisComplete() const { return Stats().pendingOpens == 0; }
std::string Synccheck::Summary() const
{
    const auto stats = Stats();
    std::ostringstream output;
    output << "tool=synccheck sync_events=" << stats.syncEvents << " synchronizations=" << stats.synchronizationEvents
           << " matched_pairs=" << stats.matchedPairs << " duplicate_opens=" << stats.duplicateOpens
           << " unmatched_closes=" << stats.unmatchedCloses << " unconsumed_opens=" << stats.unconsumedOpens
           << " pending_opens=" << stats.pendingOpens
           << " errors=" << (stats.duplicateOpens + stats.unmatchedCloses + stats.unconsumedOpens) << " warnings=0";
    return output.str();
}
} // namespace npucheck
