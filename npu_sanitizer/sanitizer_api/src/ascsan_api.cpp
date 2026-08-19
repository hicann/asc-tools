/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/ascsan_api_core.h"
#include "internal/ascsan_internal_api.h"

#include <new>

using aclsan::ApiCore;

namespace {

template <typename Fn>
AclsanStatus GuardedStatusImpl(Fn&& fn) noexcept
{
    try {
        return fn();
    } catch (const std::bad_alloc&) {
        return ACLSAN_STATUS_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return ACLSAN_STATUS_ERROR_RUNTIME;
    }
}

} // namespace

#define ACLSAN_GUARDED_STATUS(expr) GuardedStatusImpl([&]() -> AclsanStatus { return (expr); })

extern "C" const char* aclsanGetVersionString(void) { return ApiCore::Instance().VersionString(); }

extern "C" AclsanStatus aclsanExportLaunchConfigToFd(const AclsanLaunchConfig* config, int fd)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().ExportLaunchConfigToFd(config, fd));
}

extern "C" AclsanStatus aclsanImportLaunchConfigFromFd(int fd, AclsanLaunchConfig* config)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().ImportLaunchConfigFromFd(fd, config));
}

extern "C" AclsanStatus aclsanApplyLaunchConfig(const AclsanLaunchConfig* config)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().ApplyLaunchConfig(config));
}

extern "C" const AclsanLaunchConfig* aclsanGetLaunchConfig(void) { return ApiCore::Instance().GetLaunchConfig(); }

extern "C" AclsanStatus aclsanRegisterBuiltinPatchPipelines(void)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().RegisterBuiltinPatchPipelines());
}

extern "C" AclsanStatus aclsanRegisterPatchImage(const AclsanPatchImageDesc* desc, uint64_t* patchImageId)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().RegisterPatchImage(desc, patchImageId));
}

extern "C" AclsanStatus aclsanRegisterPatchPipeline(const AclsanPatchPipelineDesc* desc)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().RegisterPatchPipeline(desc));
}

extern "C" AclsanStatus aclsanSetPatchOptions(const AclsanPatchOptions* options)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().SetPatchOptions(options));
}

extern "C" AclsanStatus aclsanBuildPatchPlanForBinary(AclsanBinaryHandle binary, AclsanPatchPlanHandle* plan)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().BuildPatchPlanForBinary(binary, plan));
}

extern "C" AclsanStatus aclsanPatchBinaryFromImage(
    const AclsanPatchImageDesc* image, const AclsanPatchOptions* options, char* patchedPath, uint64_t patchedPathSize,
    AclsanPatchPlanHandle* plan)
{
    return ACLSAN_GUARDED_STATUS(
        ApiCore::Instance().PatchBinaryFromImage(image, options, patchedPath, patchedPathSize, plan));
}

extern "C" AclsanStatus aclsanGetPatchSiteInfo(uint32_t siteId, AclsanPatchSiteInfo* info)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().GetPatchSiteInfo(siteId, info));
}

extern "C" AclsanStatus aclsanSymbolizeDevicePc(
    const AclsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().SymbolizeDevicePc(query, payload, payloadSize, payloadBytes));
}

extern "C" AclsanStatus aclsanSetLaunchUserData(
    AclsanLaunchHandle launch, void* function, void* stream, const void* deviceUserData, uint64_t deviceUserDataSize)
{
    return ACLSAN_GUARDED_STATUS(
        ApiCore::Instance().SetLaunchUserData(launch, function, stream, deviceUserData, deviceUserDataSize));
}

extern "C" AclsanStatus aclsanMemoryAlloc(const AclsanMemoryAllocDesc* desc, void** ptr, AclsanMemoryHandle* memory)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryAlloc(desc, ptr, memory));
}

extern "C" AclsanStatus aclsanMemoryFree(void* ptr)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryFree(ptr));
}

extern "C" AclsanStatus aclsanMemoryMemcpy(
    void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AclsanMemcpyKind kind)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemcpy(dst, dstMax, src, bytes, kind));
}

extern "C" AclsanStatus aclsanMemoryMemcpyAsync(
    void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AclsanMemcpyKind kind, void*)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemcpy(dst, dstMax, src, bytes, kind));
}

extern "C" AclsanStatus aclsanMemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemset(dst, dstMax, value, bytes));
}

extern "C" AclsanStatus aclsanMemoryMemsetAsync(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes, void*)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryMemset(dst, dstMax, value, bytes));
}

extern "C" AclsanStatus aclsanMemorySynchronizeStream(void* stream)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemorySynchronizeStream(stream));
}

extern "C" AclsanStatus aclsanMemoryGetInfo(const void* ptr, AclsanMemoryInfo* info)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryGetInfo(ptr, info));
}

extern "C" AclsanStatus aclsanDeviceMalloc(void** devPtr, uint64_t bytes)
{
    AclsanMemoryAllocDesc desc{};
    desc.version = ACLSAN_API_VERSION;
    desc.size = sizeof(desc);
    desc.space = ACLSAN_MEMORY_SPACE_DEVICE;
    desc.bytes = bytes;
    desc.flags = ACLSAN_MEMORY_FLAG_INTERNAL;
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryAlloc(&desc, devPtr, nullptr));
}

extern "C" AclsanStatus aclsanDeviceFree(void* devPtr)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemoryFree(devPtr));
}

extern "C" AclsanStatus aclsanMemcpyD2H(void* dstHost, const void* srcDevice, uint64_t bytes)
{
    return ACLSAN_GUARDED_STATUS(
        ApiCore::Instance().MemoryMemcpy(dstHost, bytes, srcDevice, bytes, ACLSAN_MEMCPY_DEVICE_TO_HOST));
}

extern "C" AclsanStatus aclsanMemcpyH2D(void* dstDevice, const void* srcHost, uint64_t bytes)
{
    return ACLSAN_GUARDED_STATUS(
        ApiCore::Instance().MemoryMemcpy(dstDevice, bytes, srcHost, bytes, ACLSAN_MEMCPY_HOST_TO_DEVICE));
}

extern "C" AclsanStatus aclsanStreamSynchronize(void* stream)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().MemorySynchronizeStream(stream));
}

extern "C" AclsanStatus aclsanOnRuntimeEvent(const AclsanRuntimeEvent* event)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().OnRuntimeEvent(event));
}

extern "C" AclsanStatus aclsanConfigureRuntimeHook(const AclsanRuntimeHookPlan* plan)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().ConfigureRuntimeHook(plan));
}

extern "C" AclsanStatus aclsanGetRuntimeHookState(AclsanRuntimeHookState* state)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().GetRuntimeHookState(state));
}

extern "C" AclsanStatus aclsanIngestRawTraces(const AclsanRawTraceRecord* records, uint64_t count)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().IngestRawTraces(records, count));
}

extern "C" AclsanStatus aclsanReportError(const char* tool, const char* message)
{
    return ACLSAN_GUARDED_STATUS(ApiCore::Instance().ReportError(tool, message));
}

extern "C" AclsanStatus aclsanFlushReports(void) { return ACLSAN_GUARDED_STATUS(ApiCore::Instance().FlushReports()); }

#undef ACLSAN_GUARDED_STATUS
