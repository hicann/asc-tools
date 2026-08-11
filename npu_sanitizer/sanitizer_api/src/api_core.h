/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_API_CORE_H
#define ASCSAN_API_CORE_H

#include "ascsan/ascsan_api.h"
#include "ascsan/ascsan_callback.h"
#include "ascsan/ascsan_memory.h"
#include "ascsan/ascsan_patch.h"
#include "ascsan/internal_api.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct AscsanSubscriberToken_st {
    uint64_t magic = 0;
    uint64_t generation = 0;
    bool active = false;
};

namespace ascsan {

struct Subscriber {
    AscsanSubscriberHandle handle = ASCSAN_INVALID_SUBSCRIBER_HANDLE;
    std::string name;
    AscsanCallbackFunc callback = nullptr;
    void* userdata = nullptr;
    uint64_t flags = 0;
    std::set<AscsanCallbackDomain> enabledDomains;
    std::set<std::pair<AscsanCallbackDomain, uint32_t>> enabledCallbacks;
};

struct PatchSiteRecord {
    AscsanPatchSiteInfo info{};
    std::string functionName;
    std::string opName;
    std::string sourceFile;
};

struct PatchResult {
    bool patched = false;
    std::string patchedPath;
    uint64_t patchPlanId = 0;
    uint32_t pipelineMask = 0;
    std::vector<PatchSiteRecord> sites;
};

struct MemoryRecord {
    AscsanMemoryInfo info{};
};

class ApiCore {
public:
    static ApiCore& Instance();

    AscsanStatus Initialize(const AscsanInitParams* params);
    AscsanStatus Finalize();
    const char* VersionString() const;

    AscsanStatus ExportLaunchConfigToFd(const AscsanLaunchConfig* config, int fd);
    AscsanStatus ImportLaunchConfigFromFd(int fd, AscsanLaunchConfig* config);
    AscsanStatus ApplyLaunchConfig(const AscsanLaunchConfig* config);
    const AscsanLaunchConfig* GetLaunchConfig() const;

    AscsanStatus Subscribe(const AscsanSubscribeDesc* desc, AscsanSubscriberHandle* subscriber);
    AscsanStatus Unsubscribe(AscsanSubscriberHandle subscriber);
    AscsanStatus EnableCallback(
        AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, uint32_t cbid, bool enable);
    AscsanStatus EnableDomain(AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, bool enable);
    AscsanStatus GetCallbackState(
        AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, uint32_t cbid, int* enabled) const;
    bool IsInsideCallback() const;

    AscsanStatus RegisterBuiltinPatchPipelines();
    AscsanStatus RegisterPatchImage(const AscsanPatchImageDesc* desc, uint64_t* patchImageId);
    AscsanStatus RegisterPatchPipeline(const AscsanPatchPipelineDesc* desc);
    AscsanStatus SetPatchOptions(const AscsanPatchOptions* options);
    AscsanStatus BuildPatchPlanForBinary(AscsanBinaryHandle binary, AscsanPatchPlanHandle* plan);
    AscsanStatus PatchBinaryFromImage(
        const AscsanPatchImageDesc* image, const AscsanPatchOptions* options, char* patchedPath,
        uint64_t patchedPathSize, AscsanPatchPlanHandle* plan);
    AscsanStatus GetPatchSiteInfo(uint32_t siteId, AscsanPatchSiteInfo* info) const;
    AscsanStatus SymbolizeDevicePc(
        const AscsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes) const;
    AscsanStatus SetLaunchUserData(
        AscsanLaunchHandle launch, void* function, void* stream, const void* deviceUserData,
        uint64_t deviceUserDataSize);

    AscsanStatus MemoryAlloc(const AscsanMemoryAllocDesc* desc, void** ptr, AscsanMemoryHandle* memory);
    AscsanStatus MemoryFree(void* ptr);
    AscsanStatus MemoryMemcpy(void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AscsanMemcpyKind kind);
    AscsanStatus MemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes);
    AscsanStatus MemorySynchronizeStream(void* stream);
    AscsanStatus MemoryGetInfo(const void* ptr, AscsanMemoryInfo* info) const;

    AscsanStatus OnRuntimeEvent(const AscsanRuntimeEvent* event);
    AscsanStatus ConfigureRuntimeHook(const AscsanRuntimeHookPlan* plan);
    AscsanStatus GetRuntimeHookState(AscsanRuntimeHookState* state) const;
    AscsanStatus IngestRawTraces(const AscsanRawTraceRecord* records, uint64_t count);
    AscsanStatus ReportError(const char* tool, const char* message);
    AscsanStatus FlushReports();

    AscsanRuntimeHookPlan BuildHookPlanFromSubscriptions();
    void ReconfigureHookPlan();
    uint32_t ActivePatchPipelineMask() const;

    void Dispatch(AscsanCallbackDomain domain, uint32_t cbid, const void* cbdata);

private:
    ApiCore() = default;

    bool HasEnabledCallbackLocked(AscsanCallbackDomain domain, uint32_t cbid) const;
    bool HasEnabledDomainLocked(AscsanCallbackDomain domain) const;
    bool IsSupportedDomain(AscsanCallbackDomain domain) const;
    bool IsKnownCbid(AscsanCallbackDomain domain, uint32_t cbid) const;
    AscsanSubscriberToken_st* ValidateSubscriberLocked(AscsanSubscriberHandle subscriber) const;
    AscsanStatus ValidateInitialized() const;
    AscsanStatus BuildDummyPatchResult(const std::string& originalPath, uint32_t pipelineMask, PatchResult* result);
    void StorePatchSites(uint64_t binaryId, const std::vector<PatchSiteRecord>& sites);
    std::vector<AscsanRawTraceRecord> BuildSyntheticRecordsForSync() const;

    mutable std::recursive_mutex mutex_;
    bool initialized_ = false;
    bool finalized_ = false;
    AscsanLaunchConfig config_{};

    uint64_t nextSubscriberGeneration_ = 1;
    uint64_t nextPatchImage_ = 1;
    uint64_t nextPatchPlan_ = 1;
    uint64_t nextBinary_ = 1;
    uint64_t nextMemory_ = 1;
    uint64_t nextHookGeneration_ = 1;

    std::optional<Subscriber> subscriber_;
    std::unique_ptr<AscsanSubscriberToken_st> subscriberToken_;
    std::vector<std::unique_ptr<AscsanSubscriberToken_st>> retiredSubscriberTokens_;
    std::map<uint64_t, AscsanPatchImageDesc> patchImages_;
    std::map<AscsanPatchPipeline, AscsanPatchPipelineDesc> patchPipelines_;
    std::map<uint32_t, PatchSiteRecord> patchSites_;
    std::map<void*, MemoryRecord> memories_;
    std::vector<std::string> reports_;
    AscsanPatchOptions patchOptions_{};
    AscsanRuntimeHookPlan activeHookPlan_{};
};

const char* PipelineName(AscsanPatchPipeline pipeline);
uint32_t PipelineMask(AscsanPatchPipeline pipeline);
uint32_t PipelineToCbid(AscsanPatchPipeline pipeline);

} // namespace ascsan

#endif
