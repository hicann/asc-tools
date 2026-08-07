#include "cann_sanitizer_context.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

namespace ascsan::cann {
namespace {

void FillDefaultConfig(AscsanLaunchConfig *config)
{
    std::memset(config, 0, sizeof(*config));
    config->version = ASCSAN_API_VERSION;
    config->size = sizeof(*config);
    std::snprintf(config->toolName, sizeof(config->toolName), "%s", "memcheck");
    std::snprintf(config->workDir, sizeof(config->workDir), "%s", "/tmp/ascsan_cann_sanitizer");
    std::snprintf(config->probeCacheDir, sizeof(config->probeCacheDir), "%s", "/tmp/ascsan_cann_sanitizer/cache");
}

AscsanStatus LoadConfig(const AscsanLaunchConfig *explicitConfig, AscsanLaunchConfig *out)
{
    if (out == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (explicitConfig != nullptr) {
        if (explicitConfig->version != ASCSAN_API_VERSION || explicitConfig->size < sizeof(AscsanLaunchConfig)) {
            return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
        }
        *out = *explicitConfig;
        return ASCSAN_STATUS_SUCCESS;
    }

    AscsanStatus status = ascsanImportLaunchConfigFromFd(ASCSAN_CONFIG_FD, out);
    if (status == ASCSAN_STATUS_SUCCESS) {
        return status;
    }

    FillDefaultConfig(out);
    return ASCSAN_STATUS_SUCCESS;
}

void CopyToolName(AscsanCannSanitizerStats *stats, const char *toolName)
{
    if (stats == nullptr) {
        return;
    }
    std::snprintf(stats->toolName, sizeof(stats->toolName), "%s", toolName != nullptr ? toolName : "");
}

void LogProfileSelection(ascsan::cann::ToolContext &ctx, const ascsan::cann::ToolProfile &profile)
{
    std::ostringstream os;
    os << "[cann-sanitizer] config tool=" << profile.name
       << " callbacks=" << profile.callbackCount;
    for (uint64_t i = 0; i < profile.callbackCount; ++i) {
        os << " [" << static_cast<uint32_t>(profile.callbacks[i].domain)
           << ":" << profile.callbacks[i].cbid << "]";
    }
    ascsan::cann::Log(ctx, os.str());
}

} // namespace

ToolContext &Context()
{
    static ToolContext context;
    return context;
}

void Log(ToolContext &ctx, const std::string &message)
{
    if (ctx.log.is_open()) {
        ctx.log << message << "\n";
        ctx.log.flush();
        return;
    }
    std::cerr << message << "\n";
}

} // namespace ascsan::cann

extern "C" AscsanStatus ascsanCannSanitizerInitialize(const AscsanLaunchConfig *config)
{
    auto &ctx = ascsan::cann::Context();
    std::lock_guard<std::mutex> lock(ctx.mutex);
    if (ctx.initialized) {
        return ASCSAN_STATUS_SUCCESS;
    }

    AscsanLaunchConfig loaded{};
    AscsanStatus status = ascsan::cann::LoadConfig(config, &loaded);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }

    const auto *profile = ascsan::cann::FindToolProfile(loaded.toolName);
    if (profile == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }

    AscsanInitParams init{};
    init.version = ASCSAN_API_VERSION;
    init.size = sizeof(init);
    init.launchConfig = &loaded;
    init.workDir = loaded.workDir;
    status = ascsanInitialize(&init);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }

    status = ascsanApplyLaunchConfig(&loaded);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }

    status = ascsanRegisterBuiltinPatchPipelines();
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }

    ctx.config = loaded;
    ctx.tool = profile->kind;
    ctx.stats = {};
    ctx.stats.version = ASCSAN_API_VERSION;
    ctx.stats.size = sizeof(ctx.stats);
    ascsan::cann::CopyToolName(&ctx.stats, profile->name);
    ctx.checker.Configure(ctx.tool);
    if (ctx.config.logFile[0] != '\0') {
        ctx.log.open(ctx.config.logFile, std::ios::app);
    }
    ascsan::cann::LogProfileSelection(ctx, *profile);

    AscsanSubscribeDesc desc{};
    desc.version = ASCSAN_API_VERSION;
    desc.size = sizeof(desc);
    desc.name = profile->name;
    desc.callback = ascsan::cann::DispatchToolCallback;
    desc.userdata = &ctx;
    status = ascsanSubscribe(&desc, &ctx.subscriber);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }

    status = ascsan::cann::EnableToolProfile(ctx, *profile);
    if (status != ASCSAN_STATUS_SUCCESS) {
        (void)ascsanUnsubscribe(ctx.subscriber);
        ctx.subscriber = 0;
        return status;
    }

    ctx.initialized = true;
    ascsan::cann::Log(ctx, std::string("[cann-sanitizer] initialized tool=") + profile->name);
    return ASCSAN_STATUS_SUCCESS;
}

extern "C" AscsanStatus ascsanCannSanitizerFinalize(void)
{
    auto &ctx = ascsan::cann::Context();
    std::lock_guard<std::mutex> lock(ctx.mutex);
    if (!ctx.initialized) {
        return ASCSAN_STATUS_SUCCESS;
    }
    ctx.checker.FlushAll(ctx, "finalize");
    ctx.checker.Reset();
    if (ctx.subscriber != 0) {
        (void)ascsanUnsubscribe(ctx.subscriber);
        ctx.subscriber = 0;
    }
    ascsan::cann::Log(ctx, "[cann-sanitizer] finalize");
    if (ctx.log.is_open()) {
        ctx.log.close();
    }
    ctx.initialized = false;
    return ASCSAN_STATUS_SUCCESS;
}

extern "C" AscsanStatus ascsanCannSanitizerGetStats(AscsanCannSanitizerStats *stats)
{
    if (stats == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    auto &ctx = ascsan::cann::Context();
    std::lock_guard<std::mutex> lock(ctx.mutex);
    *stats = ctx.stats;
    return ASCSAN_STATUS_SUCCESS;
}

extern "C" int acltoolInitalize(const void *initInfo)
{
    auto *config = static_cast<const AscsanLaunchConfig *>(initInfo);
    return static_cast<int>(ascsanCannSanitizerInitialize(config));
}

extern "C" void CannComputeInit(void)
{
    const AscsanStatus status = ascsanCannSanitizerInitialize(nullptr);
    if (status != ASCSAN_STATUS_SUCCESS) {
        std::cerr << "[cann-sanitizer] CannComputeInit failed status=" << status << "\n";
    }
}

__attribute__((destructor)) static void AscsanCannSanitizerAutoFinalize()
{
    (void)ascsanCannSanitizerFinalize();
}
