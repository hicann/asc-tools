#include "ascsan/ascsan_api.h"
#include "ascsan/ascsan_callback.h"
#include "ascsan/ascsan_memory.h"
#include "ascsan/ascsan_patch.h"
#include "ascsan/ascsan_symbolize.h"
#include "ascsan/internal_api.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace {

static_assert(sizeof(AscsanInitParams) ==
                  sizeof(uint32_t) * 2 + sizeof(const AscsanLaunchConfig *),
              "AscsanInitParams should only carry version, size, and launchConfig");
static_assert(ASCSAN_CBID_INVALID == 0, "Callback ID 0 is invalid in every domain");
static_assert(ASCSAN_CBID_RESOURCE_INVALID == 0, "Resource callback ID 0 is invalid");
static_assert(ASCSAN_CBID_RESOURCE_MEMORY_ALLOC == 1, "Resource callback IDs are domain-local");
static_assert(ASCSAN_CBID_MEMORY_INVALID == 0, "Memory callback ID 0 is invalid");
static_assert(ASCSAN_CBID_MEMORY_MEMCPY_BEGIN == 1, "Memory callback IDs are domain-local");
static_assert(ASCSAN_CBID_BINARY_INVALID == 0, "Binary callback ID 0 is invalid");
static_assert(ASCSAN_CBID_BINARY_LOAD_BEGIN == 1, "Binary callback IDs are domain-local");
static_assert(ASCSAN_CBID_PATCH_INVALID == 0, "Patch callback ID 0 is invalid");
static_assert(ASCSAN_CBID_PATCH_BEGIN == 1, "Patch callback IDs are domain-local");
static_assert(ASCSAN_CBID_LAUNCH_INVALID == 0, "Launch callback ID 0 is invalid");
static_assert(ASCSAN_CBID_LAUNCH_BEGIN == 1, "Launch callback IDs are domain-local");
static_assert(ASCSAN_CBID_SYNCHRONIZE_INVALID == 0, "Synchronize callback ID 0 is invalid");
static_assert(ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END == 1, "Synchronize callback IDs are domain-local");
static_assert(ASCSAN_CBID_DEVICE_INSTRUCTION_INVALID == 0, "Device callback ID 0 is invalid");
static_assert(ASCSAN_CBID_DEVICE_MEMORY_ACCESS == 1, "Device callback IDs are domain-local");
static_assert(ASCSAN_CBID_DEVICE_SYNC == 2, "Device callback IDs are domain-local");
static_assert(ASCSAN_CBID_DEVICE_INSTRUCTION_MTE2 == ASCSAN_CBID_DEVICE_MEMORY_ACCESS,
              "Pipeline-specific memory aliases map to DEVICE_MEMORY_ACCESS");
static_assert(ASCSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG == ASCSAN_CBID_DEVICE_SYNC,
              "Pipeline-specific sync aliases map to DEVICE_SYNC");
static_assert(ASCSAN_CBID_REPORT_INVALID == 0, "Report callback ID 0 is invalid");
static_assert(ASCSAN_CBID_REPORT_RECORD == 1, "Report callback IDs are domain-local");
static_assert(ASCSAN_CBID_ERROR_INVALID == 0, "Error callback ID 0 is invalid");
static_assert(ASCSAN_CBID_ERROR_RECORD == 1, "Error callback IDs are domain-local");

struct CallbackCounters {
    int patchBegin = 0;
    int patchEnd = 0;
    int siteMap = 0;
    int mte2 = 0;
    int resourceAlloc = 0;
    int memcpyEnd = 0;
    int binaryFileBegin = 0;
    int binaryDataBegin = 0;
    int insideCallbackObserved = 0;
};

void EnsureDir(const char *path)
{
    mkdir(path, 0755);
}

void Callback(void *userdata, AscsanCallbackDomain domain, uint32_t cbid, const void *cbdata)
{
    auto *counters = static_cast<CallbackCounters *>(userdata);
    assert(ascsanIsInsideCallback() == 1);
    counters->insideCallbackObserved = 1;

    if (domain == ASCSAN_CB_DOMAIN_PATCH && cbid == ASCSAN_CBID_PATCH_BEGIN) {
        ++counters->patchBegin;
    }
    if (domain == ASCSAN_CB_DOMAIN_PATCH && cbid == ASCSAN_CBID_PATCH_SITE_MAP_CREATED) {
        ++counters->siteMap;
    }
    if (domain == ASCSAN_CB_DOMAIN_PATCH && cbid == ASCSAN_CBID_PATCH_END) {
        ++counters->patchEnd;
    }
    if (domain == ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION &&
        cbid == ASCSAN_CBID_DEVICE_MEMORY_ACCESS) {
        const auto *data = static_cast<const AscsanDeviceInstructionData *>(cbdata);
        assert(data != nullptr);
        if (data->pipeline == ASCSAN_PATCH_PIPELINE_MTE2) {
            ++counters->mte2;
        }
    }
    if (domain == ASCSAN_CB_DOMAIN_RESOURCE && cbid == ASCSAN_CBID_RESOURCE_MEMORY_ALLOC) {
        ++counters->resourceAlloc;
        const auto *data = static_cast<const AscsanResourceData *>(cbdata);
        assert(data != nullptr);
        assert(data->resourceId == 7);
        assert(data->bytes == 1024);
    }
    if (domain == ASCSAN_CB_DOMAIN_MEMORY && cbid == ASCSAN_CBID_MEMORY_MEMCPY_END) {
        ++counters->memcpyEnd;
        const auto *data = static_cast<const AscsanMemoryMemcpyData *>(cbdata);
        assert(data != nullptr);
        assert(data->bytes == 32);
        assert(data->kind == ASCSAN_MEMCPY_HOST_TO_DEVICE);
    }
    if (domain == ASCSAN_CB_DOMAIN_BINARY && cbid == ASCSAN_CBID_BINARY_LOAD_BEGIN) {
        const auto *data = static_cast<const AscsanBinaryData *>(cbdata);
        assert(data != nullptr);
        if (data->image.kind == ASCSAN_PATCH_IMAGE_FILE) {
            ++counters->binaryFileBegin;
            assert((data->image.flags & ASCSAN_BINARY_IMAGE_FLAG_PATH_VALID) != 0);
            assert(data->image.path != nullptr);
        } else if (data->image.kind == ASCSAN_PATCH_IMAGE_MEMORY) {
            ++counters->binaryDataBegin;
            assert((data->image.flags & ASCSAN_BINARY_IMAGE_FLAG_DATA_VALID) != 0);
            assert(data->image.imageData != nullptr);
            assert(data->image.imageSize != 0);
        } else {
            assert(false);
        }
    }
}

void *TestPtr(uint64_t value)
{
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(value));
}

} // namespace

int main()
{
    const char *workDir = "/tmp/ascsan_api_smoke";
    const char *cacheDir = "/tmp/ascsan_api_smoke/cache";
    EnsureDir(workDir);
    EnsureDir(cacheDir);

    const std::string originalPath = std::string(workDir) + "/kernel.o";
    {
        std::ofstream original(originalPath, std::ios::binary);
        original << "dummy kernel object\n";
    }

    AscsanLaunchConfig config{};
    config.version = ASCSAN_API_VERSION;
    config.size = sizeof(config);
    std::snprintf(config.toolName, sizeof(config.toolName), "%s", "memcheck");
    std::snprintf(config.workDir, sizeof(config.workDir), "%s", workDir);
    std::snprintf(config.probeCacheDir, sizeof(config.probeCacheDir), "%s", cacheDir);

    AscsanInitParams init{};
    init.version = ASCSAN_API_VERSION;
    init.size = sizeof(init);
    init.launchConfig = &config;
    assert(ascsanInitialize(&init) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanApplyLaunchConfig(&config) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanRegisterBuiltinPatchPipelines() == ASCSAN_STATUS_SUCCESS);

    CallbackCounters counters{};
    AscsanSubscribeDesc subDesc{};
    subDesc.version = ASCSAN_API_VERSION;
    subDesc.size = sizeof(subDesc);
    subDesc.name = "smoke";
    subDesc.callback = Callback;
    subDesc.userdata = &counters;
    AscsanSubscriberHandle subscriber = ASCSAN_INVALID_SUBSCRIBER_HANDLE;
    assert(ascsanSubscribe(&subDesc, &subscriber) == ASCSAN_STATUS_SUCCESS);
    AscsanSubscriberHandle duplicateSubscriber = ASCSAN_INVALID_SUBSCRIBER_HANDLE;
    assert(ascsanSubscribe(&subDesc, &duplicateSubscriber) ==
           ASCSAN_STATUS_ERROR_MAX_LIMIT_REACHED);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_RESOURCE,
                                ASCSAN_CBID_RESOURCE_INVALID,
                                1) == ASCSAN_STATUS_ERROR_INVALID_VALUE);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_PATCH,
                                ASCSAN_CBID_PATCH_BEGIN,
                                1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_PATCH,
                                ASCSAN_CBID_PATCH_SITE_MAP_CREATED,
                                1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_PATCH,
                                ASCSAN_CBID_PATCH_END,
                                1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION,
                                ASCSAN_CBID_DEVICE_MEMORY_ACCESS,
                                1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_RESOURCE,
                                ASCSAN_CBID_RESOURCE_MEMORY_ALLOC,
                                1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_MEMORY,
                                ASCSAN_CBID_MEMORY_MEMCPY_END,
                                1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanEnableCallback(subscriber,
                                ASCSAN_CB_DOMAIN_BINARY,
                                ASCSAN_CBID_BINARY_LOAD_BEGIN,
                                1) == ASCSAN_STATUS_SUCCESS);

    AscsanRuntimeHookState hookState{};
    assert(ascsanGetRuntimeHookState(&hookState) == ASCSAN_STATUS_SUCCESS);
    assert((hookState.activePlan.patchPipelineMask & (1u << ASCSAN_PATCH_PIPELINE_MTE2)) != 0);
    assert(hookState.activePlan.ruleCount > 0);

    AscsanRuntimeMemoryAllocParams alloc{};
    alloc.version = ASCSAN_API_VERSION;
    alloc.size = sizeof(alloc);
    alloc.ptr = TestPtr(0x7000);
    alloc.bytes = 1024;
    alloc.memorySpace = ASCSAN_MEMORY_SPACE_DEVICE;
    alloc.deviceId = 0;
    alloc.resourceId = 7;
    AscsanRuntimeEvent allocEvent{};
    allocEvent.version = ASCSAN_API_VERSION;
    allocEvent.size = sizeof(allocEvent);
    allocEvent.apiId = ASCSAN_RT_API_ACLRT_MALLOC;
    allocEvent.phase = ASCSAN_RUNTIME_EVENT_EXIT;
    allocEvent.apiName = "aclrtMalloc";
    allocEvent.params = &alloc;
    assert(ascsanOnRuntimeEvent(&allocEvent) == ASCSAN_STATUS_SUCCESS);
    assert(counters.resourceAlloc == 1);

    AscsanRuntimeMemcpyParams memcpy{};
    memcpy.version = ASCSAN_API_VERSION;
    memcpy.size = sizeof(memcpy);
    memcpy.dst = TestPtr(0x8000);
    memcpy.dstMax = 64;
    memcpy.src = TestPtr(0x9000);
    memcpy.bytes = 32;
    memcpy.kind = ASCSAN_MEMCPY_HOST_TO_DEVICE;
    AscsanRuntimeEvent memcpyEvent{};
    memcpyEvent.version = ASCSAN_API_VERSION;
    memcpyEvent.size = sizeof(memcpyEvent);
    memcpyEvent.apiId = ASCSAN_RT_API_ACLRT_MEMCPY;
    memcpyEvent.phase = ASCSAN_RUNTIME_EVENT_EXIT;
    memcpyEvent.apiName = "aclrtMemcpy";
    memcpyEvent.params = &memcpy;
    assert(ascsanOnRuntimeEvent(&memcpyEvent) == ASCSAN_STATUS_SUCCESS);
    assert(counters.memcpyEnd == 1);

    AscsanRuntimeBinaryLoadFromFileParams binaryFile{};
    binaryFile.version = ASCSAN_API_VERSION;
    binaryFile.size = sizeof(binaryFile);
    binaryFile.path = originalPath.c_str();
    binaryFile.imageVersion = "file-v1";
    binaryFile.binaryId = 11;
    AscsanRuntimeEvent binaryFileEvent{};
    binaryFileEvent.version = ASCSAN_API_VERSION;
    binaryFileEvent.size = sizeof(binaryFileEvent);
    binaryFileEvent.apiId = ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE;
    binaryFileEvent.phase = ASCSAN_RUNTIME_EVENT_ENTER;
    binaryFileEvent.apiName = "aclrtBinaryLoadFromFile";
    binaryFileEvent.params = &binaryFile;
    assert(ascsanOnRuntimeEvent(&binaryFileEvent) == ASCSAN_STATUS_SUCCESS);
    assert(counters.binaryFileBegin == 1);

    const char runtimeImageData[] = "runtime memory image\n";
    AscsanRuntimeBinaryLoadFromDataParams binaryData{};
    binaryData.version = ASCSAN_API_VERSION;
    binaryData.size = sizeof(binaryData);
    binaryData.imageData = runtimeImageData;
    binaryData.imageSize = sizeof(runtimeImageData);
    binaryData.imageVersion = "memory-v1";
    binaryData.binaryId = 12;
    AscsanRuntimeEvent binaryDataEvent{};
    binaryDataEvent.version = ASCSAN_API_VERSION;
    binaryDataEvent.size = sizeof(binaryDataEvent);
    binaryDataEvent.apiId = ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_DATA;
    binaryDataEvent.phase = ASCSAN_RUNTIME_EVENT_ENTER;
    binaryDataEvent.apiName = "aclrtBinaryLoadFromData";
    binaryDataEvent.params = &binaryData;
    assert(ascsanOnRuntimeEvent(&binaryDataEvent) == ASCSAN_STATUS_SUCCESS);
    assert(counters.binaryDataBegin == 1);

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
    assert(counters.patchBegin == 1);
    assert(counters.siteMap == 1);
    assert(counters.patchEnd == 1);

    const char memoryImageData[] = "dummy memory kernel object\n";
    AscsanPatchImageDesc memoryImage{};
    memoryImage.version = ASCSAN_API_VERSION;
    memoryImage.size = sizeof(memoryImage);
    memoryImage.kind = ASCSAN_PATCH_IMAGE_MEMORY;
    memoryImage.imageData = memoryImageData;
    memoryImage.imageSize = sizeof(memoryImageData);

    char patchedMemoryPath[ASCSAN_PATH_MAX] = {};
    AscsanPatchPlanHandle memoryPlan = 0;
    assert(ascsanPatchBinaryFromImage(
               &memoryImage, nullptr, patchedMemoryPath, sizeof(patchedMemoryPath), &memoryPlan) ==
           ASCSAN_STATUS_SUCCESS);
    assert(memoryPlan != 0);
    assert(std::strlen(patchedMemoryPath) > 0);
    assert(counters.patchBegin == 2);
    assert(counters.siteMap == 2);
    assert(counters.patchEnd == 2);

    AscsanPatchSiteInfo site{};
    assert(ascsanGetPatchSiteInfo(1, &site) == ASCSAN_STATUS_SUCCESS);
    assert(site.pipeline == ASCSAN_PATCH_PIPELINE_MTE2);
    assert(site.opName != nullptr);

    AscsanDevicePcQuery query{};
    query.version = ASCSAN_API_VERSION;
    query.size = sizeof(query);
    query.binaryId = site.binaryId;
    query.functionId = site.functionId;
    query.siteId = site.siteId;
    query.pc = site.pc;
    char symbolPayload[512] = {};
    uint64_t symbolPayloadBytes = 0;
    assert(ascsanSymbolizeDevicePc(
               &query, symbolPayload, sizeof(symbolPayload), &symbolPayloadBytes) ==
           ASCSAN_STATUS_SUCCESS);
    assert(symbolPayloadBytes > 0);
    assert(std::strstr(symbolPayload, "MTE2") != nullptr);
    assert(std::strstr(symbolPayload, "0x1030") != nullptr);

    AscsanRawTraceRecord record{};
    record.version = ASCSAN_API_VERSION;
    record.pipeline = ASCSAN_PATCH_PIPELINE_MTE2;
    record.siteId = 1;
    record.pc = 0x1010;
    record.arg0 = 0x1000;
    record.arg1 = 0x2000;
    record.arg2 = 16;
    assert(ascsanIngestRawTraces(&record, 1) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanStreamSynchronize(nullptr) == ASCSAN_STATUS_SUCCESS);
    assert(counters.mte2 >= 2);
    assert(counters.insideCallbackObserved == 1);

    void *dev = nullptr;
    assert(ascsanDeviceMalloc(&dev, 16) == ASCSAN_STATUS_SUCCESS);
    const char hostIn[16] = "ascsan-smoke";
    char hostOut[16] = {};
    assert(ascsanMemcpyH2D(dev, hostIn, sizeof(hostIn)) == ASCSAN_STATUS_SUCCESS);
    assert(ascsanMemcpyD2H(hostOut, dev, sizeof(hostOut)) == ASCSAN_STATUS_SUCCESS);
    assert(std::memcmp(hostIn, hostOut, sizeof(hostIn)) == 0);

    AscsanMemoryInfo memInfo{};
    assert(ascsanMemoryGetInfo(dev, &memInfo) == ASCSAN_STATUS_SUCCESS);
    assert(memInfo.bytes == 16);
    assert(ascsanDeviceFree(dev) == ASCSAN_STATUS_SUCCESS);

    assert(ascsanFinalize() == ASCSAN_STATUS_SUCCESS);
    return 0;
}
