// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi/kernel_param_metadata.h"

#include <cassert>
#include <cstring>
#include <elf.h>
#include <iostream>
#include <limits>
#include <vector>

namespace {
template <class T>
void Put(std::string& data, size_t offset, T value)
{
    assert(offset + sizeof(value) <= data.size());
    std::memcpy(&data[offset], &value, sizeof(value));
}

template <class T>
T Get(const std::string& data, size_t offset)
{
    T value{};
    assert(offset + sizeof(value) <= data.size());
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

std::string Metadata(uint32_t count, uint32_t lastOffset)
{
    std::string data(16 + 40 * count, '\0');
    Put<uint16_t>(data, 0, 16);
    Put<uint16_t>(data, 2, 12);
    Put<uint32_t>(data, 4, count);
    Put<uint32_t>(data, 8, count == 0 ? 0 : lastOffset + 8);
    for (uint32_t i = 0; i < count; ++i) {
        size_t pos = 16 + i * 40;
        Put<uint16_t>(data, pos, 17);
        Put<uint16_t>(data, pos + 2, 36);
        Put<uint32_t>(data, pos + 8, i);
        Put<uint32_t>(data, pos + 12, i + 1 == count ? lastOffset : i * 8);
        Put<uint32_t>(data, pos + 16, 8);
        Put<uint16_t>(data, pos + 20, 8);
        Put<uint16_t>(data, pos + 22, 2);
    }
    // 未知 TLV 必须原样保留。
    data += std::string("\x63\x00\x03\x00xyz", 7);
    return data;
}

std::string Elf(const std::vector<std::string>& metadata)
{
    const size_t count = metadata.size() + 3;
    std::string data(sizeof(Elf64_Ehdr) + count * sizeof(Elf64_Shdr), '\0');
    Elf64_Ehdr header{};
    std::memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(header);
    header.e_shoff = sizeof(header);
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = count;
    header.e_shstrndx = 1;
    Put(data, 0, header);
    std::string names(1, '\0');
    const auto append = [&](size_t index, const std::string& name, const std::string& content, uint64_t flags) {
        Elf64_Shdr section{};
        section.sh_name = names.size();
        names += name + '\0';
        section.sh_type = index >= 3 ? SHT_NOTE : SHT_PROGBITS;
        section.sh_flags = flags;
        section.sh_offset = data.size();
        section.sh_size = content.size();
        section.sh_addralign = 1;
        data += content;
        Put(data, header.e_shoff + index * sizeof(section), section);
    };
    append(2, ".text", "executable-bytes", SHF_ALLOC | SHF_EXECINSTR);
    for (size_t i = 0; i < metadata.size(); ++i) {
        append(i + 3, ".ascend.meta.kernel" + std::to_string(i), metadata[i], 0);
    }
    Elf64_Shdr strings{};
    strings.sh_type = SHT_STRTAB;
    strings.sh_offset = data.size();
    strings.sh_size = names.size();
    data += names;
    Put(data, header.e_shoff + sizeof(strings), strings);
    return data;
}

void CheckRejected(const std::string& original, const std::string& input, uint32_t offset)
{
    std::string output = input;
    std::string diagnostic;
    assert(!aclsan::ModifyKernelParamMetadata(original, output, offset, diagnostic));
    assert(!diagnostic.empty());
    assert(output == input);
}

void CheckMixedAndIdempotent()
{
    const auto original = Elf({Metadata(0, 0), Metadata(1, 0), Metadata(3, 16)});
    auto patched = Elf({Metadata(0, 0), Metadata(2, 8), Metadata(4, 24)});
    const auto before = patched;
    std::string diagnostic;
    assert(aclsan::ModifyKernelParamMetadata(original, patched, 24, diagnostic));
    const uint32_t counts[] = {1, 2, 4};
    for (size_t i = 0; i < 3; ++i) {
        const auto section = Get<Elf64_Shdr>(patched, 64 + (i + 3) * 64);
        const size_t pos = section.sh_offset;
        assert(patched.substr(pos + section.sh_size - 7, 7) == std::string("\x63\x00\x03\x00xyz", 7));
        assert(Get<uint32_t>(patched, pos + 4) == counts[i]);
        assert(Get<uint32_t>(patched, pos + 8) == 32);
        const size_t pointer = pos + 16 + (counts[i] - 1) * 40;
        assert(Get<uint32_t>(patched, pointer + 12) == 24);
        assert(Get<uint32_t>(patched, pointer + 16) == 8);
        assert(Get<uint16_t>(patched, pointer + 22) == 2);
    }
    auto text = Get<Elf64_Shdr>(before, 64 + 2 * 64);
    assert(before.substr(text.sh_offset, text.sh_size) == patched.substr(text.sh_offset, text.sh_size));
    const auto normalized = patched;
    assert(aclsan::ModifyKernelParamMetadata(original, patched, 24, diagnostic));
    assert(patched == normalized);
    CheckRejected(original, before, 8);
    CheckRejected(original, before, std::numeric_limits<uint32_t>::max());
    CheckRejected(original, before.substr(0, 63), 24);
    auto invalid = before;
    Put<uint64_t>(invalid, 40, std::numeric_limits<uint64_t>::max());
    CheckRejected(original, invalid, 24);
    invalid = before;
    text = Get<Elf64_Shdr>(invalid, 64 + 3 * 64);
    Put<uint16_t>(invalid, text.sh_offset + 2, 65535);
    CheckRejected(original, invalid, 24);
    text.sh_flags = SHF_ALLOC;
    invalid = before;
    Put(invalid, 64 + 3 * 64, text);
    CheckRejected(original, invalid, 24);
    CheckRejected(original, Elf({Metadata(0, 0)}), 24);
    auto metadata = Metadata(1, 0);
    Put<uint32_t>(metadata, 4, 2);
    CheckRejected(Elf({metadata}), before, 24);
    metadata = Metadata(2, 0);
    CheckRejected(Elf({metadata}), before, 24);
    metadata = Metadata(0, 0);
    metadata += metadata.substr(0, 16);
    CheckRejected(Elf({metadata}), before, 24);
    invalid = before;
    invalid[EI_DATA] = ELFDATA2MSB;
    CheckRejected(original, invalid, 24);
    invalid = before;
    const auto first = Get<Elf64_Shdr>(invalid, 64 + 3 * 64);
    auto second = Get<Elf64_Shdr>(invalid, 64 + 4 * 64);
    second.sh_name = first.sh_name;
    Put(invalid, 64 + 4 * 64, second);
    CheckRejected(original, invalid, 24);
}
} // namespace

int main()
{
    CheckMixedAndIdempotent();
    const auto original = Elf({Metadata(0, 0)});
    auto patched = original;
    std::string diagnostic;
    assert(aclsan::ModifyKernelParamMetadata(original, patched, 8, diagnostic));
    const auto section = Get<Elf64_Shdr>(patched, 64 + 3 * 64);
    assert(Get<uint32_t>(patched, section.sh_offset + 4) == 1);
    assert(Get<uint32_t>(patched, section.sh_offset + 28) == 8);
    std::cout << "kernel parameter metadata tests passed\n";
}
