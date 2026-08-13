/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cann_sanitizer_context.h"

namespace aclsan::cann {
namespace {

constexpr CallbackSelector kMemcheckCallbacks[] = {
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE},
    {ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMCPY_BEGIN},
    {ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMCPY_END},
    {ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMSET_BEGIN},
    {ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMSET_END},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_BEGIN},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_END},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_BEGIN},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_BEGIN},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS},
    {ACLSAN_CB_DOMAIN_ERROR, ACLSAN_CBID_ERROR_RECORD},
};

constexpr CallbackSelector kInitcheckCallbacks[] = {
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE},
    {ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMCPY_END},
    {ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMSET_END},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_BEGIN},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_END},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_BEGIN},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_BEGIN},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS},
};

constexpr CallbackSelector kRacecheckCallbacks[] = {
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_BEGIN},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_END},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_BEGIN},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_BEGIN},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS},
};

constexpr CallbackSelector kSynccheckCallbacks[] = {
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_BEGIN},
    {ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_END},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_BEGIN},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_BEGIN},
    {ACLSAN_CB_DOMAIN_LAUNCH, ACLSAN_CBID_LAUNCH_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC},
    {ACLSAN_CB_DOMAIN_ERROR, ACLSAN_CBID_ERROR_RECORD},
};

constexpr ToolProfile kProfiles[] = {
    {ToolKind::Memcheck, "memcheck", kMemcheckCallbacks, std::size(kMemcheckCallbacks)},
    {ToolKind::Racecheck, "racecheck", kRacecheckCallbacks, std::size(kRacecheckCallbacks)},
    {ToolKind::Initcheck, "initcheck", kInitcheckCallbacks, std::size(kInitcheckCallbacks)},
    {ToolKind::Synccheck, "synccheck", kSynccheckCallbacks, std::size(kSynccheckCallbacks)},
};

bool EqualToolName(const char* lhs, const char* rhs)
{
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

} // namespace

const ToolProfile* FindToolProfile(const char* toolName)
{
    if (toolName == nullptr || toolName[0] == '\0') {
        return &kProfiles[0];
    }
    for (const auto& profile : kProfiles) {
        if (EqualToolName(profile.name, toolName)) {
            return &profile;
        }
    }
    return nullptr;
}

const char* ToolKindName(ToolKind kind)
{
    for (const auto& profile : kProfiles) {
        if (profile.kind == kind) {
            return profile.name;
        }
    }
    return "unknown";
}

AclsanStatus EnableToolProfile(ToolContext& ctx, const ToolProfile& profile)
{
    for (uint64_t i = 0; i < profile.callbackCount; ++i) {
        const auto& selector = profile.callbacks[i];
        const AclsanStatus status = aclsanEnableCallback(ctx.subscriber, selector.domain, selector.cbid, 1);
        if (status != ACLSAN_STATUS_SUCCESS) {
            return status;
        }
    }
    return ACLSAN_STATUS_SUCCESS;
}

} // namespace aclsan::cann
