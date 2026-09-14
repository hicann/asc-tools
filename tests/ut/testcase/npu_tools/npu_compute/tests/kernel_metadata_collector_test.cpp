#include "runtime/kernel_metadata_collector.h"
#include <iostream>
#include <limits>

#define CHECK(condition)                                         \
    do {                                                         \
        if (!(condition)) {                                      \
            std::cerr << __LINE__ << ": " << #condition << '\n'; \
            return 1;                                            \
        }                                                        \
    } while (false)

int main()
{
    npucompute::KernelMetadataCollector collector;
    aclrtFuncHandle handle = nullptr;
    std::string name = "original kernel";
    aclptiAclrtBinaryGetFunctionParams params{nullptr, name.c_str(), &handle};
    aclptiCallbackData data{};
    data.functionParams = &params;
    data.callbackSite = ACLPTI_API_ENTER;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    name = "changed";
    params.kernelName = nullptr;
    handle = reinterpret_cast<aclrtFuncHandle>(0x1000);
    data.callbackSite = ACLPTI_API_EXIT;
    data.retval = ACL_SUCCESS;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    aclptiAclrtLaunchKernelParams launch{};
    launch.funcHandle = handle;
    launch.numBlocks = 64;
    data.functionParams = &launch;
    data.callbackSite = ACLPTI_API_ENTER;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, data);
    auto metadata = collector.Snapshot();
    CHECK(metadata.name == "original kernel");
    CHECK(metadata.blockDim == 64);
    launch.numBlocks = 128;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, data);
    CHECK(collector.Snapshot().blockDim == 64);

    npucompute::KernelMetadataCollector failed;
    params.kernelName = "failed";
    data.functionParams = &params;
    failed.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    data.callbackSite = ACLPTI_API_EXIT;
    data.retval = 1;
    failed.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    data.functionParams = &launch;
    data.callbackSite = ACLPTI_API_ENTER;
    failed.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, data);
    CHECK(!failed.Snapshot().name);

    for (bool overflow : {false, true}) {
        npucompute::KernelMetadataCollector simt;
        aclptiAclrtLaunchSIMTKernelWithArgsArrayParams simtLaunch{};
        simtLaunch.gridDim.x = overflow ? std::numeric_limits<uint32_t>::max() : 2;
        simtLaunch.gridDim.y = overflow ? std::numeric_limits<uint32_t>::max() : 3;
        simtLaunch.gridDim.z = overflow ? std::numeric_limits<uint32_t>::max() : 4;
        data.functionParams = &simtLaunch;
        simt.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray, data);
        CHECK(overflow ? !simt.Snapshot().blockDim : simt.Snapshot().blockDim == 24);
    }
    return 0;
}
