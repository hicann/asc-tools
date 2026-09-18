/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#pragma once

#include "injection/injection_hook.h"
#include "aclpti/aclpti_types.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace aclpti::profiling {

class BinaryRegistry {
public:
    // Holds the registry lock through the original Runtime unload. Destroy without
    // completing on original failure; complete only after original unload succeeds.
    class UnloadContext {
        friend class BinaryRegistry;
        std::unique_lock<std::mutex> lock_;
        aclrtBinHandle binary_ = nullptr;
    };
    aclptiResult RegisterBinary(
        const void* data, std::size_t size, const aclrtBinaryLoadOptions* options, aclrtBinHandle binary);
    aclptiResult RegisterBinaryFunction(aclrtBinHandle binary, const char* name, aclrtFuncHandle function);
    aclptiResult RegisterBinaryFunction(aclrtBinHandle binary, std::uint64_t entry, aclrtFuncHandle function);
    aclptiResult RegisterSymbolFunction(aclrtFuncHandle function);
    aclptiResult PrepareBinaryUnload(aclrtBinHandle binary, UnloadContext& context);
    aclptiResult CompleteBinaryUnload(UnloadContext& context);
    // Null means the function was not loaded with pipeline instrumentation enabled.
    aclrtFuncHandle FindInstrumentedFunction(aclrtFuncHandle original);

private:
    struct Binary {
        aclrtBinHandle companion = nullptr;
        std::unordered_map<aclrtFuncHandle, aclrtFuncHandle> functions;
        // Runtime may retain the image pointer; keep it alive through companion unload.
        std::vector<char> instrumentedImage;
    };
    template <typename Function, typename Key>
    aclptiResult ResolveBinaryFunction(aclrtApiId id, aclrtBinHandle binary, Key key, aclrtFuncHandle function);
    std::mutex mutex_;
    std::unordered_map<aclrtBinHandle, Binary> binaries_;
};

} // namespace aclpti::profiling
