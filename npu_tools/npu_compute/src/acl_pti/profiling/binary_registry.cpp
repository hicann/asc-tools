/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "binary_registry.h"
#include "binary_instrumenter.h"
#include "common/debug_log.h"

#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aclpti::profiling {

template <typename Function, typename Key>
aclptiResult BinaryRegistry::ResolveBinaryFunction(
    aclrtApiId id, aclrtBinHandle binary, Key key, aclrtFuncHandle function)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = binaries_.find(binary);
    if (found == binaries_.end()) {
        npucompute::detail::DebugLog(
            "aclpti", "function mapping skipped: binary=%p function=%p is not registered", binary, function);
        return ACLPTI_SUCCESS;
    }
    const auto resolve = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(id));
    if (resolve == nullptr || found->second.companion == nullptr || function == nullptr) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    aclrtFuncHandle companion = nullptr;
    const aclError status = resolve(found->second.companion, key, &companion);
    if (status != ACL_SUCCESS || companion == nullptr) {
        found->second.functions.erase(function);
        npucompute::detail::DebugLog("aclpti", "error operation=companion_function status=%d", status);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    try {
        found->second.functions[function] = companion;
        npucompute::detail::DebugLog(
            "aclpti", "function mapping registered: binary=%p original=%p companion=%p", binary, function, companion);
    } catch (const std::bad_alloc&) {
        return ACLPTI_ERROR_OUT_OF_MEMORY;
    }
    return ACLPTI_SUCCESS;
}

aclptiResult BinaryRegistry::RegisterBinary(
    const void* data, std::size_t size, const aclrtBinaryLoadOptions* options, aclrtBinHandle binary)
{
    const auto load =
        reinterpret_cast<aclrtBinaryLoadFromDataFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtBinaryLoadFromData));
    if (load == nullptr || binary == nullptr || data == nullptr || size == 0) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    try {
        std::vector<char> patched;
        if (!InstrumentKernelEnd(data, size, patched) || patched.empty()) {
            return ACLPTI_ERROR_PROFILING_FAILED;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        const auto inserted = binaries_.emplace(binary, Binary{nullptr, {}, std::move(patched)});
        if (!inserted.second) {
            return ACLPTI_ERROR_PROFILING_FAILED;
        }
        auto& state = inserted.first->second;
        const aclError status =
            load(state.instrumentedImage.data(), state.instrumentedImage.size(), options, &state.companion);
        if (status != ACL_SUCCESS) {
            npucompute::detail::DebugLog("aclpti", "error operation=companion_load status=%d", status);
            binaries_.erase(inserted.first);
            return ACLPTI_ERROR_PROFILING_FAILED;
        }
        return ACLPTI_SUCCESS;
    } catch (const std::bad_alloc&) {
        return ACLPTI_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
}

aclptiResult BinaryRegistry::RegisterBinaryFunction(aclrtBinHandle binary, const char* name, aclrtFuncHandle function)
{
    return ResolveBinaryFunction<aclrtBinaryGetFunctionFunc>(ACL_RT_API_aclrtBinaryGetFunction, binary, name, function);
}

aclptiResult BinaryRegistry::RegisterBinaryFunction(
    aclrtBinHandle binary, std::uint64_t entry, aclrtFuncHandle function)
{
    return ResolveBinaryFunction<aclrtBinaryGetFunctionByEntryFunc>(
        ACL_RT_API_aclrtBinaryGetFunctionByEntry, binary, entry, function);
}

aclptiResult BinaryRegistry::RegisterSymbolFunction(aclrtFuncHandle function)
{
    // Symbol lookup returns a Runtime handle, not a name/entry in the companion.
    // Ask Runtime for ownership to avoid matching a same-named kernel in another binary.
    aclrtBinHandle binary = nullptr;
    const aclError binaryStatus = aclrtFunctionGetBinary(function, &binary);
    if (binaryStatus != ACL_SUCCESS || binary == nullptr) {
        npucompute::detail::DebugLog(
            "aclpti", "error operation=symbol_function_binary status=%d function=%p", binaryStatus, function);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    char name[4096]{};
    const aclError nameStatus = aclrtGetFunctionName(function, sizeof(name), name);
    if (nameStatus != ACL_SUCCESS || name[0] == '\0' || name[sizeof(name) - 1] != '\0') {
        npucompute::detail::DebugLog(
            "aclpti", "error operation=symbol_function_name status=%d function=%p", nameStatus, function);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    return RegisterBinaryFunction(binary, name, function);
}

aclrtFuncHandle BinaryRegistry::FindInstrumentedFunction(aclrtFuncHandle original)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& binary : binaries_) {
        const auto found = binary.second.functions.find(original);
        if (found != binary.second.functions.end()) {
            return found->second;
        }
    }
    npucompute::detail::DebugLog("aclpti", "pipeline function missing; enable pipeline before binary load");
    return nullptr;
}

aclptiResult BinaryRegistry::PrepareBinaryUnload(aclrtBinHandle binary, UnloadContext& context)
{
    context.lock_ = std::unique_lock<std::mutex>(mutex_);
    context.binary_ = binary;
    const auto found = binaries_.find(binary);
    if (found == binaries_.end() || found->second.companion == nullptr) {
        return ACLPTI_SUCCESS;
    }
    const auto unload =
        reinterpret_cast<aclrtBinaryUnLoadFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtBinaryUnLoad));
    const aclError status = unload == nullptr ? ACL_ERROR_INTERNAL_ERROR : unload(found->second.companion);
    if (status != ACL_SUCCESS) {
        npucompute::detail::DebugLog("aclpti", "error operation=companion_unload status=%d", status);
        context.lock_.unlock();
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    found->second.companion = nullptr;
    found->second.functions.clear();
    // Keep the lock across the handler's original unload and registry removal:
    // another load may receive the same Runtime handle as soon as unload succeeds.
    return ACLPTI_SUCCESS;
}

aclptiResult BinaryRegistry::CompleteBinaryUnload(UnloadContext& context)
{
    binaries_.erase(context.binary_);
    context.lock_.unlock();
    return ACLPTI_SUCCESS;
}
} // namespace aclpti::profiling
