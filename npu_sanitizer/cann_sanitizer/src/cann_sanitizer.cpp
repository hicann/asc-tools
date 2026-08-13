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

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

namespace aclsan::cann {
namespace {

void FillDefaultConfig(AclsanLaunchConfig* config)
{
    std::memset(config, 0, sizeof(*config));
    config->version = ACLSAN_API_VERSION;
    config->size = sizeof(*config);
    std::snprintf(config->toolName, sizeof(config->toolName), "%s", "memcheck");
    std::snprintf(config->workDir, sizeof(config->workDir), "%s", "/tmp/aclsan_cann_sanitizer");
    std::snprintf(config->probeCacheDir, sizeof(config->probeCacheDir), "%s", "/tmp/aclsan_cann_sanitizer/cache");
}

AclsanStatus LoadConfig(const AclsanLaunchConfig* explicitConfig, AclsanLaunchConfig* out)
{
    if (out == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (explicitConfig != nullptr) {
        if (explicitConfig->version != ACLSAN_API_VERSION || explicitConfig->size < sizeof(AclsanLaunchConfig)) {
            return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
        }
        *out = *explicitConfig;
        return ACLSAN_STATUS_SUCCESS;
    }

    AclsanStatus status = aclsanImportLaunchConfigFromFd(ACLSAN_CONFIG_FD, out);
    if (status == ACLSAN_STATUS_SUCCESS) {
        return status;
    }

    FillDefaultConfig(out);
    return ACLSAN_STATUS_SUCCESS;
}

void CopyToolName(AclsanCannSanitizerStats* stats, const char* toolName)
{
    if (stats == nullptr) {
        return;
    }
    std::snprintf(stats->toolName, sizeof(stats->toolName), "%s", toolName != nullptr ? toolName : "");
}

void LogProfileSelection(aclsan::cann::ToolContext& ctx, const aclsan::cann::ToolProfile& profile)
{
    std::ostringstream os;
    os << "[cann-sanitizer] config tool=" << profile.name << " callbacks=" << profile.callbackCount;
    for (uint64_t i = 0; i < profile.callbackCount; ++i) {
        os << " [" << static_cast<uint32_t>(profile.callbacks[i].domain) << ":" << profile.callbacks[i].cbid << "]";
    }
    aclsan::cann::Log(ctx, os.str());
}

} // namespace

ToolContext& Context()
{
    static ToolContext context;
    return context;
}

void Log(ToolContext& ctx, const std::string& message)
{
    if (ctx.log.is_open()) {
        ctx.log << message << "\n";
        ctx.log.flush();
        return;
    }
    std::cerr << message << "\n";
}

} // namespace aclsan::cann

extern "C" AclsanStatus aclsanCannSanitizerInitialize(const AclsanLaunchConfig* config)
{
    auto& ctx = aclsan::cann::Context();
    std::lock_guard<std::mutex> lock(ctx.mutex);
    if (ctx.initialized) {
        return ACLSAN_STATUS_SUCCESS;
    }

    AclsanLaunchConfig loaded{};
    AclsanStatus status = aclsan::cann::LoadConfig(config, &loaded);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }

    const auto* profile = aclsan::cann::FindToolProfile(loaded.toolName);
    if (profile == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }

    AclsanInitParams init{};
    init.version = ACLSAN_API_VERSION;
    init.size = sizeof(init);
    init.launchConfig = &loaded;
    status = aclsanInitialize(&init);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }

    status = aclsanApplyLaunchConfig(&loaded);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }

    status = aclsanRegisterBuiltinPatchPipelines();
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }

    ctx.config = loaded;
    ctx.tool = profile->kind;
    ctx.stats = {};
    ctx.stats.version = ACLSAN_API_VERSION;
    ctx.stats.size = sizeof(ctx.stats);
    aclsan::cann::CopyToolName(&ctx.stats, profile->name);
    ctx.checker.Configure(ctx.tool);
    if (ctx.config.logFile[0] != '\0') {
        ctx.log.open(ctx.config.logFile, std::ios::app);
    }
    aclsan::cann::LogProfileSelection(ctx, *profile);

    AclsanSubscribeDesc desc{};
    desc.version = ACLSAN_API_VERSION;
    desc.size = sizeof(desc);
    desc.name = profile->name;
    desc.callback = aclsan::cann::DispatchToolCallback;
    desc.userdata = &ctx;
    status = aclsanSubscribe(&desc, &ctx.subscriber);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }

    status = aclsan::cann::EnableToolProfile(ctx, *profile);
    if (status != ACLSAN_STATUS_SUCCESS) {
        (void)aclsanUnsubscribe(ctx.subscriber);
        ctx.subscriber = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
        return status;
    }

    ctx.initialized = true;
    aclsan::cann::Log(ctx, std::string("[cann-sanitizer] initialized tool=") + profile->name);
    return ACLSAN_STATUS_SUCCESS;
}

extern "C" AclsanStatus aclsanCannSanitizerFinalize(void)
{
    auto& ctx = aclsan::cann::Context();
    std::lock_guard<std::mutex> lock(ctx.mutex);
    if (!ctx.initialized) {
        return ACLSAN_STATUS_SUCCESS;
    }
    ctx.checker.FlushAll(ctx, "finalize");
    ctx.checker.Reset();
    if (ctx.subscriber != ACLSAN_INVALID_SUBSCRIBER_HANDLE) {
        (void)aclsanUnsubscribe(ctx.subscriber);
        ctx.subscriber = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
    }
    aclsan::cann::Log(ctx, "[cann-sanitizer] finalize");
    if (ctx.log.is_open()) {
        ctx.log.close();
    }
    ctx.initialized = false;
    return ACLSAN_STATUS_SUCCESS;
}

extern "C" AclsanStatus aclsanCannSanitizerGetStats(AclsanCannSanitizerStats* stats)
{
    if (stats == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    auto& ctx = aclsan::cann::Context();
    std::lock_guard<std::mutex> lock(ctx.mutex);
    *stats = ctx.stats;
    return ACLSAN_STATUS_SUCCESS;
}

extern "C" int acltoolInitalize(const void* initInfo)
{
    auto* config = static_cast<const AclsanLaunchConfig*>(initInfo);
    return static_cast<int>(aclsanCannSanitizerInitialize(config));
}

extern "C" void CannComputeInit(void)
{
    const AclsanStatus status = aclsanCannSanitizerInitialize(nullptr);
    if (status != ACLSAN_STATUS_SUCCESS) {
        std::cerr << "[cann-sanitizer] CannComputeInit failed status=" << status << "\n";
    }
}

__attribute__((destructor)) static void AclsanCannSanitizerAutoFinalize() { (void)aclsanCannSanitizerFinalize(); }
