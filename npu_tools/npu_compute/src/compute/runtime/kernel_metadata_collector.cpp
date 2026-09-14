#include "runtime/kernel_metadata_collector.h"

#include <limits>

namespace npucompute {
namespace {

std::optional<uint64_t> GridSize(const dim3& grid)
{
    uint64_t size = grid.x;
    for (const uint64_t dimension : {static_cast<uint64_t>(grid.y), static_cast<uint64_t>(grid.z)}) {
        if (dimension != 0 && size > std::numeric_limits<uint64_t>::max() / dimension) {
            return std::nullopt;
        }
        size *= dimension;
    }
    return size;
}

} // namespace

void KernelMetadataCollector::OnCallback(aclptiCallbackId cbid, const aclptiCallbackData& data)
{
    if (data.functionParams == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (frozen_) {
        return;
    }
    if (cbid == ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction) {
        const auto& params = *static_cast<const aclptiAclrtBinaryGetFunctionParams*>(data.functionParams);
        if (params.funcHandle == nullptr) {
            return;
        }
        if (data.callbackSite == ACLPTI_API_ENTER) {
            pendingNames_.erase(params.funcHandle);
            if (params.kernelName != nullptr) {
                pendingNames_[params.funcHandle] = {params.binHandle, params.kernelName};
            }
        } else if (data.callbackSite == ACLPTI_API_EXIT) {
            const auto pending = pendingNames_.find(params.funcHandle);
            if (pending != pendingNames_.end()) {
                if (data.retval == ACL_SUCCESS && *params.funcHandle != nullptr) {
                    names_[*params.funcHandle] = std::move(pending->second);
                }
                pendingNames_.erase(pending);
            }
        }
        return;
    }
    if (data.callbackSite == ACLPTI_API_EXIT && data.retval == ACL_SUCCESS) {
        if (cbid == ACLPTI_RUNTIME_CBID_aclrtBinaryUnLoad) {
            const auto& params = *static_cast<const aclptiAclrtBinaryUnLoadParams*>(data.functionParams);
            for (auto iterator = names_.begin(); iterator != names_.end();) {
                if (iterator->second.first == params.binHandle) {
                    iterator = names_.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }
        return;
    }
    if (data.callbackSite != ACLPTI_API_ENTER) {
        return;
    }
    aclrtFuncHandle handle = nullptr;
    KernelMetadata candidate;
    candidate.deviceId = 0;
    switch (cbid) {
        case ACLPTI_RUNTIME_CBID_aclrtLaunchKernel: {
            const auto& params = *static_cast<const aclptiAclrtLaunchKernelParams*>(data.functionParams);
            handle = params.funcHandle;
            candidate.blockDim = params.numBlocks;
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs: {
            const auto& params = *static_cast<const aclptiAclrtLaunchKernelWithHostArgsParams*>(data.functionParams);
            handle = params.funcHandle;
            candidate.blockDim = params.numBlocks;
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithArgsArray: {
            const auto& params = *static_cast<const aclptiAclrtLaunchKernelWithArgsArrayParams*>(data.functionParams);
            candidate.blockDim = params.numBlocks;
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs: {
            const auto& params =
                *static_cast<const aclptiAclrtLaunchSIMTKernelWithHostArgsParams*>(data.functionParams);
            candidate.blockDim = GridSize(params.gridDim);
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray: {
            const auto& params =
                *static_cast<const aclptiAclrtLaunchSIMTKernelWithArgsArrayParams*>(data.functionParams);
            candidate.blockDim = GridSize(params.gridDim);
            break;
        }
        default:
            return;
    }
    const auto found = names_.find(handle);
    if (found != names_.end()) {
        candidate.name = found->second.second;
    }
    metadata_ = std::move(candidate);
}

KernelMetadata KernelMetadataCollector::Snapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    frozen_ = true;
    return metadata_;
}

} // namespace npucompute
