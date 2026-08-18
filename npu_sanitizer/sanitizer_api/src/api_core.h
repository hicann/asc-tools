/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_API_CORE_H
#define ACLSAN_API_CORE_H

#include "aclsan/aclsan_api.h"
#include "aclsan/aclsan_callback.h"
#include "aclsan/aclsan_memory.h"
#include "aclsan/aclsan_patch.h"
#include "aclsan/internal_api.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct AclsanSubscriberToken_st {
    uint64_t magic = 0;
    uint64_t generation = 0;
    bool active = false;
};

namespace aclsan {

struct Subscriber {
    AclsanSubscriberHandle handle = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
    std::string name;
    AclsanCallbackFunc callback = nullptr;
    void* userdata = nullptr;
    uint64_t flags = 0;
    std::set<AclsanCallbackDomain> enabledDomains;
    std::set<std::pair<AclsanCallbackDomain, uint32_t>> enabledCallbacks;
};

struct PatchSiteRecord {
    AclsanPatchSiteInfo info{};
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
    AclsanMemoryInfo info{};
};

class ApiCore {
public:
    static ApiCore& Instance();

    AclsanStatus Initialize(const AclsanInitParams* params);
    AclsanStatus Finalize();
    const char* VersionString() const;

    AclsanStatus ExportLaunchConfigToFd(const AclsanLaunchConfig* config, int fd);
    AclsanStatus ImportLaunchConfigFromFd(int fd, AclsanLaunchConfig* config);
    AclsanStatus ApplyLaunchConfig(const AclsanLaunchConfig* config);
    const AclsanLaunchConfig* GetLaunchConfig() const;

    AclsanStatus Subscribe(const AclsanSubscribeDesc* desc, AclsanSubscriberHandle* subscriber);
    AclsanStatus Unsubscribe(AclsanSubscriberHandle subscriber);
    AclsanStatus EnableCallback(
        AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, bool enable);
    AclsanStatus EnableDomain(AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, bool enable);
    AclsanStatus GetCallbackState(
        AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, int* enabled) const;
    bool IsInsideCallback() const;

    AclsanStatus RegisterBuiltinPatchPipelines();
    AclsanStatus RegisterPatchImage(const AclsanPatchImageDesc* desc, uint64_t* patchImageId);
    AclsanStatus RegisterPatchPipeline(const AclsanPatchPipelineDesc* desc);
    AclsanStatus SetPatchOptions(const AclsanPatchOptions* options);
    AclsanStatus BuildPatchPlanForBinary(AclsanBinaryHandle binary, AclsanPatchPlanHandle* plan);
    AclsanStatus PatchBinaryFromImage(
        const AclsanPatchImageDesc* image, const AclsanPatchOptions* options, char* patchedPath,
        uint64_t patchedPathSize, AclsanPatchPlanHandle* plan);
    AclsanStatus GetPatchSiteInfo(uint32_t siteId, AclsanPatchSiteInfo* info) const;
    AclsanStatus SymbolizeDevicePc(
        const AclsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes) const;
    AclsanStatus SetLaunchUserData(
        AclsanLaunchHandle launch, void* function, void* stream, const void* deviceUserData,
        uint64_t deviceUserDataSize);

    AclsanStatus MemoryAlloc(const AclsanMemoryAllocDesc* desc, void** ptr, AclsanMemoryHandle* memory);
    AclsanStatus MemoryFree(void* ptr);
    AclsanStatus MemoryMemcpy(void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AclsanMemcpyKind kind);
    AclsanStatus MemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes);
    AclsanStatus MemorySynchronizeStream(void* stream);
    AclsanStatus MemoryGetInfo(const void* ptr, AclsanMemoryInfo* info) const;

    AclsanStatus OnRuntimeEvent(const AclsanRuntimeEvent* event);
    AclsanStatus ConfigureRuntimeHook(const AclsanRuntimeHookPlan* plan);
    AclsanStatus GetRuntimeHookState(AclsanRuntimeHookState* state) const;
    AclsanStatus IngestRawTraces(const AclsanRawTraceRecord* records, uint64_t count);
    AclsanStatus ReportError(const char* tool, const char* message);
    AclsanStatus FlushReports();

    AclsanRuntimeHookPlan BuildHookPlanFromSubscriptions();
    void ReconfigureHookPlan();
    uint32_t ActivePatchPipelineMask() const;

    void Dispatch(AclsanCallbackDomain domain, uint32_t cbid, const void* cbdata);

private:
    ApiCore() = default;

    bool HasEnabledCallbackLocked(AclsanCallbackDomain domain, uint32_t cbid) const;
    bool HasEnabledDomainLocked(AclsanCallbackDomain domain) const;
    bool IsSupportedDomain(AclsanCallbackDomain domain) const;
    bool IsKnownCbid(AclsanCallbackDomain domain, uint32_t cbid) const;
    Subscriber* FindSubscriberLocked(AclsanSubscriberHandle subscriber);
    const Subscriber* FindSubscriberLocked(AclsanSubscriberHandle subscriber) const;
    AclsanStatus ValidateInitialized() const;
    AclsanStatus BuildDummyPatchResult(const std::string& originalPath, uint32_t pipelineMask, PatchResult& result);
    void StorePatchSites(uint64_t binaryId, const std::vector<PatchSiteRecord>& sites);
    mutable std::recursive_mutex mutex_;
    bool initialized_ = false;
    bool finalized_ = false;
    AclsanLaunchConfig config_{};

    uint64_t nextSubscriberGeneration_ = 1;
    uint64_t nextPatchImage_ = 1;
    uint64_t nextPatchPlan_ = 1;
    uint64_t nextBinary_ = 1;
    uint64_t nextMemory_ = 1;
    uint64_t nextHookGeneration_ = 1;

    std::optional<Subscriber> subscriber_;
    std::unique_ptr<AclsanSubscriberToken_st> subscriberToken_;
    std::vector<std::unique_ptr<AclsanSubscriberToken_st>> retiredSubscriberTokens_;
    std::map<uint64_t, AclsanPatchImageDesc> patchImages_;
    std::map<AclsanPatchPipeline, AclsanPatchPipelineDesc> patchPipelines_;
    std::map<uint32_t, PatchSiteRecord> patchSites_;
    std::map<void*, MemoryRecord> memories_;
    std::vector<std::string> reports_;
    AclsanPatchOptions patchOptions_{};
    AclsanRuntimeHookPlan activeHookPlan_{};
};

const char* PipelineName(AclsanPatchPipeline pipeline);
uint32_t PipelineMask(AclsanPatchPipeline pipeline);
uint32_t PipelineToCbid(AclsanPatchPipeline pipeline);

} // namespace aclsan

#endif
