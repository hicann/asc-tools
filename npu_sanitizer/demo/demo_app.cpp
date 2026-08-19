#include "acl/acl_rt.h"

#include <cstdio>

extern "C" int aclrtInit();

namespace {

bool CheckResult(const char* operation, aclError result)
{
    if (result == ACL_SUCCESS) {
        return true;
    }
    std::fprintf(stderr, "[aclsan-demo-app] %s failed: %d\n", operation, static_cast<int>(result));
    return false;
}

} // namespace

int main()
{
    if (aclrtInit() != ACL_SUCCESS) {
        std::fprintf(stderr, "[aclsan-demo-app] aclrtInit failed\n");
        return 1;
    }

    void* deviceAddress = nullptr;
    if (!CheckResult("aclrtMalloc", aclrtMalloc(&deviceAddress, 64, ACL_MEM_MALLOC_HUGE_FIRST))) {
        return 1;
    }

    static const int symbol = 0;
    aclrtFuncHandle functionHandle = nullptr;
    if (!CheckResult("aclrtGetFuncBySymbol", aclrtGetFuncBySymbol(&symbol, &functionHandle)) ||
        !CheckResult("aclrtSynchronizeStream", aclrtSynchronizeStream(nullptr)) ||
        !CheckResult("aclrtBinaryUnLoad", aclrtBinaryUnLoad(reinterpret_cast<aclrtBinHandle>(1))) ||
        !CheckResult("aclrtResetDevice", aclrtResetDevice(0)) || !CheckResult("aclrtFree", aclrtFree(deviceAddress))) {
        return 1;
    }

    std::fprintf(stderr, "[aclsan-demo-app] completed\n");
    return 0;
}
