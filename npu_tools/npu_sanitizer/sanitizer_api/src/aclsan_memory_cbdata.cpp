/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "device_instr/common/instruction_id.h"
#include "internal/aclsan_memory_cbdata.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace aclsan {
namespace {

constexpr uint32_t kBlockTypeAic = 0;
constexpr uint32_t kBlockTypeAiv = 1;
constexpr std::size_t kMemoryNdMaxRank = 5;
constexpr std::size_t kMaxExpandedMemoryAccessesPerInstruction = 4096;

struct RangeDescriptor {
    uint64_t bytes;
};

struct StridedDescriptor {
    uint64_t bytes;
    uint64_t count;
    uint64_t strideBytes;
};

struct NdAffineDescriptor {
    uint32_t rank;
    uint64_t bytes;
    std::array<uint64_t, kMemoryNdMaxRank> counts;
    std::array<uint64_t, kMemoryNdMaxRank> strideBytes;
};

using MemoryLayoutDescriptor = std::variant<RangeDescriptor, StridedDescriptor, NdAffineDescriptor>;

struct MemoryAccessDescriptor {
    uint64_t address;
    uint32_t accessMode;
    uint32_t dataBits;
    MemoryLayoutDescriptor layout;
};

struct MemoryInstructionProfile {
    uint32_t dataBits;
    AclsanDeviceSourceKind sourceKind;
    uint32_t blockType;
};

class MemoryInstructionProfileFactory final {
public:
    static std::optional<MemoryInstructionProfile> Create(const CopyGmToUbufAlignV2ParamField& field) noexcept
    {
        const uint32_t dataBits = field.dataBits;
        if (!IsSupportedReadDataBits(dataBits)) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAiv};
    }

    static std::optional<MemoryInstructionProfile> Create(const CopyGmToCbufAlignV2ParamField& field) noexcept
    {
        const uint32_t dataBits = field.dataBits;
        if (!IsSupportedReadDataBits(dataBits)) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAic};
    }

    template <NdNzConversionMode ConversionMode>
    static std::optional<MemoryInstructionProfile> Create(
        const CopyGmToCbufMultiParamField<ConversionMode>& field) noexcept
    {
        const uint32_t dataBits = field.dataBits;
        if (!IsSupportedReadDataBits(dataBits)) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAic};
    }

    static std::optional<MemoryInstructionProfile> Create(const CopyGmToCbufV2ParamField& field) noexcept
    {
        if (static_cast<InstructionId>(field.instrId) != InstructionId::CopyGmToCbufV2) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{0, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAic};
    }

    static std::optional<MemoryInstructionProfile> Create(const CopyUbufToGmAlignV2ParamField& field) noexcept
    {
        if (static_cast<InstructionId>(field.instrId) != InstructionId::CopyUbufToGmAlignV2) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{0, ACLSAN_DEVICE_SOURCE_MTE3, kBlockTypeAiv};
    }

    static std::optional<MemoryInstructionProfile> Create(const FixL0cToOutParamField& field) noexcept
    {
        const auto id = static_cast<InstructionId>(field.instrId);
        const uint32_t dataBits = FixpipeDestinationDataBits(id, field.quantPre);
        if (dataBits == 0) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_FIXPIPE, kBlockTypeAic};
    }

    static std::optional<MemoryInstructionProfile> Create(const LoadGmToCbuf2DV2ParamField& field) noexcept
    {
        if (static_cast<InstructionId>(field.instrId) != InstructionId::LoadGmToCbuf2DV2) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{0, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAic};
    }

    static std::optional<MemoryInstructionProfile> Create(const NdDmaOutToUbufParamField& field) noexcept
    {
        const uint32_t dataBits = field.dataBits;
        if (!IsSupportedReadDataBits(dataBits)) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAiv};
    }

private:
    static uint32_t FixpipeDestinationDataBits(InstructionId id, uint8_t quantPre) noexcept
    {
        if (id == InstructionId::FixL0cToOutF32) {
            switch (quantPre) {
                case 0:
                case 14:
                case 15:
                    return 32;
                case 1:
                case 16:
                case 31:
                case 32:
                case 33:
                case 34:
                    return 16;
                case 2:
                case 3:
                case 4:
                case 5:
                case 12:
                case 13:
                case 23:
                case 24:
                    return 8;
                case 25:
                case 26:
                    return 4;
                default:
                    return 0;
            }
        }
        if (id != InstructionId::FixL0cToOutS32) {
            return 0;
        }
        switch (quantPre) {
            case 0:
                return 32;
            case 10:
            case 11:
            case 35:
            case 36:
                return 16;
            case 8:
            case 9:
                return 8;
            case 21:
            case 22:
                return 4;
            default:
                return 0;
        }
    }

    static bool IsSupportedReadDataBits(uint32_t dataBits) noexcept
    {
        return dataBits == 8 || dataBits == 16 || dataBits == 32;
    }
};

class MemoryAccessDescriptorFactory final {
public:
    static MemoryAccessDescriptor Linear(
        uint64_t address, uint32_t accessMode, uint32_t dataBits, uint64_t count, uint64_t bytes,
        uint64_t strideBytes) noexcept
    {
        uint64_t rangeBytes = bytes;
        bool isRange = count == 1;
        if (count > 1 && strideBytes <= bytes &&
            (strideBytes == 0 || count - 1 <= (std::numeric_limits<uint64_t>::max() - bytes) / strideBytes)) {
            rangeBytes += (count - 1) * strideBytes;
            isRange = true;
        }
        if (isRange) {
            return {address, accessMode, dataBits, RangeDescriptor{rangeBytes}};
        }
        return {address, accessMode, dataBits, StridedDescriptor{bytes, count, strideBytes}};
    }

    static MemoryAccessDescriptor NdRead(
        uint64_t address, uint32_t dataBits, uint64_t bytes, std::array<uint64_t, 2> counts,
        std::array<uint64_t, 2> strides) noexcept
    {
        struct Axis {
            uint64_t count;
            uint64_t stride;
        };
        std::array<Axis, 2> axes{};
        std::size_t axisCount = 0;
        for (std::size_t index = 0; index < counts.size(); ++index) {
            if (counts[index] > 1 && strides[index] != 0) {
                axes[axisCount++] = {counts[index], strides[index]};
            }
        }
        std::sort(axes.begin(), axes.begin() + axisCount, [](const Axis& left, const Axis& right) {
            return left.stride < right.stride;
        });

        std::array<Axis, 2> sparseAxes{};
        std::size_t sparseCount = 0;
        uint64_t segmentBytes = bytes;
        for (std::size_t index = 0; index < axisCount; ++index) {
            const Axis axis = axes[index];
            const unsigned __int128 span = static_cast<unsigned __int128>(axis.count - 1) * axis.stride + segmentBytes;
            if (axis.stride <= segmentBytes && span <= std::numeric_limits<uint64_t>::max()) {
                segmentBytes = static_cast<uint64_t>(span);
            } else {
                sparseAxes[sparseCount++] = axis;
            }
        }
        if (sparseCount == 0) {
            return Linear(address, ACLSAN_DEVICE_MEMORY_ACCESS_READ, dataBits, 1, segmentBytes, 0);
        }
        if (sparseCount == 1) {
            return Linear(
                address, ACLSAN_DEVICE_MEMORY_ACCESS_READ, dataBits, sparseAxes[0].count, segmentBytes,
                sparseAxes[0].stride);
        }
        const unsigned __int128 flattenedStride =
            static_cast<unsigned __int128>(sparseAxes[0].stride) * sparseAxes[0].count;
        if (flattenedStride == sparseAxes[1].stride &&
            sparseAxes[0].count <= std::numeric_limits<uint32_t>::max() / sparseAxes[1].count) {
            return Linear(
                address, ACLSAN_DEVICE_MEMORY_ACCESS_READ, dataBits, sparseAxes[0].count * sparseAxes[1].count,
                segmentBytes, sparseAxes[0].stride);
        }

        NdAffineDescriptor layout{};
        layout.rank = 2;
        layout.bytes = segmentBytes;
        for (std::size_t index = 0; index < sparseCount; ++index) {
            layout.counts[index] = sparseAxes[index].count;
            layout.strideBytes[index] = sparseAxes[index].stride;
        }
        return {address, ACLSAN_DEVICE_MEMORY_ACCESS_READ, dataBits, layout};
    }

    static MemoryAccessDescriptor NdRead(
        uint64_t address, uint32_t dataBits, uint64_t elementBytes, const std::array<uint64_t, 5>& counts,
        const std::array<uint64_t, 5>& strides) noexcept
    {
        return {
            address, ACLSAN_DEVICE_MEMORY_ACCESS_READ, dataBits, NdAffineDescriptor{5, elementBytes, counts, strides}};
    }

    static MemoryAccessDescriptor NdWrite(
        uint64_t address, uint32_t dataBits, uint64_t bytes, std::array<uint64_t, 2> counts,
        std::array<uint64_t, 2> strides) noexcept
    {
        MemoryAccessDescriptor descriptor = NdRead(address, dataBits, bytes, counts, strides);
        descriptor.accessMode = ACLSAN_DEVICE_MEMORY_ACCESS_WRITE;
        return descriptor;
    }

    static MemoryAccessDescriptor DmaAffine(
        uint64_t address, uint32_t accessMode, uint32_t dataBits, uint64_t bytes, const std::array<uint64_t, 3>& counts,
        const std::array<uint64_t, 3>& strides) noexcept
    {
        NdAffineDescriptor layout{};
        layout.rank = 3;
        layout.bytes = bytes;
        std::copy(counts.begin(), counts.end(), layout.counts.begin());
        std::copy(strides.begin(), strides.end(), layout.strideBytes.begin());
        return {address, accessMode, dataBits, layout};
    }
};

class MemoryCbdataBuilder final {
public:
    MemoryCbdataBuilder(MemoryCbdataContext context, MemoryInstructionProfile profile) noexcept
        : context_(context), profile_(profile)
    {}

    MemoryCbdataBuilder(const MemoryCbdataBuilder&) = delete;
    MemoryCbdataBuilder& operator=(const MemoryCbdataBuilder&) = delete;
    MemoryCbdataBuilder(MemoryCbdataBuilder&&) noexcept = default;
    MemoryCbdataBuilder& operator=(MemoryCbdataBuilder&&) noexcept = default;

    [[nodiscard]] bool Add(MemoryAccessDescriptor descriptor) noexcept
    {
        if (status_ != MemoryCbdataStatus::SUCCESS) {
            return false;
        }
        if (!IsRepresentable(descriptor)) {
            Fail(MemoryCbdataStatus::ARITHMETIC_OVERFLOW);
            return false;
        }
        if (descriptors_.size() >= kMaxExpandedMemoryAccessesPerInstruction) {
            Fail(MemoryCbdataStatus::RESOURCE_EXHAUSTED);
            return false;
        }
        try {
            descriptors_.push_back(std::move(descriptor));
        } catch (const std::bad_alloc&) {
            Fail(MemoryCbdataStatus::RESOURCE_EXHAUSTED);
            return false;
        } catch (const std::length_error&) {
            Fail(MemoryCbdataStatus::RESOURCE_EXHAUSTED);
            return false;
        }
        return true;
    }

    [[nodiscard]] MemoryCbdataResult Build() && noexcept
    {
        if (status_ != MemoryCbdataStatus::SUCCESS) {
            return {status_, {}};
        }
        if (descriptors_.empty()) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }

        MemoryCbdata data;
        try {
            data.reserve(descriptors_.size());
            const AclsanDeviceEventHeader header = MakeHeader();
            const uint32_t accessCount = static_cast<uint32_t>(descriptors_.size());
            for (uint32_t index = 0; index < accessCount; ++index) {
                data.push_back(MakeCbdata(header, descriptors_[index], index, accessCount));
            }
        } catch (const std::bad_alloc&) {
            return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
        } catch (const std::length_error&) {
            return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
        }
        return {MemoryCbdataStatus::SUCCESS, std::move(data)};
    }

private:
    static bool IsRepresentable(const MemoryAccessDescriptor& descriptor) noexcept
    {
        unsigned __int128 lastOffset = 0;
        const bool valid = std::visit(
            [&lastOffset](const auto& layout) noexcept {
                using Layout = std::decay_t<decltype(layout)>;
                if (layout.bytes == 0) {
                    return false;
                }
                lastOffset = layout.bytes - 1;
                if constexpr (std::is_same_v<Layout, RangeDescriptor>) {
                    return true;
                } else if constexpr (std::is_same_v<Layout, StridedDescriptor>) {
                    if (layout.bytes > std::numeric_limits<uint32_t>::max() || layout.count == 0 ||
                        layout.count > std::numeric_limits<uint32_t>::max() ||
                        layout.strideBytes > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                        return false;
                    }
                    lastOffset += static_cast<unsigned __int128>(layout.count - 1) * layout.strideBytes;
                    return true;
                } else {
                    if (layout.bytes > std::numeric_limits<uint32_t>::max() || layout.rank == 0 ||
                        layout.rank > kMemoryNdMaxRank) {
                        return false;
                    }
                    for (std::size_t index = 0; index < layout.rank; ++index) {
                        if (layout.counts[index] == 0 ||
                            layout.strideBytes[index] > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                            return false;
                        }
                        lastOffset +=
                            static_cast<unsigned __int128>(layout.counts[index] - 1) * layout.strideBytes[index];
                    }
                    return true;
                }
            },
            descriptor.layout);
        return valid &&
               static_cast<unsigned __int128>(descriptor.address) + lastOffset <= std::numeric_limits<uint64_t>::max();
    }

    void Fail(MemoryCbdataStatus status) noexcept
    {
        status_ = status;
        descriptors_.clear();
    }

    AclsanDeviceEventHeader MakeHeader() const noexcept
    {
        return {ACLSAN_API_VERSION,   static_cast<uint32_t>(sizeof(AclsanDeviceMemoryAccessData)),
                context_.launchId,    context_.pc,
                context_.siteId,      static_cast<uint32_t>(profile_.sourceKind),
                context_.instrExecId, context_.serialNo,
                context_.deviceId,    context_.coreId,
                context_.blockId,     context_.blockType,
                context_.pipeline,    ACLSAN_DEVICE_EVENT_FLAG_EXACT};
    }

    static AclsanDeviceMemoryAccessData MakeCbdata(
        const AclsanDeviceEventHeader& header, const MemoryAccessDescriptor& descriptor, uint32_t accessIndex,
        uint32_t accessCount) noexcept
    {
        AclsanDeviceMemoryAccessData data{};
        data.header = header;
        data.address = descriptor.address;
        data.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_GM;
        data.accessMode = descriptor.accessMode;
        data.accessIndex = accessIndex;
        data.accessCount = accessCount;
        data.dataBits = descriptor.dataBits;
        std::visit(
            [&data](const auto& layout) noexcept {
                using Layout = std::decay_t<decltype(layout)>;
                if constexpr (std::is_same_v<Layout, RangeDescriptor>) {
                    data.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
                    data.layout.range.bytes = layout.bytes;
                } else if constexpr (std::is_same_v<Layout, StridedDescriptor>) {
                    data.layoutKind = ACLSAN_MEM_LAYOUT_BLOCK_REPEAT;
                    data.layout.blockRepeat = {
                        1, static_cast<uint32_t>(layout.bytes),      0, static_cast<uint32_t>(layout.count),
                        0, static_cast<int64_t>(layout.strideBytes),
                    };
                } else {
                    data.layoutKind = ACLSAN_MEM_LAYOUT_ND_AFFINE;
                    data.layout.ndAffine.rank = layout.rank;
                    data.layout.ndAffine.elementBytes = static_cast<uint32_t>(layout.bytes);
                    std::copy(layout.counts.begin(), layout.counts.end(), data.layout.ndAffine.dims);
                    std::transform(
                        layout.strideBytes.begin(), layout.strideBytes.end(), data.layout.ndAffine.strides,
                        [](uint64_t stride) { return static_cast<int64_t>(stride); });
                }
            },
            descriptor.layout);
        return data;
    }

    MemoryCbdataContext context_;
    MemoryInstructionProfile profile_;
    std::vector<MemoryAccessDescriptor> descriptors_;
    MemoryCbdataStatus status_ = MemoryCbdataStatus::SUCCESS;
};

class MemoryFieldVisitor final {
public:
    MemoryFieldVisitor(MemoryCbdataContext context, const MemoryRegisterState& registerState) noexcept
        : context_(context), registerState_(registerState)
    {}

    MemoryCbdataResult operator()(const CopyGmToUbufAlignV2ParamField& field) const noexcept
    {
        return ConvertGmRead(field, DmaLoopDirection::GM_TO_UBUF);
    }

    MemoryCbdataResult operator()(const CopyGmToCbufAlignV2ParamField& field) const noexcept
    {
        return ConvertGmRead(field, DmaLoopDirection::GM_TO_CBUF);
    }

    MemoryCbdataResult operator()(const CopyGmToCbufMultiNd2NzParamField& field) const noexcept
    {
        return ConvertMultiGmRead(field);
    }

    MemoryCbdataResult operator()(const CopyGmToCbufMultiDn2NzParamField& field) const noexcept
    {
        return ConvertMultiGmRead(field);
    }

    MemoryCbdataResult operator()(const CopyGmToCbufV2ParamField& field) const noexcept
    {
        constexpr uint64_t kC0Bytes = 32;
        constexpr std::array<uint64_t, 5> kInsertedPaddingSourceBytes{1, 2, 4, 8, 16};
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.burstNum == 0 || field.burstLen == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if (field.padFunctionMode > 8 ||
            (field.padFunctionMode >= 1 && field.padFunctionMode <= 5 && field.burstLen != 1)) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        const bool insertsPadding = field.padFunctionMode >= 1 && field.padFunctionMode <= 5;
        if (field.burstLen > std::numeric_limits<uint64_t>::max() / kC0Bytes ||
            (!insertsPadding && field.srcStride > std::numeric_limits<uint64_t>::max() / kC0Bytes)) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }
        const uint64_t burstBytes = insertsPadding ? kInsertedPaddingSourceBytes[field.padFunctionMode - 1] :
                                                     static_cast<uint64_t>(field.burstLen) * kC0Bytes;
        const uint64_t burstStride = insertsPadding ? burstBytes : field.srcStride * kC0Bytes;
        if (burstBytes > std::numeric_limits<uint32_t>::max() ||
            burstStride > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }
        return ConvertDmaAccess(
            *profile, DmaLoopDirection::GM_TO_CBUF, field.srcAddr, ACLSAN_DEVICE_MEMORY_ACCESS_READ, field.burstNum,
            burstBytes, burstStride);
    }

    MemoryCbdataResult operator()(const CopyUbufToGmAlignV2ParamField& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.burstNum == 0 || field.burstLen == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        return ConvertDmaAccess(
            *profile, DmaLoopDirection::UBUF_TO_GM, field.dstAddr, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, field.burstNum,
            field.burstLen, field.dstStride);
    }

    MemoryCbdataResult operator()(const FixL0cToOutParamField& field) const noexcept
    {
        if (HasUnsupportedFixpipeMode(field)) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.nSize == 0 || field.mSize == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if ((field.nz2ndEnable && field.nz2dnEnable) ||
            (field.splitEnable && (field.nz2ndEnable || field.nz2dnEnable))) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.splitEnable &&
            (field.instrId != static_cast<uint32_t>(InstructionId::FixL0cToOutF32) || profile->dataBits != 32)) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (!field.nz2ndEnable && !field.nz2dnEnable &&
            ((field.splitEnable && field.nSize % 8 != 0) || (!field.splitEnable && field.nSize % 16 != 0))) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.c0PadEnable &&
            (field.instrId != static_cast<uint32_t>(InstructionId::FixL0cToOutF32) || field.sid != 0 ||
             field.nSize % 16 != 0 || field.mSize != 16 || field.loopDstStride != 256 || field.loopSrtStride != 16 ||
             field.l2CacheControl != 0 || field.clipReluPre != 0 || field.unitFlag != 0 || field.quantPre != 0 ||
             field.reluPre != 0 || field.splitEnable || field.nz2ndEnable || field.nz2dnEnable)) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }

        MemoryCbdataBuilder builder(context_, *profile);
        if (profile->dataBits == 4) {
            if (!field.nz2ndEnable && !field.nz2dnEnable && field.nSize % 64 != 0) {
                return {MemoryCbdataStatus::INVALID_FIELD, {}};
            }
            return ConvertFixpipePacked4(field, std::move(builder));
        }

        const uint64_t dstElementBytes = profile->dataBits / 8U;
        if (field.loopDstStride > std::numeric_limits<uint64_t>::max() / dstElementBytes) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }
        const uint64_t dstStrideBytes = static_cast<uint64_t>(field.loopDstStride) * dstElementBytes;
        if (field.nz2ndEnable || field.nz2dnEnable) {
            if (!registerState_.loop3.has_value()) {
                return {
                    MemoryCbdataStatus::MISSING_REGISTER_STATE, {}, static_cast<uint64_t>(InstructionId::Loop3Param)};
            }
            const Loop3ParamField& loop3 = *registerState_.loop3;
            if (loop3.loopCount == 0) {
                return {MemoryCbdataStatus::NO_ACCESS, {}};
            }
            if (loop3.dstStride > std::numeric_limits<uint64_t>::max() / dstElementBytes) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            const uint64_t matrixStrideBytes = static_cast<uint64_t>(loop3.dstStride) * dstElementBytes;
            const uint64_t segmentElements = field.nz2ndEnable ? field.nSize : field.mSize;
            if (segmentElements > std::numeric_limits<uint64_t>::max() / dstElementBytes) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            const uint64_t innerCount = field.nz2ndEnable ? field.mSize : field.nSize;
            return BuildSingleAccess(
                std::move(builder), MemoryAccessDescriptorFactory::NdWrite(
                                        field.dstAddr, profile->dataBits, segmentElements * dstElementBytes,
                                        std::array<uint64_t, 2>{innerCount, loop3.loopCount},
                                        std::array<uint64_t, 2>{dstStrideBytes, matrixStrideBytes}));
        }
        return ConvertFixpipeNz(field, profile->dataBits, dstElementBytes, dstStrideBytes, std::move(builder));
    }

    MemoryCbdataResult operator()(const LoadGmToCbuf2DV2ParamField& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.decompMode != 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if (field.mStep == 0 || field.kStep == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if (!registerState_.mte2Source.has_value()) {
            return {MemoryCbdataStatus::MISSING_REGISTER_STATE, {}, static_cast<uint64_t>(InstructionId::Mte2SrcPara)};
        }
        constexpr uint64_t kFractalBytes = 512;
        const int64_t signedSrcStride = registerState_.mte2Source->srcStride;
        const __int128 strideMagnitude =
            signedSrcStride < 0 ? -static_cast<__int128>(signedSrcStride) : static_cast<__int128>(signedSrcStride);
        const __int128 repeatStrideBytes = strideMagnitude * kFractalBytes;
        if (repeatStrideBytes > std::numeric_limits<int64_t>::max()) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }

        const __int128 firstFractal =
            static_cast<__int128>(field.kStartPosition) * strideMagnitude + field.mStartPosition;
        const __int128 firstAddress = static_cast<__int128>(field.srcAddr) + firstFractal * kFractalBytes;
        const __int128 lowestAddress = signedSrcStride < 0 ?
                                           firstAddress - static_cast<__int128>(field.kStep - 1) * repeatStrideBytes :
                                           firstAddress;
        if (lowestAddress < 0 || lowestAddress > std::numeric_limits<uint64_t>::max()) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }
        MemoryCbdataBuilder builder(context_, *profile);
        return BuildSingleAccess(
            std::move(builder), MemoryAccessDescriptorFactory::NdRead(
                                    static_cast<uint64_t>(lowestAddress), profile->dataBits, kFractalBytes,
                                    std::array<uint64_t, 2>{field.mStep, field.kStep},
                                    std::array<uint64_t, 2>{kFractalBytes, static_cast<uint64_t>(repeatStrideBytes)}));
    }

    MemoryCbdataResult operator()(const NdDmaOutToUbufParamField& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        const std::array<uint64_t, 5> counts{
            field.loop0Size, field.loop1Size, field.loop2Size, field.loop3Size, field.loop4Size};
        for (std::size_t index = 0; index < counts.size(); ++index) {
            if (!registerState_.ndDmaLoopStrides[index].has_value()) {
                return {
                    MemoryCbdataStatus::MISSING_REGISTER_STATE,
                    {},
                    static_cast<uint64_t>(InstructionId::NdDmaLoop0Stride) + index};
            }
        }
        if (std::any_of(counts.begin(), counts.end(), [](uint64_t count) { return count == 0; })) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }

        const uint64_t elementBytes = profile->dataBits / 8U;
        std::array<uint64_t, 5> strideBytes{};
        for (std::size_t index = 0; index < strideBytes.size(); ++index) {
            if (counts[index] <= 1) {
                continue;
            }
            const uint64_t srcStride = registerState_.ndDmaLoopStrides[index]->srcStride;
            if (srcStride > std::numeric_limits<uint64_t>::max() / elementBytes) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            strideBytes[index] = srcStride * elementBytes;
        }

        MemoryCbdataBuilder builder(context_, *profile);
        return BuildSingleAccess(
            std::move(builder),
            MemoryAccessDescriptorFactory::NdRead(field.srcAddr, profile->dataBits, elementBytes, counts, strideBytes));
    }

private:
    static bool HasUnsupportedFixpipeMode(const FixL0cToOutParamField& field) noexcept
    {
        return field.quantPost != 0 || field.reluPost != 0 || field.clipReluPost || field.loopEnhanceEnable ||
               field.eltwiseOp != 0 || field.eltwiseAntqEnable || field.loopEnhanceMergeEnable ||
               field.winoPostEnable || field.brcbEnable;
    }

    template <typename Field>
    MemoryCbdataResult ConvertGmRead(const Field& field, DmaLoopDirection direction) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.burstNum == 0 || field.burstLen == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        return ConvertDmaAccess(
            *profile, direction, field.srcAddr, ACLSAN_DEVICE_MEMORY_ACCESS_READ, field.burstNum, field.burstLen,
            field.burstSrcStride);
    }

    MemoryCbdataResult ConvertDmaAccess(
        const MemoryInstructionProfile& profile, DmaLoopDirection direction, uint64_t address, uint32_t accessMode,
        uint64_t burstNum, uint64_t burstBytes, uint64_t burstStride) const noexcept
    {
        if (burstBytes > std::numeric_limits<uint32_t>::max() ||
            burstStride > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }
        const auto directionIndex = static_cast<std::size_t>(direction);
        if (directionIndex >= registerState_.dmaLoopSizes.size()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }

        std::array<uint64_t, 2> loopCounts{1, 1};
        if (registerState_.dmaLoopSizes[directionIndex].has_value()) {
            const DmaLoopSizeParamField& size = *registerState_.dmaLoopSizes[directionIndex];
            loopCounts = {size.loop1Size, size.loop2Size};
        }
        if (loopCounts[0] == 0 || loopCounts[1] == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }

        std::array<uint64_t, 2> loopStrides{};
        for (std::size_t index = 0; index < loopCounts.size(); ++index) {
            if (loopCounts[index] <= 1) {
                continue;
            }
            const auto& strideState = registerState_.dmaLoopStrides[directionIndex][index];
            if (!strideState.has_value()) {
                return {
                    MemoryCbdataStatus::MISSING_REGISTER_STATE,
                    {},
                    MissingDmaLoopStrideInstructionId(direction, index)};
            }
            loopStrides[index] =
                direction == DmaLoopDirection::UBUF_TO_GM ? strideState->dstStride : strideState->srcStride;
            if (loopStrides[index] > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
        }

        MemoryCbdataBuilder builder(context_, profile);
        if (loopCounts[0] == 1 && loopCounts[1] == 1) {
            return BuildSingleAccess(
                std::move(builder), MemoryAccessDescriptorFactory::Linear(
                                        address, accessMode, profile.dataBits, burstNum, burstBytes, burstStride));
        }
        return BuildSingleAccess(
            std::move(builder), MemoryAccessDescriptorFactory::DmaAffine(
                                    address, accessMode, profile.dataBits, burstBytes,
                                    std::array<uint64_t, 3>{burstNum, loopCounts[0], loopCounts[1]},
                                    std::array<uint64_t, 3>{burstStride, loopStrides[0], loopStrides[1]}));
    }

    template <NdNzConversionMode ConversionMode>
    MemoryCbdataResult ConvertMultiGmRead(const CopyGmToCbufMultiParamField<ConversionMode>& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        const uint64_t elementBytes = profile->dataBits / 8U;
        const uint64_t rowCount = ConversionMode == NdNzConversionMode::ND2NZ ? field.nValue : field.dValue;
        const uint64_t rowElements = ConversionMode == NdNzConversionMode::ND2NZ ? field.dValue : field.nValue;
        if (!registerState_.mte2Nz.has_value()) {
            return {
                MemoryCbdataStatus::MISSING_REGISTER_STATE, {}, static_cast<uint64_t>(InstructionId::SetMte2NzPara)};
        }
        const uint16_t matrixNum = registerState_.mte2Nz->matrixNum;
        if (matrixNum == 0 || rowCount == 0 || rowElements == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if (rowElements > std::numeric_limits<uint64_t>::max() / elementBytes) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }

        MemoryCbdataBuilder builder(context_, *profile);
        return BuildSingleAccess(
            std::move(builder), MemoryAccessDescriptorFactory::NdRead(
                                    field.srcAddr, profile->dataBits, rowElements * elementBytes,
                                    std::array<uint64_t, 2>{rowCount, matrixNum},
                                    std::array<uint64_t, 2>{field.loop1SrcStride, field.loop4SrcStride}));
    }

    static MemoryCbdataResult BuildSingleAccess(
        MemoryCbdataBuilder&& builder, MemoryAccessDescriptor descriptor) noexcept
    {
        if (!builder.Add(std::move(descriptor))) {
            return std::move(builder).Build();
        }
        return std::move(builder).Build();
    }

    static uint64_t MissingDmaLoopStrideInstructionId(DmaLoopDirection direction, std::size_t loopIndex) noexcept
    {
        uint64_t loop1InstructionId = 0;
        switch (direction) {
            case DmaLoopDirection::UBUF_TO_GM:
                loop1InstructionId = static_cast<uint64_t>(InstructionId::Loop1StrideUbufToGm);
                break;
            case DmaLoopDirection::GM_TO_UBUF:
                loop1InstructionId = static_cast<uint64_t>(InstructionId::Loop1StrideGmToUbuf);
                break;
            case DmaLoopDirection::GM_TO_CBUF:
                loop1InstructionId = static_cast<uint64_t>(InstructionId::Loop1StrideGmToCbuf);
                break;
        }
        return loop1InstructionId + loopIndex;
    }

    static std::optional<MemoryAccessDescriptor> Packed4Write(
        uint64_t baseAddress, unsigned __int128 elementOffset, unsigned __int128 elementCount) noexcept
    {
        const unsigned __int128 bitOffset = elementOffset * 4U;
        const unsigned __int128 byteOffset = bitOffset / 8U;
        const unsigned __int128 leadingBits = bitOffset % 8U;
        const unsigned __int128 byteCount = (leadingBits + elementCount * 4U + 7U) / 8U;
        if (elementCount == 0 || byteOffset > std::numeric_limits<uint64_t>::max() ||
            byteCount > std::numeric_limits<uint64_t>::max() ||
            static_cast<unsigned __int128>(baseAddress) + byteOffset > std::numeric_limits<uint64_t>::max()) {
            return std::nullopt;
        }
        return MemoryAccessDescriptorFactory::Linear(
            baseAddress + static_cast<uint64_t>(byteOffset), ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, 4, 1,
            static_cast<uint64_t>(byteCount), 0);
    }

    MemoryCbdataResult ConvertFixpipePacked4(
        const FixL0cToOutParamField& field, MemoryCbdataBuilder&& builder) const noexcept
    {
        MemoryCbdataStatus segmentStatus = MemoryCbdataStatus::SUCCESS;
        auto addSegment = [&builder, &field, &segmentStatus](
                              unsigned __int128 offset, unsigned __int128 elements) noexcept {
            const auto descriptor = Packed4Write(field.dstAddr, offset, elements);
            if (!descriptor.has_value()) {
                segmentStatus = MemoryCbdataStatus::ARITHMETIC_OVERFLOW;
                return false;
            }
            return builder.Add(*descriptor);
        };

        if (field.nz2ndEnable || field.nz2dnEnable) {
            if (!registerState_.loop3.has_value()) {
                return {
                    MemoryCbdataStatus::MISSING_REGISTER_STATE, {}, static_cast<uint64_t>(InstructionId::Loop3Param)};
            }
            const Loop3ParamField& loop3 = *registerState_.loop3;
            if (loop3.loopCount == 0) {
                return {MemoryCbdataStatus::NO_ACCESS, {}};
            }
            const uint64_t innerCount = field.nz2ndEnable ? field.mSize : field.nSize;
            const unsigned __int128 segmentCount = static_cast<unsigned __int128>(innerCount) * loop3.loopCount;
            if (segmentCount > kMaxExpandedMemoryAccessesPerInstruction) {
                return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
            }
            const uint64_t segmentElements = field.nz2ndEnable ? field.nSize : field.mSize;
            for (uint64_t inner = 0; inner < innerCount; ++inner) {
                for (uint64_t matrix = 0; matrix < loop3.loopCount; ++matrix) {
                    const unsigned __int128 offset = static_cast<unsigned __int128>(inner) * field.loopDstStride +
                                                     static_cast<unsigned __int128>(matrix) * loop3.dstStride;
                    if (!addSegment(offset, segmentElements)) {
                        return segmentStatus == MemoryCbdataStatus::SUCCESS ? std::move(builder).Build() :
                                                                              MemoryCbdataResult{segmentStatus, {}};
                    }
                }
            }
            return std::move(builder).Build();
        }

        constexpr uint32_t kFractalSize = 64;
        const uint32_t fullGroups = field.nSize / kFractalSize;
        const uint32_t tailRows = field.nSize % kFractalSize;
        for (uint32_t group = 0; group < fullGroups; ++group) {
            if (!addSegment(
                    static_cast<unsigned __int128>(group) * field.loopDstStride,
                    static_cast<unsigned __int128>(field.mSize) * kFractalSize)) {
                return segmentStatus == MemoryCbdataStatus::SUCCESS ? std::move(builder).Build() :
                                                                      MemoryCbdataResult{segmentStatus, {}};
            }
        }
        if (tailRows != 0 && !addSegment(
                                 static_cast<unsigned __int128>(fullGroups) * field.loopDstStride,
                                 static_cast<unsigned __int128>(field.mSize) * tailRows)) {
            return segmentStatus == MemoryCbdataStatus::SUCCESS ? std::move(builder).Build() :
                                                                  MemoryCbdataResult{segmentStatus, {}};
        }
        return std::move(builder).Build();
    }

    static MemoryCbdataResult ConvertFixpipeNz(
        const FixL0cToOutParamField& field, uint32_t dataBits, uint64_t dstElementBytes, uint64_t groupStrideBytes,
        MemoryCbdataBuilder&& builder) noexcept
    {
        constexpr uint32_t kDefaultFractalSize = 16;
        constexpr uint32_t kChannelSplitFractalSize = 8;
        constexpr uint32_t kChannelMergeB8FractalSize = 32;
        const uint32_t fractalSize = field.splitEnable ? kChannelSplitFractalSize :
                                     dataBits == 8     ? kChannelMergeB8FractalSize :
                                                         kDefaultFractalSize;
        const uint32_t fullGroups = field.nSize / fractalSize;
        const uint32_t tailRows = field.nSize % fractalSize;
        if (fullGroups != 0) {
            const uint64_t groupBytes = static_cast<uint64_t>(field.mSize) * fractalSize * dstElementBytes;
            if (groupBytes > std::numeric_limits<uint32_t>::max()) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            if (!builder.Add(MemoryAccessDescriptorFactory::Linear(
                    field.dstAddr, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, dataBits, fullGroups, groupBytes,
                    groupStrideBytes))) {
                return std::move(builder).Build();
            }
        }
        if (tailRows != 0) {
            if (groupStrideBytes != 0 &&
                fullGroups > (std::numeric_limits<uint64_t>::max() - field.dstAddr) / groupStrideBytes) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            const uint64_t tailAddress = field.dstAddr + static_cast<uint64_t>(fullGroups) * groupStrideBytes;
            const uint64_t tailBytes = static_cast<uint64_t>(field.mSize) * tailRows * dstElementBytes;
            if (tailBytes > std::numeric_limits<uint32_t>::max()) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            if (!builder.Add(MemoryAccessDescriptorFactory::Linear(
                    tailAddress, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, dataBits, 1, tailBytes, groupStrideBytes))) {
                return std::move(builder).Build();
            }
        }
        return std::move(builder).Build();
    }

    MemoryCbdataContext context_;
    const MemoryRegisterState& registerState_;
};

} // namespace

MemoryFieldToCbdataConverter::MemoryFieldToCbdataConverter(
    MemoryCbdataContext context, MemoryRegisterState registerState) noexcept
    : context_(context), registerState_(std::move(registerState))
{}

MemoryCbdataResult MemoryFieldToCbdataConverter::Convert(const MemoryInstructionField& field) const noexcept
{
    return std::visit(MemoryFieldVisitor{context_, registerState_}, field);
}

} // namespace aclsan
