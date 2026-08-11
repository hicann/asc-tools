/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "api_core.h"
#include "ascsan/internal_api.h"

#include <new>

using ascsan::ApiCore;

namespace {

template <typename Fn>
AscsanStatus GuardedStatusImpl(Fn&& fn) noexcept
{
    try {
        return fn();
    } catch (const std::bad_alloc&) {
        return ASCSAN_STATUS_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return ASCSAN_STATUS_ERROR_RUNTIME;
    }
}

} // namespace

#define ASCSAN_GUARDED_STATUS(expr) GuardedStatusImpl([&]() -> AscsanStatus { return (expr); })

extern "C" AscsanStatus ascsanInitialize(const AscsanInitParams* params)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().Initialize(params));
}

extern "C" AscsanStatus ascsanFinalize(void) { return ASCSAN_GUARDED_STATUS(ApiCore::Instance().Finalize()); }

extern "C" const char* ascsanGetVersionString(void) { return ApiCore::Instance().VersionString(); }

extern "C" AscsanStatus ascsanExportLaunchConfigToFd(const AscsanLaunchConfig* config, int fd)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().ExportLaunchConfigToFd(config, fd));
}

extern "C" AscsanStatus ascsanImportLaunchConfigFromFd(int fd, AscsanLaunchConfig* config)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().ImportLaunchConfigFromFd(fd, config));
}

extern "C" AscsanStatus ascsanApplyLaunchConfig(const AscsanLaunchConfig* config)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().ApplyLaunchConfig(config));
}

extern "C" const AscsanLaunchConfig* ascsanGetLaunchConfig(void) { return ApiCore::Instance().GetLaunchConfig(); }

extern "C" AscsanStatus ascsanSubscribe(const AscsanSubscribeDesc* desc, AscsanSubscriberHandle* subscriber)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().Subscribe(desc, subscriber));
}

extern "C" AscsanStatus ascsanUnsubscribe(AscsanSubscriberHandle subscriber)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().Unsubscribe(subscriber));
}

extern "C" AscsanStatus ascsanEnableCallback(
    AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, uint32_t cbid, int enable)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().EnableCallback(subscriber, domain, cbid, enable != 0));
}

extern "C" AscsanStatus ascsanEnableDomain(AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, int enable)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().EnableDomain(subscriber, domain, enable != 0));
}

extern "C" AscsanStatus ascsanGetCallbackState(
    AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, uint32_t cbid, int* enabled)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().GetCallbackState(subscriber, domain, cbid, enabled));
}

extern "C" int ascsanIsInsideCallback(void) { return ApiCore::Instance().IsInsideCallback() ? 1 : 0; }

extern "C" AscsanStatus ascsanRegisterBuiltinPatchPipelines(void)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().RegisterBuiltinPatchPipelines());
}

extern "C" AscsanStatus ascsanRegisterPatchImage(const AscsanPatchImageDesc* desc, uint64_t* patchImageId)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().RegisterPatchImage(desc, patchImageId));
}

extern "C" AscsanStatus ascsanRegisterPatchPipeline(const AscsanPatchPipelineDesc* desc)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().RegisterPatchPipeline(desc));
}

extern "C" AscsanStatus ascsanSetPatchOptions(const AscsanPatchOptions* options)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().SetPatchOptions(options));
}

extern "C" AscsanStatus ascsanBuildPatchPlanForBinary(AscsanBinaryHandle binary, AscsanPatchPlanHandle* plan)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().BuildPatchPlanForBinary(binary, plan));
}

extern "C" AscsanStatus ascsanPatchBinaryFromImage(
    const AscsanPatchImageDesc* image, const AscsanPatchOptions* options, char* patchedPath, uint64_t patchedPathSize,
    AscsanPatchPlanHandle* plan)
{
    return ASCSAN_GUARDED_STATUS(
        ApiCore::Instance().PatchBinaryFromImage(image, options, patchedPath, patchedPathSize, plan));
}

extern "C" AscsanStatus ascsanGetPatchSiteInfo(uint32_t siteId, AscsanPatchSiteInfo* info)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().GetPatchSiteInfo(siteId, info));
}

extern "C" AscsanStatus ascsanSymbolizeDevicePc(
    const AscsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().SymbolizeDevicePc(query, payload, payloadSize, payloadBytes));
}

extern "C" AscsanStatus ascsanSetLaunchUserData(
    AscsanLaunchHandle launch, void* function, void* stream, const void* deviceUserData, uint64_t deviceUserDataSize)
{
    return ASCSAN_GUARDED_STATUS(
        ApiCore::Instance().SetLaunchUserData(launch, function, stream, deviceUserData, deviceUserDataSize));
}

extern "C" AscsanStatus ascsanMemoryAlloc(const AscsanMemoryAllocDesc* desc, void** ptr, AscsanMemoryHandle* memory)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryAlloc(desc, ptr, memory));
}

extern "C" AscsanStatus ascsanMemoryFree(void* ptr)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryFree(ptr));
}

extern "C" AscsanStatus ascsanMemoryMemcpy(
    void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AscsanMemcpyKind kind)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemcpy(dst, dstMax, src, bytes, kind));
}

extern "C" AscsanStatus ascsanMemoryMemcpyAsync(
    void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AscsanMemcpyKind kind, void*)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemcpy(dst, dstMax, src, bytes, kind));
}

extern "C" AscsanStatus ascsanMemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemset(dst, dstMax, value, bytes));
}

extern "C" AscsanStatus ascsanMemoryMemsetAsync(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes, void*)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemset(dst, dstMax, value, bytes));
}

extern "C" AscsanStatus ascsanMemorySynchronizeStream(void* stream)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemorySynchronizeStream(stream));
}

extern "C" AscsanStatus ascsanMemoryGetInfo(const void* ptr, AscsanMemoryInfo* info)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryGetInfo(ptr, info));
}

extern "C" AscsanStatus ascsanDeviceMalloc(void** devPtr, uint64_t bytes)
{
    AscsanMemoryAllocDesc desc{};
    desc.version = ASCSAN_API_VERSION;
    desc.size = sizeof(desc);
    desc.space = ASCSAN_MEMORY_SPACE_DEVICE;
    desc.bytes = bytes;
    desc.flags = ASCSAN_MEMORY_FLAG_INTERNAL;
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryAlloc(&desc, devPtr, nullptr));
}

extern "C" AscsanStatus ascsanDeviceFree(void* devPtr)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemoryFree(devPtr));
}

extern "C" AscsanStatus ascsanMemcpyD2H(void* dstHost, const void* srcDevice, uint64_t bytes)
{
    return ASCSAN_GUARDED_STATUS(
        ApiCore::Instance().MemoryMemcpy(dstHost, bytes, srcDevice, bytes, ASCSAN_MEMCPY_DEVICE_TO_HOST));
}

extern "C" AscsanStatus ascsanMemcpyH2D(void* dstDevice, const void* srcHost, uint64_t bytes)
{
    return ASCSAN_GUARDED_STATUS(
        ApiCore::Instance().MemoryMemcpy(dstDevice, bytes, srcHost, bytes, ASCSAN_MEMCPY_HOST_TO_DEVICE));
}

extern "C" AscsanStatus ascsanStreamSynchronize(void* stream)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().MemorySynchronizeStream(stream));
}

extern "C" AscsanStatus ascsanOnRuntimeEvent(const AscsanRuntimeEvent* event)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().OnRuntimeEvent(event));
}

extern "C" AscsanStatus ascsanConfigureRuntimeHook(const AscsanRuntimeHookPlan* plan)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().ConfigureRuntimeHook(plan));
}

extern "C" AscsanStatus ascsanGetRuntimeHookState(AscsanRuntimeHookState* state)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().GetRuntimeHookState(state));
}

extern "C" AscsanStatus ascsanIngestRawTraces(const AscsanRawTraceRecord* records, uint64_t count)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().IngestRawTraces(records, count));
}

extern "C" AscsanStatus ascsanReportError(const char* tool, const char* message)
{
    return ASCSAN_GUARDED_STATUS(ApiCore::Instance().ReportError(tool, message));
}

extern "C" AscsanStatus ascsanFlushReports(void) { return ASCSAN_GUARDED_STATUS(ApiCore::Instance().FlushReports()); }

#undef ASCSAN_GUARDED_STATUS
