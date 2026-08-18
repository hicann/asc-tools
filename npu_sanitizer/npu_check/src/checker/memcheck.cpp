#include "checker/memcheck.h"

#include <limits>

namespace npu::sanitizer {
namespace {

AllocationContext ToContext(const std::optional<Allocation>& allocation)
{
    AllocationContext context{};
    if (!allocation.has_value()) {
        return context;
    }
    context.present = true;
    context.resourceId = allocation->resourceId;
    context.generation = allocation->generation;
    context.base = allocation->base;
    context.bytes = allocation->bytes;
    context.deviceId = allocation->deviceId;
    return context;
}

} // namespace

Memcheck::Memcheck(bool strictUnknown) : strictUnknown_(strictUnknown) {}

void Memcheck::OnAllocation(const AclsanResourceData& data)
{
    if (data.memorySpace != ACLSAN_MEMORY_SPACE_DEVICE || data.common.result != 0) {
        return;
    }
    const auto result =
        allocations_.Register(data.resourceId, reinterpret_cast<uint64_t>(data.ptr), data.bytes, data.deviceId);
    if (result.status == AllocationUpdateStatus::OK) {
        ++stats_.allocations;
    }
}

void Memcheck::OnFree(const AclsanResourceData& data)
{
    if (data.memorySpace != ACLSAN_MEMORY_SPACE_DEVICE || data.common.result != 0) {
        return;
    }
    const auto result = allocations_.Release(data.resourceId, reinterpret_cast<uint64_t>(data.ptr), data.deviceId);
    if (result.status == AllocationUpdateStatus::OK) {
        ++stats_.frees;
    }
}

std::vector<Diagnostic> Memcheck::CheckAccess(
    const std::string& operation, AccessKind kind, uint64_t address, uint64_t bytes,
    const InstructionContext& instruction, uint32_t deviceId) const
{
    const RangeResult range = allocations_.Classify(deviceId, address, bytes);
    if (range.status == RangeStatus::VALID || (range.status == RangeStatus::UNKNOWN && !strictUnknown_)) {
        return {};
    }

    Diagnostic diagnostic{};
    diagnostic.kind = DiagnosticKind::OUT_OF_BOUNDS;
    diagnostic.severity = Severity::ERROR;
    diagnostic.operation = operation;
    diagnostic.access = kind;
    diagnostic.address = address;
    diagnostic.bytes = bytes;
    diagnostic.allocation = ToContext(range.allocation);
    diagnostic.instruction = instruction;
    diagnostic.suggestion = "check the GM allocation extent, byte count, stride, and tail handling at this access";

    switch (range.status) {
        case RangeStatus::OUT_OF_BOUNDS:
            diagnostic.detail = "GM access crosses an allocation boundary";
            break;
        case RangeStatus::USE_AFTER_FREE:
            diagnostic.detail = "GM access does not target a live allocation";
            break;
        case RangeStatus::AMBIGUOUS:
            diagnostic.detail = "GM access does not resolve uniquely to one live allocation";
            break;
        case RangeStatus::OVERFLOW:
            diagnostic.detail = "GM address plus access size overflows the device address range";
            break;
        case RangeStatus::UNKNOWN:
            diagnostic.detail = "GM access is outside every tracked allocation";
            break;
        case RangeStatus::VALID:
            return {};
    }
    return {diagnostic};
}

void Memcheck::QueueDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& data)
{
    if (data.memorySpace != ACLSAN_DEVICE_MEMORY_SPACE_GM) {
        return;
    }
    ++stats_.deviceOperations;
    if (pendingDeviceAccesses_.size() >= kMaxPendingDeviceOperations) {
        ++stats_.droppedDeviceOperations;
        return;
    }
    pendingDeviceAccesses_.push_back(data);
    stats_.pendingDeviceOperations = pendingDeviceAccesses_.size();
}

std::vector<Diagnostic> Memcheck::CheckDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& data) const
{
    InstructionContext instruction{};
    instruction.present = true;
    instruction.pipeline = data.header.pipeline;
    instruction.sourceKind = data.header.sourceKind;
    instruction.memorySpace = data.memorySpace;
    instruction.accessIndex = data.accessIndex;
    instruction.accessCount = data.accessCount;
    instruction.layoutKind = data.layoutKind;
    instruction.siteId = data.header.siteId;
    instruction.blockId = data.header.blockId;
    instruction.blockType = data.header.blockType;
    instruction.deviceId = data.header.deviceId;
    instruction.coreId = data.header.coreId;
    instruction.launchId = data.header.launchId;
    instruction.instrExecId = data.header.instrExecId;
    instruction.serialNo = data.header.serialNo;
    instruction.pc = data.header.pc;
    instruction.flags = data.header.flags;

    const std::string operation = [&data] {
        switch (data.header.sourceKind) {
            case ACLSAN_DEVICE_SOURCE_MTE2:
                return std::string("MTE2");
            case ACLSAN_DEVICE_SOURCE_MTE3:
                return std::string("MTE3");
            case ACLSAN_DEVICE_SOURCE_FIXPIPE:
                return std::string("FIXPIPE");
            case ACLSAN_DEVICE_SOURCE_LD:
                return std::string("LD");
            case ACLSAN_DEVICE_SOURCE_ST:
                return std::string("ST");
            case ACLSAN_DEVICE_SOURCE_VECTOR:
                return std::string("VECTOR");
            case ACLSAN_DEVICE_SOURCE_CUBE:
                return std::string("CUBE");
            case ACLSAN_DEVICE_SOURCE_SCALAR:
                return std::string("SCALAR");
            default:
                return std::string("DEVICE");
        }
    }();

    std::vector<Diagnostic> diagnostics;
    if (data.header.version != ACLSAN_API_VERSION || data.header.size < sizeof(AclsanDeviceMemoryAccessData)) {
        return {};
    }
    if (data.accessCount == 0 || data.accessIndex >= data.accessCount) {
        return {};
    }

    std::vector<AccessKind> accessKinds;
    switch (data.accessMode) {
        case ACLSAN_DEVICE_MEMORY_ACCESS_READ:
            accessKinds.push_back(AccessKind::READ);
            break;
        case ACLSAN_DEVICE_MEMORY_ACCESS_WRITE:
            accessKinds.push_back(AccessKind::WRITE);
            break;
        case ACLSAN_DEVICE_MEMORY_ACCESS_READ_WRITE:
            accessKinds.push_back(AccessKind::READ);
            accessKinds.push_back(AccessKind::WRITE);
            break;
        default:
            return {};
    }

    // The allocation registry tracks GM allocations.  UB/L1/L0/BT/private
    // accesses are still valid typed events, but require a separate shadow
    // space and therefore do not participate in this GM memcheck yet.
    if (data.memorySpace != ACLSAN_DEVICE_MEMORY_SPACE_GM) {
        return {};
    }
    if ((data.header.flags & ACLSAN_DEVICE_EVENT_FLAG_PREDICATED) != 0 && data.predicateMask0 == 0 &&
        data.predicateMask1 == 0) {
        return {};
    }

    auto appendAccess = [&](uint64_t address, uint64_t bytes) {
        for (const AccessKind kind : accessKinds) {
            auto found = CheckAccess(operation, kind, address, bytes, instruction, data.header.deviceId);
            diagnostics.insert(diagnostics.end(), found.begin(), found.end());
        }
    };

    constexpr uint64_t kMaxLayoutSegments = 1u << 20u;
    const auto addressWithOffset = [](uint64_t base, __int128 offset) -> std::optional<uint64_t> {
        if (offset < -static_cast<__int128>(base) ||
            offset > static_cast<__int128>(std::numeric_limits<uint64_t>::max() - base)) {
            return std::nullopt;
        }
        return static_cast<uint64_t>(static_cast<__int128>(base) + offset);
    };

    switch (data.layoutKind) {
        case ACLSAN_MEM_LAYOUT_SCALAR:
            if (data.layout.scalar.bytes == 0) {
                return {};
            }
            appendAccess(data.address, data.layout.scalar.bytes);
            break;
        case ACLSAN_MEM_LAYOUT_RANGE:
            if (data.layout.range.bytes == 0) {
                return {};
            }
            appendAccess(data.address, data.layout.range.bytes);
            break;
        case ACLSAN_MEM_LAYOUT_BLOCK_REPEAT: {
            const auto& layout = data.layout.blockRepeat;
            if (layout.blockNum == 0 || layout.blockSize == 0 || layout.repeatTimes == 0) {
                return {};
            }
            const uint64_t segmentCount = static_cast<uint64_t>(layout.blockNum) * layout.repeatTimes;
            if (segmentCount > kMaxLayoutSegments) {
                return {};
            }
            for (uint32_t repeat = 0; repeat < layout.repeatTimes; ++repeat) {
                for (uint32_t block = 0; block < layout.blockNum; ++block) {
                    const __int128 offset = static_cast<__int128>(repeat) * layout.repeatStride +
                                            static_cast<__int128>(block) * layout.blockStride;
                    const auto address = addressWithOffset(data.address, offset);
                    if (!address) {
                        return {};
                    }
                    appendAccess(*address, layout.blockSize);
                }
            }
            break;
        }
        case ACLSAN_MEM_LAYOUT_ND_AFFINE: {
            const auto& layout = data.layout.ndAffine;
            if (layout.rank == 0 || layout.rank > 5 || layout.elementBytes == 0) {
                return {};
            }
            uint64_t elementCount = 1;
            bool countOverflow = false;
            for (uint32_t dimension = 0; dimension < layout.rank; ++dimension) {
                if (layout.dims[dimension] == 0 ||
                    elementCount > std::numeric_limits<uint64_t>::max() / layout.dims[dimension]) {
                    countOverflow = true;
                    break;
                }
                elementCount *= layout.dims[dimension];
            }
            if (countOverflow) {
                return {};
            }

            const __int128 kAddressOffsetMin = -static_cast<__int128>(std::numeric_limits<uint64_t>::max());
            const __int128 kAddressOffsetMax = static_cast<__int128>(std::numeric_limits<uint64_t>::max());
            __int128 minOffset = 0;
            __int128 maxOffset = 0;
            bool offsetOverflow = false;
            for (uint32_t dimension = 0; dimension < layout.rank; ++dimension) {
                const __int128 extent = static_cast<__int128>(layout.dims[dimension] - 1) *
                                        static_cast<__int128>(layout.strides[dimension]);
                if (extent < 0) {
                    if (extent < kAddressOffsetMin - minOffset) {
                        offsetOverflow = true;
                        break;
                    }
                    minOffset += extent;
                } else {
                    if (extent > kAddressOffsetMax - maxOffset) {
                        offsetOverflow = true;
                        break;
                    }
                    maxOffset += extent;
                }
            }
            if (offsetOverflow) {
                return {};
            }

            if (elementCount <= kMaxLayoutSegments) {
                for (uint64_t linear = 0; linear < elementCount; ++linear) {
                    uint64_t index = linear;
                    __int128 offset = 0;
                    for (uint32_t dimension = layout.rank; dimension > 0; --dimension) {
                        const uint32_t current = dimension - 1;
                        const uint64_t coordinate = index % layout.dims[current];
                        index /= layout.dims[current];
                        const __int128 term = static_cast<__int128>(coordinate) * layout.strides[current];
                        if (term < 0) {
                            if (term < kAddressOffsetMin - offset) {
                                offsetOverflow = true;
                                break;
                            }
                        } else if (term > kAddressOffsetMax - offset) {
                            offsetOverflow = true;
                            break;
                        }
                        offset += term;
                    }
                    if (offsetOverflow) {
                        return {};
                    }
                    const auto address = addressWithOffset(data.address, offset);
                    if (!address) {
                        return {};
                    }
                    appendAccess(*address, layout.elementBytes);
                }
            } else {
                const auto first = addressWithOffset(data.address, minOffset);
                const auto last = addressWithOffset(data.address, maxOffset);
                if (!first || !last) {
                    return {};
                }
                if (*last < *first || *last - *first > std::numeric_limits<uint64_t>::max() - layout.elementBytes) {
                    return {};
                }
                // For large layouts, the extrema form a safe allocation
                // envelope: if both extrema are inside one allocation, every
                // affine element is inside it; otherwise the envelope exposes
                // the crossed allocation boundary without enumerating lanes.
                appendAccess(*first, *last - *first + layout.elementBytes);
            }
            break;
        }
        default:
            return {};
    }

    return diagnostics;
}

std::vector<Diagnostic> Memcheck::OnSynchronization()
{
    ++stats_.synchronizationEvents;
    std::vector<AclsanDeviceMemoryAccessData> accesses;
    accesses.swap(pendingDeviceAccesses_);
    stats_.pendingDeviceOperations = 0;

    std::vector<Diagnostic> diagnostics;
    for (const auto& access : accesses) {
        auto found = CheckDeviceMemoryAccess(access);
        diagnostics.insert(diagnostics.end(), found.begin(), found.end());
    }
    Count(diagnostics);
    return diagnostics;
}

MemcheckStats Memcheck::Stats() const
{
    MemcheckStats stats = stats_;
    stats.pendingDeviceOperations = pendingDeviceAccesses_.size();
    return stats;
}

void Memcheck::Count(const std::vector<Diagnostic>& diagnostics)
{
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == Severity::ERROR) {
            ++stats_.errors;
        } else {
            ++stats_.warnings;
        }
    }
}

} // namespace npu::sanitizer
