#include "cann_sanitizer_context.h"

#include <cstddef>
#include <cstdint>
#include <sstream>

namespace ascsan::cann {
namespace {

uint64_t PointerValue(const void *ptr)
{
    return static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(ptr));
}

uint64_t EventPtr(const AscsanResourceData &resource)
{
    return PointerValue(resource.ptr);
}

std::size_t WindowInstructionCount(const CheckWindow &window)
{
    return window.mte2.size() + window.mte3.size() + window.fixpipe.size() +
           window.setWaitFlag.size() + window.getRlsBuf.size();
}

void AppendWindowCounts(std::ostringstream &os, const CheckWindow &window)
{
    os << " mte2=" << window.mte2.size()
       << " mte3=" << window.mte3.size()
       << " fixpipe=" << window.fixpipe.size()
       << " setWaitFlag=" << window.setWaitFlag.size()
       << " getRlsBuf=" << window.getRlsBuf.size();
}

} // namespace

bool CheckWindowKey::operator<(const CheckWindowKey &other) const
{
    if (launchId != other.launchId) {
        return launchId < other.launchId;
    }
    if (binaryId != other.binaryId) {
        return binaryId < other.binaryId;
    }
    if (functionId != other.functionId) {
        return functionId < other.functionId;
    }
    return blockId < other.blockId;
}

bool DummyChecker::HandlerKey::operator<(const HandlerKey &other) const
{
    if (domain != other.domain) {
        return domain < other.domain;
    }
    return cbid < other.cbid;
}

void DummyChecker::Configure(ToolKind tool)
{
    Reset();
    tool_ = tool;
    switch (tool_) {
        case ToolKind::Memcheck:
            RegisterMemcheck();
            break;
        case ToolKind::Racecheck:
            RegisterRacecheck();
            break;
        case ToolKind::Initcheck:
            RegisterInitcheck();
            break;
        case ToolKind::Synccheck:
            RegisterSynccheck();
            break;
    }
}

void DummyChecker::Reset()
{
    handlers_.clear();
    allocations_.clear();
    windows_.clear();
    memoryOps_.clear();
    patchEvents_.clear();
    reports_.clear();
}

void DummyChecker::Register(AscsanCallbackDomain domain, uint32_t cbid, Handler handler)
{
    handlers_[HandlerKey{domain, cbid}] = handler;
}

void DummyChecker::RegisterMemcheck()
{
    Register(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC, &DummyChecker::OnResourceAlloc);
    Register(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE, &DummyChecker::OnResourceFree);
    Register(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_BEGIN, &DummyChecker::OnMemoryOp);
    Register(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMCPY_END, &DummyChecker::OnMemoryOp);
    Register(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_BEGIN, &DummyChecker::OnMemoryOp);
    Register(ASCSAN_CB_DOMAIN_MEMORY, ASCSAN_CBID_MEMORY_MEMSET_END, &DummyChecker::OnMemoryOp);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &DummyChecker::OnSynchronize);
    Register(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END, &DummyChecker::OnSynchronize);
    Register(ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_MEMORY_ACCESS, &DummyChecker::OnInstruction);
}

void DummyChecker::RegisterRacecheck()
{
    Register(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC, &DummyChecker::OnResourceAlloc);
    Register(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE, &DummyChecker::OnResourceFree);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &DummyChecker::OnSynchronize);
    Register(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END, &DummyChecker::OnSynchronize);
    Register(ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_SYNC, &DummyChecker::OnInstruction);
    Register(ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_MEMORY_ACCESS, &DummyChecker::OnInstruction);
}

void DummyChecker::RegisterInitcheck()
{
    RegisterMemcheck();
}

void DummyChecker::RegisterSynccheck()
{
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END, &DummyChecker::OnPatch);
    Register(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &DummyChecker::OnSynchronize);
    Register(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END, &DummyChecker::OnSynchronize);
    Register(ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ASCSAN_CBID_DEVICE_SYNC, &DummyChecker::OnInstruction);
}

void DummyChecker::OnCallback(ToolContext &ctx, const ToolEvent &event)
{
    ++ctx.stats.checkerEvents;
    const auto it = handlers_.find(HandlerKey{event.domain, event.cbid});
    if (it == handlers_.end()) {
        OnGeneric(ctx, event);
        return;
    }
    (this->*(it->second))(ctx, event);
    ctx.stats.checkerReports = reports_.size();
}

void DummyChecker::OnResourceAlloc(ToolContext &ctx, const ToolEvent &event)
{
    if (!event.hasResource) {
        return;
    }
    allocations_[EventPtr(event.resource)] = AllocationRecord{
        event.resource.resourceId,
        EventPtr(event.resource),
        event.resource.bytes,
        event.resource.memorySpace,
        event.resource.deviceId,
    };

    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " resource-alloc id=" << event.resource.resourceId
       << " ptr=0x" << std::hex << EventPtr(event.resource) << std::dec
       << " bytes=" << event.resource.bytes;
    Log(ctx, os.str());
}

void DummyChecker::OnResourceFree(ToolContext &ctx, const ToolEvent &event)
{
    if (!event.hasResource) {
        return;
    }
    allocations_.erase(EventPtr(event.resource));

    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " resource-free id=" << event.resource.resourceId
       << " ptr=0x" << std::hex << EventPtr(event.resource) << std::dec;
    Log(ctx, os.str());
}

void DummyChecker::OnMemoryOp(ToolContext &ctx, const ToolEvent &event)
{
    if (!event.hasMemory) {
        return;
    }
    memoryOps_.push_back(event);

    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " memory-op cbid=" << event.cbid
       << " src=0x" << std::hex << PointerValue(event.memory.src)
       << " dst=0x" << PointerValue(event.memory.dst) << std::dec
       << " bytes=" << event.memory.bytes;
    Log(ctx, os.str());
}

void DummyChecker::OnPatch(ToolContext &ctx, const ToolEvent &event)
{
    if (!event.hasPatch) {
        return;
    }
    patchEvents_.push_back(event);

    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " patch cbid=" << event.cbid
       << " plan=" << event.patch.patchPlanId
       << " mask=0x" << std::hex << event.patch.pipelineMask << std::dec;
    if (!event.patchPatchedPath.empty()) {
        os << " patched=" << event.patchPatchedPath;
    }
    Log(ctx, os.str());
}

CheckWindowKey DummyChecker::MakeWindowKey(const ToolEvent &event) const
{
    CheckWindowKey key{};
    if (!event.hasInstruction) {
        return key;
    }
    key.launchId = event.instruction.launchId;
    key.binaryId = event.instruction.binaryId;
    key.functionId = event.instruction.functionId;
    key.blockId = event.instruction.blockId;
    return key;
}

CheckWindow &DummyChecker::GetWindow(ToolContext &ctx, const CheckWindowKey &key)
{
    auto result = windows_.try_emplace(key);
    if (result.second) {
        ++ctx.stats.checkerWindows;
    }
    return result.first->second;
}

void DummyChecker::OnInstruction(ToolContext &ctx, const ToolEvent &event)
{
    if (!event.hasInstruction) {
        return;
    }

    ++ctx.stats.checkerInstructions;
    auto &window = GetWindow(ctx, MakeWindowKey(event));
    switch (event.parsed.pipeline) {
        case ASCSAN_PATCH_PIPELINE_MTE2:
            window.mte2.push_back(event);
            ++ctx.stats.memoryTransferEvents;
            break;
        case ASCSAN_PATCH_PIPELINE_MTE3:
            window.mte3.push_back(event);
            ++ctx.stats.memoryTransferEvents;
            break;
        case ASCSAN_PATCH_PIPELINE_FIXPIPE:
            window.fixpipe.push_back(event);
            ++ctx.stats.fixpipeEvents;
            break;
        case ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            window.setWaitFlag.push_back(event);
            ++ctx.stats.syncEvents;
            break;
        case ASCSAN_PATCH_PIPELINE_GET_RLS_BUF:
            window.getRlsBuf.push_back(event);
            ++ctx.stats.syncEvents;
            break;
        case ASCSAN_PATCH_PIPELINE_INVALID:
            break;
    }

    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " instruction=" << event.parsed.op
       << " site=" << event.parsed.siteId
       << " pc=0x" << std::hex << event.parsed.pc << std::dec;
    if (event.hasSite) {
        os << " siteOp=" << event.site.opName
           << " sitePc=0x" << std::hex << event.site.pc << std::dec
           << " function=" << event.site.functionName;
    }
    Log(ctx, os.str());
}

void DummyChecker::OnSynchronize(ToolContext &ctx, const ToolEvent &event)
{
    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " synchronize cbid=" << event.cbid
       << " api=" << event.apiName;
    Log(ctx, os.str());
    FlushAll(ctx, "sync");
}

void DummyChecker::OnGeneric(ToolContext &ctx, const ToolEvent &event)
{
    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " generic domain=" << static_cast<uint32_t>(event.domain)
       << " cbid=" << event.cbid;
    if (!event.apiName.empty()) {
        os << " api=" << event.apiName;
    }
    Log(ctx, os.str());
}

void DummyChecker::FlushAll(ToolContext &ctx, const char *reason)
{
    while (!windows_.empty()) {
        auto it = windows_.begin();
        const CheckWindowKey key = it->first;
        const CheckWindow window = it->second;
        windows_.erase(it);
        CompleteWindow(ctx, key, window, reason != nullptr ? reason : "unknown");
    }
}

void DummyChecker::CompleteWindow(ToolContext &ctx,
                                  const CheckWindowKey &key,
                                  const CheckWindow &window,
                                  const char *reason)
{
    if (WindowInstructionCount(window) == 0) {
        return;
    }
    ++ctx.stats.checkerCompletedWindows;

    if ((tool_ == ToolKind::Memcheck || tool_ == ToolKind::Initcheck || tool_ == ToolKind::Racecheck) &&
        !window.fixpipe.empty() && window.mte2.empty() && window.mte3.empty()) {
        AddReport(ctx, "FIXPIPE window has no MTE2/MTE3 producer records");
    }
    if ((tool_ == ToolKind::Synccheck || tool_ == ToolKind::Racecheck) &&
        !window.getRlsBuf.empty() && window.setWaitFlag.empty()) {
        AddReport(ctx, "GET_RLS_BUF window has no SET_WAIT_FLAG record");
    }

    std::ostringstream os;
    os << "[cann-sanitizer] checker=" << ToolKindName(tool_)
       << " window-complete reason=" << reason
       << " launch=" << key.launchId
       << " binary=" << key.binaryId
       << " function=" << key.functionId
       << " block=" << key.blockId;
    AppendWindowCounts(os, window);
    os << " reports=" << reports_.size();
    Log(ctx, os.str());
    ctx.stats.checkerReports = reports_.size();
}

void DummyChecker::AddReport(ToolContext &ctx, const std::string &message)
{
    reports_.push_back(message);
    ctx.stats.checkerReports = reports_.size();
    Log(ctx, std::string("[cann-sanitizer] checker-report ") + message);
}

} // namespace ascsan::cann
