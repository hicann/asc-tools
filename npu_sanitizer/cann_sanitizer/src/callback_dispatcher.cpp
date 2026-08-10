#include "cann_sanitizer_context.h"

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <sstream>

namespace ascsan::cann {
namespace {

void CopyText(char *dst, std::size_t dstSize, const char *src)
{
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    std::snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

uint64_t PointerValue(const void *ptr)
{
    return static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(ptr));
}

void CountDomain(ToolContext &ctx, AscsanCallbackDomain domain)
{
    ++ctx.stats.callbacks;
    switch (domain) {
        case ASCSAN_CB_DOMAIN_RESOURCE:
            ++ctx.stats.resourceCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_MEMORY:
            ++ctx.stats.memoryCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_BINARY:
            ++ctx.stats.binaryCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_PATCH:
            ++ctx.stats.patchCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_LAUNCH:
            ++ctx.stats.launchCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_SYNCHRONIZE:
            ++ctx.stats.syncCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION:
            ++ctx.stats.deviceInstructionCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_REPORT:
            ++ctx.stats.reportCallbacks;
            break;
        case ASCSAN_CB_DOMAIN_ERROR:
            ++ctx.stats.errorCallbacks;
            break;
        default:
            break;
    }
}

template <typename T>
void CaptureCommon(ToolEvent &event, const T *data)
{
    if (data == nullptr) {
        return;
    }
    event.apiName = data->common.apiName != nullptr ? data->common.apiName : "";
    event.result = data->common.result;
    event.correlationId = data->common.correlationId;
}

void FillInstructionSite(ToolEvent &event)
{
    event.parsed = ParseInstruction(event.instruction);

    AscsanPatchSiteInfo site{};
    site.version = ASCSAN_API_VERSION;
    site.size = sizeof(site);
    if (ascsanGetPatchSiteInfo(event.parsed.siteId, &site) == ASCSAN_STATUS_SUCCESS) {
        event.hasSite = true;
        event.site.siteId = site.siteId;
        event.site.pipeline = site.pipeline;
        event.site.binaryId = site.binaryId;
        event.site.functionId = site.functionId;
        event.site.pc = site.pc;
        event.site.functionName = site.functionName != nullptr ? site.functionName : "";
        event.site.opName = site.opName != nullptr ? site.opName : "";
        event.site.sourceFile = site.sourceFile != nullptr ? site.sourceFile : "";
        event.site.sourceLine = site.sourceLine;
    }
}

ToolEvent BuildToolEvent(AscsanCallbackDomain domain, uint32_t cbid, const void *cbdata)
{
    ToolEvent event{};
    event.domain = domain;
    event.cbid = cbid;

    switch (domain) {
        case ASCSAN_CB_DOMAIN_RESOURCE: {
            const auto *data = static_cast<const AscsanResourceData *>(cbdata);
            CaptureCommon(event, data);
            if (data != nullptr) {
                event.hasResource = true;
                event.resource = *data;
            }
            break;
        }
        case ASCSAN_CB_DOMAIN_MEMORY:
            if (cbid == ASCSAN_CBID_MEMORY_MEMCPY_BEGIN ||
                cbid == ASCSAN_CBID_MEMORY_MEMCPY_END) {
                const auto *data = static_cast<const AscsanMemoryMemcpyData *>(cbdata);
                CaptureCommon(event, data);
                if (data != nullptr) {
                    event.hasMemory = true;
                    event.memory = *data;
                }
            } else if (cbid == ASCSAN_CBID_MEMORY_MEMSET_BEGIN ||
                       cbid == ASCSAN_CBID_MEMORY_MEMSET_END) {
                const auto *data = static_cast<const AscsanMemoryMemsetData *>(cbdata);
                CaptureCommon(event, data);
                if (data != nullptr) {
                    event.hasMemory = true;
                    event.memory.common = data->common;
                    event.memory.common.size = sizeof(event.memory);
                    event.memory.dst = data->dst;
                    event.memory.src = nullptr;
                    event.memory.bytes = data->bytes;
                    event.memory.kind = ASCSAN_MEMCPY_DEFAULT;
                    event.memory.stream = data->stream;
                }
            }
            break;
        case ASCSAN_CB_DOMAIN_PATCH: {
            const auto *data = static_cast<const AscsanPatchData *>(cbdata);
            CaptureCommon(event, data);
            if (data != nullptr) {
                event.hasPatch = true;
                event.patch = *data;
                event.patchOriginalPath = data->original.path != nullptr ? data->original.path : "";
                event.patchPatchedPath = data->patched.path != nullptr ? data->patched.path : "";
                event.patch.original.path = nullptr;
                event.patch.original.imageData = nullptr;
                event.patch.patched.path = nullptr;
                event.patch.patched.imageData = nullptr;
            }
            break;
        }
        case ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION: {
            const auto *data = static_cast<const AscsanDeviceInstructionData *>(cbdata);
            CaptureCommon(event, data);
            if (data != nullptr) {
                event.hasInstruction = true;
                event.instruction = *data;
                FillInstructionSite(event);
            }
            break;
        }
        case ASCSAN_CB_DOMAIN_BINARY:
            CaptureCommon(event, static_cast<const AscsanBinaryData *>(cbdata));
            break;
        case ASCSAN_CB_DOMAIN_LAUNCH:
            CaptureCommon(event, static_cast<const AscsanLaunchData *>(cbdata));
            break;
        case ASCSAN_CB_DOMAIN_SYNCHRONIZE:
            CaptureCommon(event, static_cast<const AscsanSynchronizeData *>(cbdata));
            break;
        case ASCSAN_CB_DOMAIN_REPORT:
            CaptureCommon(event, static_cast<const AscsanReportData *>(cbdata));
            break;
        case ASCSAN_CB_DOMAIN_ERROR:
            CaptureCommon(event, static_cast<const AscsanErrorData *>(cbdata));
            break;
        default:
            break;
    }
    return event;
}

void CaptureCallbackData(ToolContext &ctx, const ToolEvent &event)
{
    auto &stats = ctx.stats;
    stats.lastDomain = static_cast<uint32_t>(event.domain);
    stats.lastCbid = event.cbid;
    stats.lastResult = event.result;
    stats.lastCorrelationId = event.correlationId;
    CopyText(stats.lastApiName, sizeof(stats.lastApiName), event.apiName.c_str());

    stats.lastResourceId = 0;
    stats.lastResourceBytes = 0;
    stats.lastResourceMemorySpace = 0;
    stats.lastResourceDeviceId = 0;
    if (event.hasResource) {
        stats.lastResourceId = event.resource.resourceId;
        stats.lastResourceBytes = event.resource.bytes;
        stats.lastResourceMemorySpace = event.resource.memorySpace;
        stats.lastResourceDeviceId = event.resource.deviceId;
    }

    stats.lastMemoryBytes = 0;
    stats.lastMemorySrc = 0;
    stats.lastMemoryDst = 0;
    stats.lastMemoryKind = 0;
    if (event.hasMemory) {
        stats.lastMemoryBytes = event.memory.bytes;
        stats.lastMemorySrc = PointerValue(event.memory.src);
        stats.lastMemoryDst = PointerValue(event.memory.dst);
        stats.lastMemoryKind = event.memory.kind;
    }

    stats.lastPatchBinaryId = 0;
    stats.lastPatchPlanId = 0;
    stats.lastPatchPipelineMask = 0;
    if (event.hasPatch) {
        stats.lastPatchBinaryId = event.patch.binaryId;
        stats.lastPatchPlanId = event.patch.patchPlanId;
        stats.lastPatchPipelineMask = event.patch.pipelineMask;
    }

    stats.lastInstructionSiteId = 0;
    stats.lastInstructionPipeline = 0;
    stats.lastInstructionPc = 0;
    stats.lastInstructionSrc = 0;
    stats.lastInstructionDst = 0;
    stats.lastInstructionBytes = 0;
    CopyText(stats.lastInstructionOpName, sizeof(stats.lastInstructionOpName), "");
    if (event.hasInstruction) {
        stats.lastInstructionSiteId = event.instruction.siteId;
        stats.lastInstructionPipeline = event.instruction.pipeline;
        stats.lastInstructionPc = event.instruction.pc;
        stats.lastInstructionSrc = event.instruction.rawArgs[0];
        stats.lastInstructionDst = event.instruction.rawArgs[1];
        stats.lastInstructionBytes = event.instruction.rawArgs[2];
    }
}

} // namespace

void DispatchToolCallback(void *userdata,
                          AscsanCallbackDomain domain,
                          uint32_t cbid,
                          const void *cbdata)
{
    auto *ctx = static_cast<ToolContext *>(userdata);
    if (ctx == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(ctx->mutex);
    CountDomain(*ctx, domain);
    ToolEvent event = BuildToolEvent(domain, cbid, cbdata);
    CaptureCallbackData(*ctx, event);
    if (event.hasInstruction) {
        CopyText(ctx->stats.lastInstructionOpName,
                 sizeof(ctx->stats.lastInstructionOpName),
                 event.hasSite && !event.site.opName.empty() ? event.site.opName.c_str()
                                                              : event.parsed.op.c_str());
        ctx->stats.lastInstructionSrc = event.parsed.src;
        ctx->stats.lastInstructionDst = event.parsed.dst;
        ctx->stats.lastInstructionBytes = event.parsed.bytes;
    }
    ctx->checker.OnCallback(*ctx, event);
}

} // namespace ascsan::cann
