/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_CANN_SANITIZER_CONTEXT_H
#define ASCSAN_CANN_SANITIZER_CONTEXT_H

#include "ascsan/cann_sanitizer.h"
#include "ascsan/internal_api.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace ascsan::cann {

enum class ToolKind { Memcheck, Racecheck, Initcheck, Synccheck };

struct CallbackSelector {
    AscsanCallbackDomain domain;
    uint32_t cbid;
};

struct ToolProfile {
    ToolKind kind;
    const char* name;
    const CallbackSelector* callbacks;
    uint64_t callbackCount;
};

struct ParsedInstruction {
    enum class Kind { Unknown, MemoryTransfer, Fixpipe, SyncFlag, BufferLifetime };

    Kind kind = Kind::Unknown;
    AscsanPatchPipeline pipeline = ASCSAN_PATCH_PIPELINE_INVALID;
    uint32_t cbid = 0;
    uint32_t siteId = 0;
    uint64_t pc = 0;
    uint64_t src = 0;
    uint64_t dst = 0;
    uint64_t bytes = 0;
    uint64_t aux0 = 0;
    uint64_t aux1 = 0;
    std::string op;
};

struct ToolEventSite {
    uint32_t siteId = 0;
    AscsanPatchPipeline pipeline = ASCSAN_PATCH_PIPELINE_INVALID;
    uint64_t binaryId = 0;
    uint64_t functionId = 0;
    uint64_t pc = 0;
    std::string functionName;
    std::string opName;
    std::string sourceFile;
    uint32_t sourceLine = 0;
};

struct ToolEvent {
    AscsanCallbackDomain domain = ASCSAN_CB_DOMAIN_ERROR;
    uint32_t cbid = 0;
    std::string apiName;
    int result = 0;
    uint64_t correlationId = 0;

    bool hasResource = false;
    AscsanResourceData resource{};

    bool hasMemory = false;
    AscsanMemoryMemcpyData memory{};

    bool hasPatch = false;
    AscsanPatchData patch{};
    std::string patchOriginalPath;
    std::string patchPatchedPath;

    bool hasInstruction = false;
    AscsanDeviceInstructionData instruction{};
    ParsedInstruction parsed{};
    bool hasSite = false;
    ToolEventSite site{};
};

struct CheckWindowKey {
    uint64_t launchId = 0;
    uint64_t binaryId = 0;
    uint64_t functionId = 0;
    uint32_t blockId = 0;

    bool operator<(const CheckWindowKey& other) const;
};

struct AllocationRecord {
    uint64_t resourceId = 0;
    uint64_t ptr = 0;
    uint64_t bytes = 0;
    uint32_t memorySpace = 0;
    uint32_t deviceId = 0;
};

struct CheckWindow {
    std::vector<ToolEvent> mte2;
    std::vector<ToolEvent> mte3;
    std::vector<ToolEvent> fixpipe;
    std::vector<ToolEvent> setWaitFlag;
    std::vector<ToolEvent> getRlsBuf;
};

struct ToolContext;

class DummyChecker {
public:
    void Configure(ToolKind tool);
    void Reset();
    void OnCallback(ToolContext& ctx, const ToolEvent& event);
    void FlushAll(ToolContext& ctx, const char* reason);

private:
    struct HandlerKey {
        AscsanCallbackDomain domain = ASCSAN_CB_DOMAIN_ERROR;
        uint32_t cbid = 0;

        bool operator<(const HandlerKey& other) const;
    };

    using Handler = void (DummyChecker::*)(ToolContext&, const ToolEvent&);

    void Register(AscsanCallbackDomain domain, uint32_t cbid, Handler handler);
    void RegisterMemcheck();
    void RegisterRacecheck();
    void RegisterInitcheck();
    void RegisterSynccheck();

    void OnResourceAlloc(ToolContext& ctx, const ToolEvent& event);
    void OnResourceFree(ToolContext& ctx, const ToolEvent& event);
    void OnMemoryOp(ToolContext& ctx, const ToolEvent& event);
    void OnPatch(ToolContext& ctx, const ToolEvent& event);
    void OnInstruction(ToolContext& ctx, const ToolEvent& event);
    void OnSynchronize(ToolContext& ctx, const ToolEvent& event);
    void OnGeneric(ToolContext& ctx, const ToolEvent& event);

    CheckWindowKey MakeWindowKey(const ToolEvent& event) const;
    CheckWindow& GetWindow(ToolContext& ctx, const CheckWindowKey& key);
    void CompleteWindow(ToolContext& ctx, const CheckWindowKey& key, const CheckWindow& window, const char* reason);
    void AddReport(ToolContext& ctx, const std::string& message);

    ToolKind tool_ = ToolKind::Memcheck;
    std::map<HandlerKey, Handler> handlers_;
    std::map<uint64_t, AllocationRecord> allocations_;
    std::map<CheckWindowKey, CheckWindow> windows_;
    std::vector<ToolEvent> memoryOps_;
    std::vector<ToolEvent> patchEvents_;
    std::vector<std::string> reports_;
};

struct ToolContext {
    std::mutex mutex;
    bool initialized = false;
    ToolKind tool = ToolKind::Memcheck;
    AscsanLaunchConfig config{};
    AscsanSubscriberHandle subscriber = ASCSAN_INVALID_SUBSCRIBER_HANDLE;
    AscsanCannSanitizerStats stats{};
    DummyChecker checker;
    std::ofstream log;
};

ToolContext& Context();
const ToolProfile* FindToolProfile(const char* toolName);
AscsanStatus EnableToolProfile(ToolContext& ctx, const ToolProfile& profile);
ParsedInstruction ParseInstruction(const AscsanDeviceInstructionData& instruction);
void DispatchToolCallback(void* userdata, AscsanCallbackDomain domain, uint32_t cbid, const void* cbdata);
void Log(ToolContext& ctx, const std::string& message);
const char* ToolKindName(ToolKind kind);

} // namespace ascsan::cann

#endif
