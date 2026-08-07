#include "api_core.h"

#include <cstring>

namespace ascsan {
namespace {

void AddRuleAction(AscsanRuntimeHookPlan &plan, uint32_t api, uint32_t actions)
{
    for (uint32_t i = 0; i < plan.ruleCount; ++i) {
        if (plan.rules[i].api == api) {
            plan.rules[i].actions |= actions;
            return;
        }
    }
    if (plan.ruleCount >= ASCSAN_HOOK_RULE_MAX) {
        return;
    }
    plan.rules[plan.ruleCount].api = api;
    plan.rules[plan.ruleCount].actions = actions;
    ++plan.ruleCount;
}

} // namespace

const char *PipelineName(AscsanPatchPipeline pipeline)
{
    switch (pipeline) {
        case ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            return "SET_WAIT_FLAG";
        case ASCSAN_PATCH_PIPELINE_GET_RLS_BUF:
            return "GET_RLS_BUF";
        case ASCSAN_PATCH_PIPELINE_MTE2:
            return "MTE2";
        case ASCSAN_PATCH_PIPELINE_MTE3:
            return "MTE3";
        case ASCSAN_PATCH_PIPELINE_FIXPIPE:
            return "FIXPIPE";
        default:
            return "INVALID";
    }
}

uint32_t PipelineMask(AscsanPatchPipeline pipeline)
{
    return pipeline == ASCSAN_PATCH_PIPELINE_INVALID ? 0u : (1u << static_cast<uint32_t>(pipeline));
}

uint32_t PipelineToCbid(AscsanPatchPipeline pipeline)
{
    switch (pipeline) {
        case ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            return ASCSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG;
        case ASCSAN_PATCH_PIPELINE_GET_RLS_BUF:
            return ASCSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF;
        case ASCSAN_PATCH_PIPELINE_MTE2:
            return ASCSAN_CBID_DEVICE_INSTRUCTION_MTE2;
        case ASCSAN_PATCH_PIPELINE_MTE3:
            return ASCSAN_CBID_DEVICE_INSTRUCTION_MTE3;
        case ASCSAN_PATCH_PIPELINE_FIXPIPE:
            return ASCSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE;
        default:
            return 0;
    }
}

AscsanRuntimeHookPlan ApiCore::BuildHookPlanFromSubscriptions()
{
    AscsanRuntimeHookPlan plan{};
    plan.version = ASCSAN_API_VERSION;
    plan.size = sizeof(plan);
    plan.generation = nextHookGeneration_++;

    for (uint32_t domain = ASCSAN_CB_DOMAIN_RESOURCE; domain <= ASCSAN_CB_DOMAIN_ERROR; ++domain) {
        if (HasEnabledDomainLocked(static_cast<AscsanCallbackDomain>(domain))) {
            plan.callbackDomainMask |= (1u << domain);
        }
    }

    if (HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_MALLOC,
                      ASCSAN_HOOK_RECORD_POST | ASCSAN_HOOK_TRACK_RESOURCE |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_MALLOC_HOST,
                      ASCSAN_HOOK_RECORD_POST | ASCSAN_HOOK_TRACK_RESOURCE |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_FREE,
                      ASCSAN_HOOK_RECORD_PRE | ASCSAN_HOOK_TRACK_RESOURCE |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_FREE_HOST,
                      ASCSAN_HOOK_RECORD_PRE | ASCSAN_HOOK_TRACK_RESOURCE |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ASCSAN_CB_DOMAIN_MEMORY) ||
        HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_BEGIN) ||
        HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_END)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_MEMCPY,
                      ASCSAN_HOOK_RECORD_PRE | ASCSAN_HOOK_RECORD_POST |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_BEGIN) ||
        HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_END)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_MEMSET,
                      ASCSAN_HOOK_RECORD_PRE | ASCSAN_HOOK_RECORD_POST |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ASCSAN_CB_DOMAIN_BINARY)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE,
                      ASCSAN_HOOK_RECORD_PRE | ASCSAN_HOOK_RECORD_POST |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ASCSAN_CB_DOMAIN_PATCH)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE,
                      ASCSAN_HOOK_PATCH_BINARY | ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ASCSAN_CB_DOMAIN_LAUNCH)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_LAUNCH_KERNEL_WITH_ARGS_ARRAY,
                      ASCSAN_HOOK_RECORD_PRE | ASCSAN_HOOK_RECORD_POST |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }
    if (HasEnabledDomainLocked(ASCSAN_CB_DOMAIN_SYNCHRONIZE)) {
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM,
                      ASCSAN_HOOK_RECORD_POST | ASCSAN_HOOK_FLUSH_TRACE |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
        AddRuleAction(plan,
                      ASCSAN_RT_API_ACLRT_SYNCHRONIZE_DEVICE,
                      ASCSAN_HOOK_RECORD_POST | ASCSAN_HOOK_FLUSH_TRACE |
                          ASCSAN_HOOK_DISPATCH_CALLBACK);
    }

    const struct {
        uint32_t cbid;
        AscsanPatchPipeline pipeline;
    } deviceSubscriptions[] = {
        {ASCSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG, ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG},
        {ASCSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF, ASCSAN_PATCH_PIPELINE_GET_RLS_BUF},
        {ASCSAN_CBID_DEVICE_INSTRUCTION_MTE2, ASCSAN_PATCH_PIPELINE_MTE2},
        {ASCSAN_CBID_DEVICE_INSTRUCTION_MTE3, ASCSAN_PATCH_PIPELINE_MTE3},
        {ASCSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE, ASCSAN_PATCH_PIPELINE_FIXPIPE},
    };
    for (const auto &entry : deviceSubscriptions) {
        if (HasEnabledCallbackLocked(ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, entry.cbid)) {
            plan.patchPipelineMask |= PipelineMask(entry.pipeline);
            AddRuleAction(plan, ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE, ASCSAN_HOOK_PATCH_BINARY);
            AddRuleAction(plan,
                          ASCSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM,
                          ASCSAN_HOOK_FLUSH_TRACE | ASCSAN_HOOK_DISPATCH_CALLBACK);
        }
    }
    return plan;
}

void ApiCore::ReconfigureHookPlan()
{
    activeHookPlan_ = BuildHookPlanFromSubscriptions();
}

uint32_t ApiCore::ActivePatchPipelineMask() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return activeHookPlan_.patchPipelineMask;
}

AscsanStatus ApiCore::ConfigureRuntimeHook(const AscsanRuntimeHookPlan *plan)
{
    if (plan == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (plan->version != ASCSAN_API_VERSION || plan->size != sizeof(AscsanRuntimeHookPlan)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    activeHookPlan_ = *plan;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::GetRuntimeHookState(AscsanRuntimeHookState *state) const
{
    if (state == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::memset(state, 0, sizeof(*state));
    state->version = ASCSAN_API_VERSION;
    state->size = sizeof(*state);
    state->generation = activeHookPlan_.generation;
    state->activePlan = activeHookPlan_;
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace ascsan
