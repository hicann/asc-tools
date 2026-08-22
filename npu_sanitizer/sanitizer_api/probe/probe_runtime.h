/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_PROBE_RUNTIME_H_
#define ACLSAN_PROBE_RUNTIME_H_

#include "image_transformer.h"
#include "npu_compute/injection_hook.h"
#include "device_symbolizer.h"
#include "probe_parser.h"

#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace aclsan::probe {

using AclrtBinaryGetGlobalFunc =
    aclError (*)(aclrtBinHandle binHandle, const char* name, void** address, size_t* bytes);
using AclrtGetFunctionAttributeFunc =
    aclError (*)(aclrtFuncHandle funcHandle, aclrtFuncAttribute attrType, int64_t* attrValue);

struct ProbeRuntimeApi {
    aclrtBinaryLoadFromDataFunc binaryLoadFromData = nullptr;
    aclrtBinaryGetFunctionFunc binaryGetFunction = nullptr;
    aclrtBinaryGetFunctionByEntryFunc binaryGetFunctionByEntry = nullptr;
    AclrtBinaryGetGlobalFunc binaryGetGlobal = nullptr;
    AclrtGetFunctionAttributeFunc getFunctionAttribute = nullptr;
    aclrtMallocFunc mallocDevice = nullptr;
    aclrtFreeFunc freeDevice = nullptr;
    aclrtMemcpyFunc memcpy = nullptr;
};

class ProbeRuntime final {
public:
    aclError LoadBinary(
        const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle,
        const ImageTransformConfig& config, const ProbeRuntimeApi& api, ImageTransformFunction transform,
        std::string& error);

    aclError GetFunction(
        aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle, const ProbeRuntimeApi& api);

    aclError GetFunctionByEntry(
        aclrtBinHandle binHandle, uint64_t functionEntry, aclrtFuncHandle* funcHandle, const ProbeRuntimeApi& api);

    void RecordFunction(aclrtFuncHandle function);
    bool IsTargetFunction(aclrtFuncHandle function) const;
    aclError PrepareLaunch(
        aclrtFuncHandle function, uint32_t blockCount, aclrtStream stream, const ProbeRuntimeApi& api);
    void RecordLaunchResult(aclrtFuncHandle function, aclrtStream stream, aclError result);
    aclError Collect(aclrtStream stream, const ProbeRuntimeApi& api, sanitizer::ProbeParseResult& result);
    aclError Clear(const ProbeRuntimeApi& api);
    CallStackResult ResolveCallStack(uint64_t pc) const;
    CallStackResult ResolveCallStackWithRunner(uint64_t pc, const CommandRunner& runner) const;
    bool OwnsBinary(aclrtBinHandle binHandle) const;
    bool HasPending(aclrtStream stream) const;

private:
    aclError BindGlobal(const ProbeRuntimeApi& api, const char* name, const void* value, size_t valueBytes);

    mutable std::mutex mutex_;
    aclrtBinHandle binary_ = nullptr;
    std::set<aclrtFuncHandle> functions_;
    ImageTransformResult transformed_;
    std::unique_ptr<DeviceSymbolizer> symbolizer_;
    uint64_t binaryId_ = 0;
    uint64_t nextBinaryId_ = 1;
    void* probeOutput_ = nullptr;
    size_t probeBytes_ = 0;
    uint32_t blockCount_ = 0;
    uint32_t aivOffset_ = 0;
    aclrtStream pendingStream_ = nullptr;
    bool pending_ = false;
};

} // namespace aclsan::probe

#endif // ACLSAN_PROBE_RUNTIME_H_
