// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi/kernel_param_metadata.h"

#include <algorithm>
#include <elf.h>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace aclsan {
namespace {
constexpr uint16_t PARAM_SUMMARY = 16;
constexpr uint16_t PARAM_INFO = 17;
constexpr size_t SUMMARY_SIZE = 12;
constexpr size_t PARAM_INFO_SIZE = 36;
constexpr uint32_t POINTER_SIZE = 8;

// TODO: 后续统一整理成一个公共函数
void Require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool InRange(size_t size, uint64_t offset, uint64_t length) { return offset <= size && length <= size - offset; }

uint64_t Read(const std::string& data, size_t offset, size_t size)
{
    Require(InRange(data.size(), offset, size), "truncated ELF or parameter metadata");
    uint64_t result = 0;
    for (size_t i = 0; i < size; ++i) {
        result |= static_cast<uint64_t>(static_cast<unsigned char>(data[offset + i])) << (i * 8);
    }
    return result;
}

void Write(std::string& data, size_t offset, uint64_t value, size_t size)
{
    Require(InRange(data.size(), offset, size), "invalid metadata write range");
    for (size_t i = 0; i < size; ++i) {
        data[offset + i] = static_cast<char>((value >> (i * 8)) & 0xffU);
    }
}

struct Section {
    size_t header;
    uint64_t offset;
    uint64_t size;
    uint64_t alignment;
    uint64_t flags;
};

std::map<std::string, Section> ReadSections(const std::string& data)
{
    Require(
        data.size() >= sizeof(Elf64_Ehdr) && data.compare(0, SELFMAG, ELFMAG) == 0 &&
            Read(data, EI_CLASS, 1) == ELFCLASS64 && Read(data, EI_DATA, 1) == ELFDATA2LSB &&
            Read(data, EI_VERSION, 1) == EV_CURRENT && Read(data, 20, 4) == EV_CURRENT &&
            Read(data, 52, 2) == sizeof(Elf64_Ehdr),
        "expected ELF64 little-endian version 1");
    const uint64_t table = Read(data, 40, 8);
    const uint64_t count = Read(data, 60, 2);
    const uint64_t namesIndex = Read(data, 62, 2);
    Require(
        Read(data, 58, 2) == sizeof(Elf64_Shdr) && count > 0 && namesIndex > 0 && namesIndex < count &&
            table >= sizeof(Elf64_Ehdr) && InRange(data.size(), table, count * sizeof(Elf64_Shdr)),
        "invalid or unsupported ELF section table");
    const size_t namesHeader = table + namesIndex * sizeof(Elf64_Shdr);
    const uint64_t namesOffset = Read(data, namesHeader + 24, 8);
    const uint64_t namesSize = Read(data, namesHeader + 32, 8);
    Require(
        Read(data, namesHeader + 4, 4) == SHT_STRTAB && InRange(data.size(), namesOffset, namesSize),
        "invalid ELF section names");
    std::map<std::string, Section> sections;
    for (size_t i = 1; i < count; ++i) {
        const size_t header = table + i * sizeof(Elf64_Shdr);
        const uint64_t nameOffset = Read(data, header, 4);
        const uint64_t type = Read(data, header + 4, 4);
        Section section{
            header, Read(data, header + 24, 8), Read(data, header + 32, 8), Read(data, header + 48, 8),
            Read(data, header + 8, 8)};
        Require(
            type == SHT_NOBITS || InRange(data.size(), section.offset, section.size),
            "ELF section exceeds file bounds");
        Require(nameOffset < namesSize, "invalid ELF section name offset");
        const size_t end = data.find('\0', namesOffset + nameOffset);
        Require(end != std::string::npos && end < namesOffset + namesSize, "unterminated ELF section name");
        if ((section.flags & SHF_ALLOC) != 0 && type != SHT_NOBITS && section.size != 0) {
            Require(
                section.offset >= table + count * sizeof(Elf64_Shdr) || section.offset + section.size <= table,
                "allocated ELF section overlaps section table");
        }
        const std::string name = data.substr(namesOffset + nameOffset, end - namesOffset - nameOffset);
        if (name.compare(0, 13, ".ascend.meta.") != 0) {
            continue;
        }
        Require(
            type == SHT_NOTE && (section.flags & SHF_ALLOC) == 0,
            "kernel metadata must be a non-allocated NOTE section");
        Require(
            section.alignment == 0 ||
                ((section.alignment & (section.alignment - 1)) == 0 && section.alignment <= data.size()),
            "invalid metadata alignment");
        Require(sections.emplace(name, section).second, "duplicate kernel metadata section");
    }
    return sections;
}

struct Tlv {
    uint16_t tag;
    std::string value;
};

std::vector<Tlv> ReadMetadata(const std::string& data, const Section& section)
{
    const std::string bytes = data.substr(section.offset, section.size);
    std::vector<Tlv> values;
    size_t summaryCount = 0;
    size_t infoCount = 0;
    uint32_t count = 0;
    for (size_t offset = 0; offset < bytes.size();) {
        const auto tag = static_cast<uint16_t>(Read(bytes, offset, 2));
        const size_t length = Read(bytes, offset + 2, 2);
        offset += 4;
        Require(InRange(bytes.size(), offset, length), "truncated parameter TLV");
        if (tag == PARAM_SUMMARY) {
            Require(length == SUMMARY_SIZE, "unsupported ParamSummary layout");
            ++summaryCount;
            count = Read(bytes, offset, 4);
        } else if (tag == PARAM_INFO) {
            Require(length == PARAM_INFO_SIZE, "unsupported ParamInfo layout");
            Require(Read(bytes, offset + 4, 4) == infoCount, "non-sequential parameter index");
            ++infoCount;
        }
        values.push_back({tag, bytes.substr(offset, length)});
        offset += length;
    }
    Require(summaryCount == 1 && infoCount == count, "inconsistent parameter summary and descriptions");
    return values;
}

void AppendTlv(std::string& output, uint16_t tag, const std::string& value)
{
    const size_t header = output.size();
    output.resize(header + 4);
    Write(output, header, tag, 2);
    Write(output, header + 2, value.size(), 2);
    output += value;
}

std::string NormalizeMetadata(const std::vector<Tlv>& original, const std::vector<Tlv>& patched, uint32_t traceOffset)
{
    std::vector<std::string> parameters;
    uint64_t end = 0;
    for (const auto& item : original) {
        if (item.tag == PARAM_SUMMARY) {
            Require(Read(item.value, 4, 4) <= traceOffset, "original argument area exceeds trace offset");
        }
        if (item.tag != PARAM_INFO) {
            continue;
        }
        const uint64_t offset = Read(item.value, 8, 4);
        const uint64_t size = Read(item.value, 12, 4);
        Require(
            size > 0 && offset >= end && offset <= traceOffset && size <= traceOffset - offset,
            "original parameter overlaps trace pointer or another parameter");
        end = offset + size;
        parameters.push_back(item.value);
    }
    // 与 dav-3510 bisheng-tune 的 synthetic pointer ParamInfo 一致：8 字节、8 字节对齐、类型 2。
    std::string pointer(PARAM_INFO_SIZE, '\0');
    Write(pointer, 4, parameters.size(), 4);
    Write(pointer, 8, traceOffset, 4);
    Write(pointer, 12, POINTER_SIZE, 4);
    Write(pointer, 16, POINTER_SIZE, 2);
    Write(pointer, 18, 2, 2);
    std::string output;
    for (const auto& item : patched) {
        if (item.tag == PARAM_INFO) {
            continue;
        }
        if (item.tag != PARAM_SUMMARY) {
            AppendTlv(output, item.tag, item.value);
            continue;
        }
        const uint64_t count = Read(item.value, 0, 4);
        Require(
            count == parameters.size() || count == parameters.size() + 1, "unexpected instrumented parameter count");
        std::string summary = item.value;
        Write(summary, 0, parameters.size() + 1, 4);
        Write(summary, 4, traceOffset + POINTER_SIZE, 4);
        AppendTlv(output, PARAM_SUMMARY, summary);
        for (const auto& parameter : parameters) {
            AppendTlv(output, PARAM_INFO, parameter);
        }
        AppendTlv(output, PARAM_INFO, pointer);
    }
    return output;
}
} // namespace

bool ModifyKernelParamMetadata(
    const std::string& original, std::string& patched, uint32_t traceOffset, std::string& diagnostic)
{
    try {
        Require(
            traceOffset >= POINTER_SIZE && traceOffset % POINTER_SIZE == 0 &&
                traceOffset <= std::numeric_limits<uint32_t>::max() - POINTER_SIZE,
            "invalid trace pointer offset");
        const auto sources = ReadSections(original);
        const auto destinations = ReadSections(patched);
        Require(!sources.empty(), "no kernel parameter metadata found");
        std::string result = patched;
        for (const auto& entry : sources) {
            const auto destination = destinations.find(entry.first);
            Require(destination != destinations.end(), "instrumented kernel metadata section is missing");
            const Section& section = destination->second;
            const auto metadata =
                NormalizeMetadata(ReadMetadata(original, entry.second), ReadMetadata(patched, section), traceOffset);
            if (metadata == patched.substr(section.offset, section.size)) {
                continue;
            }
            // 追加非加载元数据，只更新对应节头；不移动任何原始代码、数据或 program header。
            const size_t alignment = std::max<uint64_t>(4, section.alignment);
            const size_t padding = (alignment - result.size() % alignment) % alignment;
            Require(
                padding <= result.max_size() - result.size() &&
                    metadata.size() <= result.max_size() - result.size() - padding,
                "metadata output is too large");
            result.append(padding, '\0');
            const size_t offset = result.size();
            result += metadata;
            Write(result, section.header + 24, offset, 8);
            Write(result, section.header + 32, metadata.size(), 8);
        }
        patched.swap(result);
        diagnostic.clear();
        return true;
    } catch (const std::exception& error) {
        diagnostic = error.what();
        return false;
    }
}
} // namespace aclsan
