#pragma once

#include "aclpti/aclpti_runtime_api.h"
#include "compute_types.h"
#include <map>
#include <mutex>

namespace npucompute {

class KernelMetadataCollector final {
public:
    void OnCallback(aclptiCallbackId cbid, const aclptiCallbackData& data);
    KernelMetadata Snapshot();

private:
    std::mutex mutex_;
    std::map<aclrtFuncHandle, std::pair<aclrtBinHandle, std::string>> names_;
    std::map<aclrtFuncHandle*, std::pair<aclrtBinHandle, std::string>> pendingNames_;
    KernelMetadata metadata_;
    bool frozen_ = false;
};

} // namespace npucompute
