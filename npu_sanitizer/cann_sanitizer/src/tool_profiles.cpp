#include "cann_sanitizer_context.h"

namespace ascsan::cann {
namespace {

constexpr CallbackSelector kMemcheckCallbacks[] = {
    {ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE},
    {ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_BEGIN},
    {ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_END},
    {ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_BEGIN},
    {ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_END},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_BEGIN},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_END},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_BEGIN},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_MEMORY_ACCESS},
    {ASCSAN_CB_DOMAIN_ERROR, ASCSAN_CBID_ERROR_RECORD},
};

constexpr CallbackSelector kInitcheckCallbacks[] = {
    {ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE},
    {ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_END},
    {ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_END},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_BEGIN},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_END},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_BEGIN},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_MEMORY_ACCESS},
};

constexpr CallbackSelector kRacecheckCallbacks[] = {
    {ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_BEGIN},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_END},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_BEGIN},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_SYNC},
    {ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_MEMORY_ACCESS},
};

constexpr CallbackSelector kSynccheckCallbacks[] = {
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_BEGIN},
    {ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_END},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED},
    {ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_BEGIN},
    {ASCSAN_CB_DOMAIN_LAUNCH, ASCSAN_CBID_LAUNCH_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
    {ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END},
    {ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_SYNC},
    {ASCSAN_CB_DOMAIN_ERROR, ASCSAN_CBID_ERROR_RECORD},
};

constexpr ToolProfile kProfiles[] = {
    {ToolKind::Memcheck, "memcheck", kMemcheckCallbacks, std::size(kMemcheckCallbacks)},
    {ToolKind::Racecheck, "racecheck", kRacecheckCallbacks, std::size(kRacecheckCallbacks)},
    {ToolKind::Initcheck, "initcheck", kInitcheckCallbacks, std::size(kInitcheckCallbacks)},
    {ToolKind::Synccheck, "synccheck", kSynccheckCallbacks, std::size(kSynccheckCallbacks)},
};

bool EqualToolName(const char *lhs, const char *rhs)
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

const ToolProfile *FindToolProfile(const char *toolName)
{
    if (toolName == nullptr || toolName[0] == '\0') {
        return &kProfiles[0];
    }
    for (const auto &profile : kProfiles) {
        if (EqualToolName(profile.name, toolName)) {
            return &profile;
        }
    }
    return nullptr;
}

const char *ToolKindName(ToolKind kind)
{
    for (const auto &profile : kProfiles) {
        if (profile.kind == kind) {
            return profile.name;
        }
    }
    return "unknown";
}

AscsanStatus EnableToolProfile(ToolContext &ctx, const ToolProfile &profile)
{
    for (uint64_t i = 0; i < profile.callbackCount; ++i) {
        const auto &selector = profile.callbacks[i];
        const AscsanStatus status = ascsanEnableCallback(
            ctx.subscriber, selector.domain, selector.cbid, 1);
        if (status != ASCSAN_STATUS_SUCCESS) {
            return status;
        }
    }
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace ascsan::cann
