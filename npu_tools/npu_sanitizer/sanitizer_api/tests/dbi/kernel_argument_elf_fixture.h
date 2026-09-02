/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#pragma once

#include <elf.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace aclsan::test {

inline std::vector<uint8_t> MakeKernelArgumentSizeElf(uint32_t argumentSize)
{
    constexpr char kSectionNames[] = "\0.shstrtab\0__CCE_KernelArgSize";
    const std::string sectionNames(kSectionNames, sizeof(kSectionNames));
    const size_t sectionHeadersOffset = sizeof(Elf64_Ehdr);
    const size_t sectionNamesOffset = sectionHeadersOffset + 3 * sizeof(Elf64_Shdr);
    const size_t argumentSizeOffset = sectionNamesOffset + sectionNames.size();
    std::vector<uint8_t> image(argumentSizeOffset + sizeof(argumentSize), 0);

    Elf64_Ehdr header{};
    std::memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_shoff = sectionHeadersOffset;
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = 3;
    header.e_shstrndx = 1;
    std::memcpy(image.data(), &header, sizeof(header));

    Elf64_Shdr sectionNamesHeader{};
    sectionNamesHeader.sh_name = 1;
    sectionNamesHeader.sh_type = SHT_STRTAB;
    sectionNamesHeader.sh_offset = sectionNamesOffset;
    sectionNamesHeader.sh_size = sectionNames.size();
    std::memcpy(
        image.data() + sectionHeadersOffset + sizeof(Elf64_Shdr), &sectionNamesHeader, sizeof(sectionNamesHeader));

    Elf64_Shdr argumentSizeHeader{};
    argumentSizeHeader.sh_name = 11;
    argumentSizeHeader.sh_type = SHT_NOTE;
    argumentSizeHeader.sh_offset = argumentSizeOffset;
    argumentSizeHeader.sh_size = sizeof(argumentSize);
    std::memcpy(
        image.data() + sectionHeadersOffset + 2 * sizeof(Elf64_Shdr), &argumentSizeHeader, sizeof(argumentSizeHeader));
    std::memcpy(image.data() + sectionNamesOffset, sectionNames.data(), sectionNames.size());
    std::memcpy(image.data() + argumentSizeOffset, &argumentSize, sizeof(argumentSize));
    return image;
}

} // namespace aclsan::test
