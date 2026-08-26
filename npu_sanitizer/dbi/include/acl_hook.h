// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#pragma once

#include "dbi_pipeline.h"

#include <acl/acl_rt.h>

#include <cstdint>
#include <string>
#include <vector>

namespace aclsan {

struct AclHookInstallOptions {
    const char* arch;
    uint32_t argSize;
    uint32_t probeGroupMask;
    const char* toolchainRoot;
    const char* sourceRoot;
    const char* workDirectory;
    const char* cacheDirectory;
    uint8_t strict;
    uint8_t keepTemp;
    const char* const* compilerArgs;
    size_t compilerArgCount;
};

using OriginalDataLoad = aclError (*)(const void*, size_t, const aclrtBinaryLoadOptions*, aclrtBinHandle*);
using PatchRunner = DbiResult (*)(const DbiRequest&, void*);

struct AclHookConfig {
    std::string arch;
    uint32_t argSize = 0;
    std::vector<ProbeGroup> probeGroups;
    std::string toolchainRoot;
    std::string sourceRoot;
    std::string workDirectory;
    std::string cacheDirectory;
    bool strict = false;
    bool keepTemp = false;
    std::vector<std::string> compilerArgs;
    std::vector<std::string> tuneArgs;
};

AclHookConfig DefaultHookConfig();
AclHookConfig DefaultHookConfig(uint32_t probeGroupMask);
bool IsBinaryLoadHookReentrant() noexcept;
bool InstallAclHooks(const AclHookConfig& config, std::string& error);
bool UninstallAclHooks();

// This entry point is also used by tests to exercise fallback semantics without a CANN runtime.
aclError HandleBinaryLoadFromData(
    const AclHookConfig& config, const void* data, size_t length, const aclrtBinaryLoadOptions* options,
    aclrtBinHandle* handle, OriginalDataLoad original, PatchRunner runner, void* runnerData,
    bool* loadedPatched = nullptr);
aclError HandleBinaryLoadFromDataWithDefaultConfig(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* handle,
    OriginalDataLoad original, uint32_t probeGroupMask, bool* loadedPatched = nullptr) noexcept;

} // namespace aclsan

extern "C" int NpuCheckInstallAclHooks(const aclsan::AclHookInstallOptions* options, char* error, size_t errorCapacity);
extern "C" int NpuCheckUninstallAclHooks();
