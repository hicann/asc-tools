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
#include "cce_instr/cce_instr_types.h"
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

constexpr uint64_t kUnknownLaunchId = 0;
constexpr uint32_t kDefaultDeviceId = 0;
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
    static std::optional<MemoryInstructionProfile> Create(
        const sanitizer::CopyGmToUbufAlignV2ParamField& field) noexcept
    {
        const uint32_t dataBits = VectorReadDataBits(field.instr_id);
        if (dataBits == 0) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAiv};
    }

    static std::optional<MemoryInstructionProfile> Create(
        const sanitizer::CopyGmToCbufAlignV2ParamField& field) noexcept
    {
        const uint32_t dataBits = CubeReadDataBits(field.instr_id);
        if (dataBits == 0) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAic};
    }

    template <sanitizer::NdNzConversionMode ConversionMode>
    static std::optional<MemoryInstructionProfile> Create(
        const sanitizer::CopyGmToCbufMultiParamField<ConversionMode>& field) noexcept
    {
        const uint32_t dataBits = MultiReadDataBits<ConversionMode>(field.instr_id);
        if (dataBits == 0) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{dataBits, ACLSAN_DEVICE_SOURCE_MTE2, kBlockTypeAic};
    }

    static std::optional<MemoryInstructionProfile> Create(
        const sanitizer::CopyUbufToGmAlignV2ParamField& field) noexcept
    {
        if (static_cast<sanitizer::CceInstructionId>(field.instr_id) !=
            sanitizer::CceInstructionId::CopyUbufToGmAlignV2) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{0, ACLSAN_DEVICE_SOURCE_MTE3, kBlockTypeAiv};
    }

    static std::optional<MemoryInstructionProfile> Create(const FixpipeMemoryField& field) noexcept
    {
        const auto id = static_cast<sanitizer::CceInstructionId>(field.field.instr_id);
        if (id != sanitizer::CceInstructionId::FixL0cToOutF32 && id != sanitizer::CceInstructionId::FixL0cToOutS32) {
            return std::nullopt;
        }
        return MemoryInstructionProfile{
            static_cast<uint32_t>(field.dstElementBytes) * 8U, ACLSAN_DEVICE_SOURCE_FIXPIPE, kBlockTypeAic};
    }

private:
    static uint32_t VectorReadDataBits(uint32_t instructionId) noexcept
    {
        switch (static_cast<sanitizer::CceInstructionId>(instructionId)) {
            case sanitizer::CceInstructionId::CopyGmToUbufAlignV2B8:
                return 8;
            case sanitizer::CceInstructionId::CopyGmToUbufAlignV2B16:
                return 16;
            case sanitizer::CceInstructionId::CopyGmToUbufAlignV2B32:
                return 32;
            default:
                return 0;
        }
    }

    static uint32_t CubeReadDataBits(uint32_t instructionId) noexcept
    {
        switch (static_cast<sanitizer::CceInstructionId>(instructionId)) {
            case sanitizer::CceInstructionId::CopyGmToCbufAlignV2B8:
                return 8;
            case sanitizer::CceInstructionId::CopyGmToCbufAlignV2B16:
                return 16;
            case sanitizer::CceInstructionId::CopyGmToCbufAlignV2B32:
                return 32;
            default:
                return 0;
        }
    }

    template <sanitizer::NdNzConversionMode ConversionMode>
    static uint32_t MultiReadDataBits(uint32_t instructionId) noexcept
    {
        const auto id = static_cast<sanitizer::CceInstructionId>(instructionId);
        if constexpr (ConversionMode == sanitizer::NdNzConversionMode::ND2NZ) {
            if (id == sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB8) {
                return 8;
            }
            if (id == sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB16) {
                return 16;
            }
            if (id == sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB32) {
                return 32;
            }
        } else {
            if (id == sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB8) {
                return 8;
            }
            if (id == sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB16) {
                return 16;
            }
            if (id == sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB32) {
                return 32;
            }
        }
        return 0;
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
            sparseAxes[0].count <= std::numeric_limits<uint64_t>::max() / sparseAxes[1].count) {
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
        if (failed_) {
            return false;
        }
        if (descriptors_.size() >= kMaxExpandedMemoryAccessesPerInstruction) {
            Fail();
            return false;
        }
        try {
            descriptors_.push_back(std::move(descriptor));
        } catch (const std::bad_alloc&) {
            Fail();
            return false;
        } catch (const std::length_error&) {
            Fail();
            return false;
        }
        return true;
    }

    [[nodiscard]] MemoryCbdataResult Build() && noexcept
    {
        if (failed_) {
            return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
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
    void Fail() noexcept
    {
        failed_ = true;
        descriptors_.clear();
    }

    AclsanDeviceEventHeader MakeHeader() const noexcept
    {
        return {ACLSAN_API_VERSION,   static_cast<uint32_t>(sizeof(AclsanDeviceMemoryAccessData)),
                kUnknownLaunchId,     context_.pc,
                context_.siteId,      static_cast<uint32_t>(profile_.sourceKind),
                context_.instrExecId, context_.serialNo,
                kDefaultDeviceId,     context_.coreId,
                context_.blockId,     profile_.blockType,
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
    bool failed_ = false;
};

class MemoryFieldVisitor final {
public:
    explicit MemoryFieldVisitor(MemoryCbdataContext context) noexcept : context_(context) {}

    MemoryCbdataResult operator()(const sanitizer::CopyGmToUbufAlignV2ParamField& field) const noexcept
    {
        return ConvertGmRead(field);
    }

    MemoryCbdataResult operator()(const sanitizer::CopyGmToCbufAlignV2ParamField& field) const noexcept
    {
        return ConvertGmRead(field);
    }

    MemoryCbdataResult operator()(const MultiNd2NzMemoryField& field) const noexcept
    {
        return ConvertMultiGmRead(field);
    }

    MemoryCbdataResult operator()(const MultiDn2NzMemoryField& field) const noexcept
    {
        return ConvertMultiGmRead(field);
    }

    MemoryCbdataResult operator()(const sanitizer::CopyUbufToGmAlignV2ParamField& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.burstNum == 0 || field.burstLen == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        MemoryCbdataBuilder builder(context_, *profile);
        return BuildSingleAccess(
            std::move(builder), MemoryAccessDescriptorFactory::Linear(
                                    field.dstAddr, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, profile->dataBits, field.burstNum,
                                    field.burstLen, field.dstStride));
    }

    MemoryCbdataResult operator()(const FixpipeMemoryField& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.nSize == 0 || field.mSize == 0 || field.dstStride == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if (field.dstElementBytes == 0 || (field.nz2nd && field.nz2dn) ||
            (field.channelSplit && (field.nz2nd || field.nz2dn))) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }

        const uint64_t dstStrideBytes = static_cast<uint64_t>(field.dstStride) * field.dstElementBytes;
        MemoryCbdataBuilder builder(context_, *profile);
        if (field.nz2nd) {
            const uint32_t rowBytes = static_cast<uint32_t>(field.nSize) * field.dstElementBytes;
            return BuildSingleAccess(
                std::move(builder), MemoryAccessDescriptorFactory::Linear(
                                        field.field.dstAddr, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, profile->dataBits,
                                        field.mSize, rowBytes, dstStrideBytes));
        }
        if (field.nz2dn) {
            const uint32_t columnBytes = static_cast<uint32_t>(field.mSize) * field.dstElementBytes;
            return BuildSingleAccess(
                std::move(builder), MemoryAccessDescriptorFactory::Linear(
                                        field.field.dstAddr, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, profile->dataBits,
                                        field.nSize, columnBytes, dstStrideBytes));
        }
        return ConvertFixpipeNz(field, profile->dataBits, dstStrideBytes, std::move(builder));
    }

private:
    template <typename Field>
    MemoryCbdataResult ConvertGmRead(const Field& field) const noexcept
    {
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        if (field.burstNum == 0 || field.burstLen == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        MemoryCbdataBuilder builder(context_, *profile);
        return BuildSingleAccess(
            std::move(builder), MemoryAccessDescriptorFactory::Linear(
                                    field.srcAddr, ACLSAN_DEVICE_MEMORY_ACCESS_READ, profile->dataBits, field.burstNum,
                                    field.burstLen, field.burstSrcStride));
    }

    template <sanitizer::NdNzConversionMode ConversionMode>
    MemoryCbdataResult ConvertMultiGmRead(const MultiMemoryField<ConversionMode>& input) const noexcept
    {
        const auto& field = input.field;
        const auto profile = MemoryInstructionProfileFactory::Create(field);
        if (!profile.has_value()) {
            return {MemoryCbdataStatus::INVALID_FIELD, {}};
        }
        const uint64_t elementBytes = profile->dataBits / 8U;
        const uint64_t rowCount = ConversionMode == sanitizer::NdNzConversionMode::ND2NZ ? field.nValue : field.dValue;
        const uint64_t rowElements =
            ConversionMode == sanitizer::NdNzConversionMode::ND2NZ ? field.dValue : field.nValue;
        if (input.matrixNum == 0 || rowCount == 0 || rowElements == 0) {
            return {MemoryCbdataStatus::NO_ACCESS, {}};
        }
        if (rowElements > std::numeric_limits<uint64_t>::max() / elementBytes) {
            return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
        }

        MemoryCbdataBuilder builder(context_, *profile);
        return BuildSingleAccess(
            std::move(builder), MemoryAccessDescriptorFactory::NdRead(
                                    field.srcAddr, profile->dataBits, rowElements * elementBytes,
                                    {rowCount, input.matrixNum}, {field.loop1SrcStride, field.loop4SrcStride}));
    }

    static MemoryCbdataResult BuildSingleAccess(
        MemoryCbdataBuilder&& builder, MemoryAccessDescriptor descriptor) noexcept
    {
        if (!builder.Add(std::move(descriptor))) {
            return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
        }
        return std::move(builder).Build();
    }

    static MemoryCbdataResult ConvertFixpipeNz(
        const FixpipeMemoryField& field, uint32_t dataBits, uint64_t groupStrideBytes,
        MemoryCbdataBuilder&& builder) noexcept
    {
        constexpr uint32_t kDefaultFractalSize = 16;
        constexpr uint32_t kChannelSplitFractalSize = 8;
        const uint32_t fractalSize = field.channelSplit ? kChannelSplitFractalSize : kDefaultFractalSize;
        const uint32_t fullGroups = field.nSize / fractalSize;
        const uint32_t tailRows = field.nSize % fractalSize;
        if (fullGroups != 0) {
            const uint32_t groupBytes = static_cast<uint32_t>(field.mSize) * fractalSize * field.dstElementBytes;
            if (!builder.Add(MemoryAccessDescriptorFactory::Linear(
                    field.field.dstAddr, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, dataBits, fullGroups, groupBytes,
                    groupStrideBytes))) {
                return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
            }
        }
        if (tailRows != 0) {
            if (fullGroups > (std::numeric_limits<uint64_t>::max() - field.field.dstAddr) / groupStrideBytes) {
                return {MemoryCbdataStatus::ARITHMETIC_OVERFLOW, {}};
            }
            const uint64_t tailAddress = field.field.dstAddr + static_cast<uint64_t>(fullGroups) * groupStrideBytes;
            const uint32_t tailBytes = static_cast<uint32_t>(field.mSize) * tailRows * field.dstElementBytes;
            if (!builder.Add(MemoryAccessDescriptorFactory::Linear(
                    tailAddress, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, dataBits, 1, tailBytes, groupStrideBytes))) {
                return {MemoryCbdataStatus::RESOURCE_EXHAUSTED, {}};
            }
        }
        return std::move(builder).Build();
    }

    MemoryCbdataContext context_;
};

} // namespace

MemoryFieldToCbdataConverter::MemoryFieldToCbdataConverter(MemoryCbdataContext context) noexcept : context_(context) {}

MemoryCbdataResult MemoryFieldToCbdataConverter::Convert(const MemoryInstructionField& field) const noexcept
{
    return std::visit(MemoryFieldVisitor{context_}, field);
}

} // namespace aclsan
