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

#include <cstring>

namespace aclsan {
namespace {

void AddRuleAction(AclsanRuntimeHookPlan& plan, uint32_t api, uint32_t actions)
{
    for (uint32_t i = 0; i < plan.ruleCount; ++i) {
        if (plan.rules[i].api == api) {
            plan.rules[i].actions |= actions;
            return;
        }
    }
    if (plan.ruleCount >= ACLSAN_HOOK_RULE_MAX) {
        return;
    }
    plan.rules[plan.ruleCount].api = api;
    plan.rules[plan.ruleCount].actions = actions;
    ++plan.ruleCount;
}

} // namespace

const char* PipelineName(AclsanPatchPipeline pipeline)
{
    switch (pipeline) {
        case ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            return "SET_WAIT_FLAG";
        case ACLSAN_PATCH_PIPELINE_GET_RLS_BUF:
            return "GET_RLS_BUF";
        case ACLSAN_PATCH_PIPELINE_MTE2:
            return "MTE2";
        case ACLSAN_PATCH_PIPELINE_MTE3:
            return "MTE3";
        case ACLSAN_PATCH_PIPELINE_FIXPIPE:
            return "FIXPIPE";
        default:
            return "INVALID";
    }
}

uint32_t PipelineMask(AclsanPatchPipeline pipeline)
{
    return pipeline == ACLSAN_PATCH_PIPELINE_INVALID ? 0u : (1u << static_cast<uint32_t>(pipeline));
}

uint32_t PipelineToCbid(AclsanPatchPipeline pipeline)
{
    switch (pipeline) {
        case ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
        case ACLSAN_PATCH_PIPELINE_GET_RLS_BUF:
            return ACLSAN_CBID_DEVICE_SYNC;
        case ACLSAN_PATCH_PIPELINE_MTE2:
        case ACLSAN_PATCH_PIPELINE_MTE3:
        case ACLSAN_PATCH_PIPELINE_FIXPIPE:
            return ACLSAN_CBID_DEVICE_MEMORY_ACCESS;
        default:
            return 0;
    }
}

AclsanRuntimeHookPlan ApiCore::BuildHookPlanFromSubscriptions()
{
    AclsanRuntimeHookPlan plan{};
    plan.version = ACLSAN_API_VERSION;
    plan.size = sizeof(plan);
    plan.generation = nextHookGeneration_++;

    for (uint32_t domain = ACLSAN_CB_DOMAIN_RESOURCE; domain <= ACLSAN_CB_DOMAIN_ERROR; ++domain) {
        if (HasEnabledDomainLocked(static_cast<AclsanCallbackDomain>(domain))) {
            plan.callbackDomainMask |= (1u << domain);
        }
    }

    if (HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_MALLOC,
            ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_TRACK_RESOURCE | ACLSAN_HOOK_DISPATCH_CALLBACK);
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_MALLOC_HOST,
            ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_TRACK_RESOURCE | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_FREE,
            ACLSAN_HOOK_RECORD_PRE | ACLSAN_HOOK_TRACK_RESOURCE | ACLSAN_HOOK_DISPATCH_CALLBACK);
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_FREE_HOST,
            ACLSAN_HOOK_RECORD_PRE | ACLSAN_HOOK_TRACK_RESOURCE | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ACLSAN_CB_DOMAIN_MEMORY) ||
        HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMCPY_BEGIN) ||
        HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMCPY_END)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_MEMCPY,
            ACLSAN_HOOK_RECORD_PRE | ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMSET_BEGIN) ||
        HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMSET_END)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_MEMSET,
            ACLSAN_HOOK_RECORD_PRE | ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ACLSAN_CB_DOMAIN_BINARY)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE,
            ACLSAN_HOOK_RECORD_PRE | ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ACLSAN_CB_DOMAIN_PATCH)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE, ACLSAN_HOOK_PATCH_BINARY | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ACLSAN_CB_DOMAIN_LAUNCH)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_LAUNCH_KERNEL_WITH_ARGS_ARRAY,
            ACLSAN_HOOK_RECORD_PRE | ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ACLSAN_CB_DOMAIN_SYNCHRONIZE)) {
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM,
            ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_FLUSH_TRACE | ACLSAN_HOOK_DISPATCH_CALLBACK);
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_SYNCHRONIZE_DEVICE,
            ACLSAN_HOOK_RECORD_POST | ACLSAN_HOOK_FLUSH_TRACE | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }

    if (HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS)) {
        plan.patchPipelineMask |= PipelineMask(ACLSAN_PATCH_PIPELINE_MTE2);
        plan.patchPipelineMask |= PipelineMask(ACLSAN_PATCH_PIPELINE_MTE3);
        plan.patchPipelineMask |= PipelineMask(ACLSAN_PATCH_PIPELINE_FIXPIPE);
    }
    if (HasEnabledCallbackLocked(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC)) {
        plan.patchPipelineMask |= PipelineMask(ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG);
        plan.patchPipelineMask |= PipelineMask(ACLSAN_PATCH_PIPELINE_GET_RLS_BUF);
    }
    if (plan.patchPipelineMask != 0) {
        AddRuleAction(plan, ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE, ACLSAN_HOOK_PATCH_BINARY);
        AddRuleAction(
            plan, ACLSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM, ACLSAN_HOOK_FLUSH_TRACE | ACLSAN_HOOK_DISPATCH_CALLBACK);
    }
    return plan;
}

void ApiCore::ReconfigureHookPlan() { activeHookPlan_ = BuildHookPlanFromSubscriptions(); }

uint32_t ApiCore::ActivePatchPipelineMask() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return activeHookPlan_.patchPipelineMask;
}

AclsanStatus ApiCore::ConfigureRuntimeHook(const AclsanRuntimeHookPlan* plan)
{
    if (plan == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (plan->version != ACLSAN_API_VERSION || plan->size != sizeof(AclsanRuntimeHookPlan)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    activeHookPlan_ = *plan;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::GetRuntimeHookState(AclsanRuntimeHookState* state) const
{
    if (state == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::memset(state, 0, sizeof(*state));
    state->version = ACLSAN_API_VERSION;
    state->size = sizeof(*state);
    state->generation = activeHookPlan_.generation;
    state->activePlan = activeHookPlan_;
    return ACLSAN_STATUS_SUCCESS;
}

} // namespace aclsan
