/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl_pti/profiling/binary_instrumenter.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <elf.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::vector<char> MakeElfWithKernelArgumentSizes(
    const std::vector<std::uint32_t>& argumentSizes,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& parameters = {})
{
    constexpr char sectionNames[] = "\0.shstrtab\0__CCE_KernelArgSize\0.ascend.meta.test_kernel\0";
    const std::string names(sectionNames, sizeof(sectionNames));
    const std::size_t sectionCount = argumentSizes.empty() && parameters.empty() ? 2 : 4;
    const std::size_t sectionHeadersOffset = sizeof(Elf64_Ehdr);
    const std::size_t sectionNamesOffset = sectionHeadersOffset + sectionCount * sizeof(Elf64_Shdr);
    const std::size_t argumentSizesOffset = sectionNamesOffset + names.size();
    const std::size_t argumentSizesBytes = argumentSizes.size() * sizeof(std::uint32_t);
    const std::size_t metadataOffset = (argumentSizesOffset + argumentSizesBytes + 3U) & ~std::size_t(3U);
    std::string metadata;
    if (!argumentSizes.empty() || !parameters.empty()) {
        auto append = [&](std::uint16_t tag, const std::string& value) {
            const std::uint16_t length = static_cast<std::uint16_t>(value.size());
            metadata.append(reinterpret_cast<const char*>(&tag), sizeof(tag));
            metadata.append(reinterpret_cast<const char*>(&length), sizeof(length));
            metadata += value;
        };
        std::string summary(12, '\0');
        const std::uint32_t parameterCount = static_cast<std::uint32_t>(parameters.size());
        std::uint32_t argumentBytes = 0;
        for (const auto& parameter : parameters) {
            argumentBytes = std::max(argumentBytes, parameter.first + parameter.second);
        }
        std::memcpy(summary.data(), &parameterCount, sizeof(parameterCount));
        std::memcpy(summary.data() + sizeof(parameterCount), &argumentBytes, sizeof(argumentBytes));
        append(16, summary);

        for (std::uint32_t index = 0; index < parameterCount; ++index) {
            std::string parameter(36, '\0');
            const auto& source = parameters[index];
            std::memcpy(parameter.data() + 4, &index, sizeof(index));
            std::memcpy(parameter.data() + 8, &source.first, sizeof(source.first));
            std::memcpy(parameter.data() + 12, &source.second, sizeof(source.second));
            append(17, parameter);
        }
    }
    const std::size_t metadataEnd = metadataOffset + metadata.size();
    std::vector<char> image(metadataEnd, 0);

    Elf64_Ehdr header{};
    std::memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(Elf64_Ehdr);
    header.e_shoff = sectionHeadersOffset;
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = static_cast<Elf64_Half>(sectionCount);
    header.e_shstrndx = 1;
    std::memcpy(image.data(), &header, sizeof(header));

    Elf64_Shdr namesHeader{};
    namesHeader.sh_name = 1;
    namesHeader.sh_type = SHT_STRTAB;
    namesHeader.sh_offset = sectionNamesOffset;
    namesHeader.sh_size = names.size();
    std::memcpy(image.data() + sectionHeadersOffset + sizeof(Elf64_Shdr), &namesHeader, sizeof(namesHeader));

    if (!argumentSizes.empty()) {
        Elf64_Shdr argumentSizesHeader{};
        argumentSizesHeader.sh_name = 11;
        argumentSizesHeader.sh_type = SHT_NOTE;
        argumentSizesHeader.sh_offset = argumentSizesOffset;
        argumentSizesHeader.sh_size = argumentSizesBytes;
        std::memcpy(
            image.data() + sectionHeadersOffset + 2 * sizeof(Elf64_Shdr), &argumentSizesHeader,
            sizeof(argumentSizesHeader));
    }
    if (!metadata.empty()) {
        Elf64_Shdr metadataHeader{};
        metadataHeader.sh_name = static_cast<Elf64_Word>(names.find(".ascend.meta.test_kernel"));
        metadataHeader.sh_type = SHT_NOTE;
        metadataHeader.sh_offset = metadataOffset;
        metadataHeader.sh_size = metadata.size();
        metadataHeader.sh_addralign = 4;
        std::memcpy(
            image.data() + sectionHeadersOffset + 3 * sizeof(Elf64_Shdr), &metadataHeader, sizeof(metadataHeader));
    }

    std::memcpy(image.data() + sectionNamesOffset, names.data(), names.size());
    for (std::size_t index = 0; index < argumentSizes.size(); ++index) {
        std::memcpy(
            image.data() + argumentSizesOffset + index * sizeof(std::uint32_t), &argumentSizes[index],
            sizeof(std::uint32_t));
    }
    if (!metadata.empty()) {
        std::memcpy(image.data() + metadataOffset, metadata.data(), metadata.size());
    }
    return image;
}

} // namespace

int main()
{
    const std::vector<char> input = MakeElfWithKernelArgumentSizes({40, 31, 16}, {{0, 232}});
    const std::vector<char> aggregateInput = MakeElfWithKernelArgumentSizes({}, {{24, 24}, {0, 24}});
    const std::vector<char> missingArgumentMetadata = MakeElfWithKernelArgumentSizes({});
    std::vector<char> output;
    std::uint32_t traceOffset = 123;
    if (aclpti::profiling::InstrumentKernelEnd(
            missingArgumentMetadata.data(), missingArgumentMetadata.size(), output, &traceOffset) ||
        traceOffset != 0) {
        return 1;
    }
    // A failed linker invocation must not invalidate the extracted asset cache.
    if (aclpti::profiling::InstrumentKernelEnd(input.data(), input.size(), output)) {
        return 2;
    }
    std::atomic<bool> success{true};
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&] {
            std::vector<char> patched;
            if (!aclpti::profiling::InstrumentKernelEnd(input.data(), input.size(), patched) ||
                patched.size() <= input.size()) {
                success = false;
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    if (!aclpti::profiling::InstrumentKernelEnd(input.data(), input.size(), output, &traceOffset) ||
        traceOffset != 232) {
        return 3;
    }
    std::vector<char> aggregateOutput;
    if (!aclpti::profiling::InstrumentKernelEnd(
            aggregateInput.data(), aggregateInput.size(), aggregateOutput, &traceOffset) ||
        traceOffset != 48) {
        return 4;
    }
    return success ? 0 : 5;
}
