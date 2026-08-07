#include "ascsan/cann_sanitizer.h"
#include "ascsan/internal_api.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace {

void EnsureDir(const char *path)
{
    mkdir(path, 0755);
}

AscsanLaunchConfig MakeConfig(const char *toolName, const char *workDir, const char *cacheDir)
{
    EnsureDir(workDir);
    EnsureDir(cacheDir);

    AscsanLaunchConfig config{};
    config.version = ASCSAN_API_VERSION;
    config.size = sizeof(config);
    std::snprintf(config.toolName, sizeof(config.toolName), "%s", toolName);
    std::snprintf(config.workDir, sizeof(config.workDir), "%s", workDir);
    std::snprintf(config.probeCacheDir, sizeof(config.probeCacheDir), "%s", cacheDir);
    return config;
}

void PatchDummyKernel(const char *workDir)
{
    const std::string originalPath = std::string(workDir) + "/kernel.o";
    {
        std::ofstream original(originalPath, std::ios::binary);
        original << "dummy kernel object\n";
    }

    AscsanPatchImageDesc image{};
    image.version = ASCSAN_API_VERSION;
    image.size = sizeof(image);
    image.kind = ASCSAN_PATCH_IMAGE_FILE;
    image.path = originalPath.c_str();
    char patchedPath[ASCSAN_PATH_MAX] = {};
    AscsanPatchPlanHandle plan = 0;
    assert(ascsanPatchBinaryFromImage(&image, nullptr, patchedPath, sizeof(patchedPath), &plan) ==
           ASCSAN_STATUS_SUCCESS);
    assert(plan != 0);
    assert(std::strlen(patchedPath) > 0);
}

void *TestPtr(uint64_t value)
{
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(value));
}

void SendMemcheckHostEvents()
{
    AscsanRuntimeMemoryAllocParams alloc{};
    alloc.version = ASCSAN_API_VERSION;
    alloc.size = sizeof(alloc);
    alloc.ptr = TestPtr(0x700000);
    alloc.bytes = 4096;
    alloc.memorySpace = ASCSAN_MEMORY_SPACE_DEVICE;
    alloc.deviceId = 0;
    alloc.resourceId = 42;

    AscsanRuntimeEvent allocEvent{};
    allocEvent.version = ASCSAN_API_VERSION;
    allocEvent.size = sizeof(allocEvent);
    allocEvent.apiId = ASCSAN_RT_API_ACLRT_MALLOC;
    allocEvent.phase = ASCSAN_RUNTIME_EVENT_EXIT;
    allocEvent.apiName = "aclrtMalloc";
    allocEvent.params = &alloc;
    allocEvent.result = 0;
    allocEvent.correlationId = 1001;
    assert(ascsanOnRuntimeEvent(&allocEvent) == ASCSAN_STATUS_SUCCESS);

    AscsanCannSanitizerStats stats{};
    assert(ascsanCannSanitizerGetStats(&stats) == ASCSAN_STATUS_SUCCESS);
    assert(stats.lastDomain == ASCSAN_CB_DOMAIN_RESOURCE);
    assert(stats.lastCbid == ASCSAN_CBID_RESOURCE_MEMORY_ALLOC);
    assert(stats.lastResourceId == 42);
    assert(stats.lastResourceBytes == 4096);
    assert(stats.lastResourceMemorySpace == ASCSAN_MEMORY_SPACE_DEVICE);
    assert(std::strcmp(stats.lastApiName, "aclrtMalloc") == 0);

    AscsanRuntimeMemcpyParams copy{};
    copy.version = ASCSAN_API_VERSION;
    copy.size = sizeof(copy);
    copy.dst = TestPtr(0x700100);
    copy.dstMax = 128;
    copy.src = TestPtr(0x100100);
    copy.bytes = 64;
    copy.kind = ASCSAN_MEMCPY_HOST_TO_DEVICE;
    copy.stream = TestPtr(0x55);

    AscsanRuntimeEvent memcpyEvent{};
    memcpyEvent.version = ASCSAN_API_VERSION;
    memcpyEvent.size = sizeof(memcpyEvent);
    memcpyEvent.apiId = ASCSAN_RT_API_ACLRT_MEMCPY;
    memcpyEvent.phase = ASCSAN_RUNTIME_EVENT_EXIT;
    memcpyEvent.apiName = "aclrtMemcpy";
    memcpyEvent.params = &copy;
    memcpyEvent.result = 0;
    memcpyEvent.correlationId = 1002;
    assert(ascsanOnRuntimeEvent(&memcpyEvent) == ASCSAN_STATUS_SUCCESS);

    assert(ascsanCannSanitizerGetStats(&stats) == ASCSAN_STATUS_SUCCESS);
    assert(stats.lastDomain == ASCSAN_CB_DOMAIN_MEMORY);
    assert(stats.lastCbid == ASCSAN_CBID_MEMORY_MEMCPY_END);
    assert(stats.lastMemorySrc == static_cast<uint64_t>(static_cast<std::uintptr_t>(0x100100)));
    assert(stats.lastMemoryDst == static_cast<uint64_t>(static_cast<std::uintptr_t>(0x700100)));
    assert(stats.lastMemoryBytes == 64);
    assert(stats.lastMemoryKind == ASCSAN_MEMCPY_HOST_TO_DEVICE);
    assert(std::strcmp(stats.lastApiName, "aclrtMemcpy") == 0);
}

void SendSyncEvent()
{
    AscsanRuntimeEvent syncEvent{};
    syncEvent.version = ASCSAN_API_VERSION;
    syncEvent.size = sizeof(syncEvent);
    syncEvent.apiId = ASCSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM;
    syncEvent.phase = ASCSAN_RUNTIME_EVENT_EXIT;
    syncEvent.apiName = "aclrtSynchronizeStream";
    syncEvent.result = 0;
    syncEvent.correlationId = 2001;
    assert(ascsanOnRuntimeEvent(&syncEvent) == ASCSAN_STATUS_SUCCESS);
}

void RunMemcheckProfile()
{
    const char *workDir = "/tmp/ascsan_cann_sanitizer_smoke_memcheck";
    const char *cacheDir = "/tmp/ascsan_cann_sanitizer_smoke_memcheck/cache";
    AscsanLaunchConfig config = MakeConfig("memcheck", workDir, cacheDir);

    assert(acltoolInitalize(&config) == static_cast<int>(ASCSAN_STATUS_SUCCESS));

    AscsanRuntimeHookState hookState{};
    assert(ascsanGetRuntimeHookState(&hookState) == ASCSAN_STATUS_SUCCESS);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_MTE2)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_MTE3)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_FIXPIPE)) != 0);

    SendMemcheckHostEvents();
    PatchDummyKernel(workDir);

    AscsanRawTraceRecord records[2]{};
    records[0].version = ASCSAN_API_VERSION;
    records[0].pipeline = ASCSAN_PATCH_PIPELINE_MTE2;
    records[0].siteId = 1;
    records[0].pc = 0x1010;
    records[0].arg0 = 0x1000;
    records[0].arg1 = 0x2000;
    records[0].arg2 = 64;
    records[1].version = ASCSAN_API_VERSION;
    records[1].pipeline = ASCSAN_PATCH_PIPELINE_FIXPIPE;
    records[1].siteId = 3;
    records[1].pc = 0x1050;
    records[1].arg0 = 0x3000;
    records[1].arg1 = 0x4000;
    records[1].arg2 = 128;
    assert(ascsanIngestRawTraces(records, 2) == ASCSAN_STATUS_SUCCESS);

    AscsanCannSanitizerStats stats{};
    assert(ascsanCannSanitizerGetStats(&stats) == ASCSAN_STATUS_SUCCESS);
    assert(std::strcmp(stats.toolName, "memcheck") == 0);
    assert(stats.patchCallbacks >= 3);
    assert(stats.resourceCallbacks >= 1);
    assert(stats.memoryCallbacks >= 1);
    assert(stats.deviceInstructionCallbacks >= 2);
    assert(stats.memoryTransferEvents >= 1);
    assert(stats.fixpipeEvents >= 1);
    assert(stats.lastDomain == ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION);
    assert(stats.lastCbid == ASCSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE);
    assert(stats.lastInstructionPc == 0x1050);
    assert(stats.lastInstructionBytes == 128);
    assert(std::strcmp(stats.lastInstructionOpName, "FIXPIPE") == 0);

    SendSyncEvent();
    assert(ascsanCannSanitizerGetStats(&stats) == ASCSAN_STATUS_SUCCESS);
    assert(stats.checkerEvents >= 6);
    assert(stats.checkerInstructions >= 2);
    assert(stats.checkerWindows >= 1);
    assert(stats.checkerCompletedWindows >= 1);
    assert(stats.checkerReports == 0);

    assert(ascsanCannSanitizerFinalize() == ASCSAN_STATUS_SUCCESS);
    assert(ascsanFinalize() == ASCSAN_STATUS_SUCCESS);
}

void RunSynccheckProfile()
{
    const char *workDir = "/tmp/ascsan_cann_sanitizer_smoke_synccheck";
    const char *cacheDir = "/tmp/ascsan_cann_sanitizer_smoke_synccheck/cache";
    AscsanLaunchConfig config = MakeConfig("synccheck", workDir, cacheDir);

    assert(acltoolInitalize(&config) == static_cast<int>(ASCSAN_STATUS_SUCCESS));

    AscsanRuntimeHookState hookState{};
    assert(ascsanGetRuntimeHookState(&hookState) == ASCSAN_STATUS_SUCCESS);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_GET_RLS_BUF)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_MTE2)) == 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_MTE3)) == 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_FIXPIPE)) == 0);

    PatchDummyKernel(workDir);

    AscsanRawTraceRecord records[2]{};
    records[0].version = ASCSAN_API_VERSION;
    records[0].pipeline = ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG;
    records[0].siteId = 1;
    records[0].pc = 0x2010;
    records[0].arg0 = 7;
    records[0].arg1 = 1;
    records[1].version = ASCSAN_API_VERSION;
    records[1].pipeline = ASCSAN_PATCH_PIPELINE_GET_RLS_BUF;
    records[1].siteId = 2;
    records[1].pc = 0x2020;
    records[1].arg0 = 0x5000;
    records[1].arg1 = 32;
    records[1].arg2 = 9;
    assert(ascsanIngestRawTraces(records, 2) == ASCSAN_STATUS_SUCCESS);

    AscsanCannSanitizerStats stats{};
    assert(ascsanCannSanitizerGetStats(&stats) == ASCSAN_STATUS_SUCCESS);
    assert(std::strcmp(stats.toolName, "synccheck") == 0);
    assert(stats.patchCallbacks >= 3);
    assert(stats.deviceInstructionCallbacks >= 2);
    assert(stats.syncEvents >= 2);
    assert(stats.memoryTransferEvents == 0);
    assert(stats.fixpipeEvents == 0);
    assert(stats.lastDomain == ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION);
    assert(stats.lastCbid == ASCSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF);
    assert(std::strcmp(stats.lastInstructionOpName, "GET_RLS_BUF") == 0);

    SendSyncEvent();
    assert(ascsanCannSanitizerGetStats(&stats) == ASCSAN_STATUS_SUCCESS);
    assert(stats.checkerEvents >= 4);
    assert(stats.checkerInstructions >= 2);
    assert(stats.checkerWindows >= 1);
    assert(stats.checkerCompletedWindows >= 1);
    assert(stats.checkerReports == 0);

    assert(ascsanCannSanitizerFinalize() == ASCSAN_STATUS_SUCCESS);
    assert(ascsanFinalize() == ASCSAN_STATUS_SUCCESS);
}

} // namespace

int main()
{
    (void)ascsanCannSanitizerFinalize();
    (void)ascsanFinalize();
    RunMemcheckProfile();
    RunSynccheckProfile();
    return 0;
}
