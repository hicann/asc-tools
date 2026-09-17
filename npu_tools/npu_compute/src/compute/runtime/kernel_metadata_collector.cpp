/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "runtime/kernel_metadata_collector.h"

#include <limits>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <fstream>

namespace npucompute {
namespace {

template <typename Value>
bool ReadElfValue(std::ifstream& input, uint64_t offset, uint64_t fileSize, Value& value)
{
    if (offset > fileSize || sizeof(value) > fileSize - offset) {
        return false;
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    return static_cast<bool>(input.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

std::string ResolveSymbolName(const void* symbol)
{
    Dl_info info{};
    if (symbol == nullptr || ::dladdr(symbol, &info) == 0) {
        return {};
    }
    if (info.dli_saddr == symbol && info.dli_sname != nullptr) {
        return info.dli_sname;
    }
    if (info.dli_fname == nullptr) {
        return {};
    }
    std::ifstream input(info.dli_fname, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0) {
        return {};
    }
    const auto fileSize = static_cast<uint64_t>(input.tellg());
    Elf64_Ehdr header{};
    if (!ReadElfValue(input, 0, fileSize, header) || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_shentsize != sizeof(Elf64_Shdr) || (header.e_type != ET_DYN && header.e_type != ET_EXEC) ||
        header.e_shoff > fileSize || header.e_shnum > (fileSize - header.e_shoff) / sizeof(Elf64_Shdr)) {
        return {};
    }
    const uint64_t address = reinterpret_cast<uintptr_t>(symbol) -
                             (header.e_type == ET_DYN ? reinterpret_cast<uintptr_t>(info.dli_fbase) : 0);
    for (uint16_t sectionIndex = 0; sectionIndex < header.e_shnum; ++sectionIndex) {
        Elf64_Shdr section{};
        if (!ReadElfValue(input, header.e_shoff + sectionIndex * sizeof(section), fileSize, section) ||
            (section.sh_type != SHT_SYMTAB && section.sh_type != SHT_DYNSYM) ||
            section.sh_entsize != sizeof(Elf64_Sym) || section.sh_link >= header.e_shnum ||
            section.sh_offset > fileSize || section.sh_size > fileSize - section.sh_offset) {
            continue;
        }
        Elf64_Shdr strings{};
        if (!ReadElfValue(input, header.e_shoff + section.sh_link * sizeof(strings), fileSize, strings) ||
            strings.sh_type != SHT_STRTAB || strings.sh_offset > fileSize ||
            strings.sh_size > fileSize - strings.sh_offset) {
            continue;
        }
        for (uint64_t index = 0; index < section.sh_size / sizeof(Elf64_Sym); ++index) {
            Elf64_Sym entry{};
            if (!ReadElfValue(input, section.sh_offset + index * sizeof(entry), fileSize, entry) ||
                entry.st_value != address || ELF64_ST_TYPE(entry.st_info) != STT_FUNC || entry.st_shndx == SHN_UNDEF ||
                entry.st_name >= strings.sh_size) {
                continue;
            }
            input.seekg(static_cast<std::streamoff>(strings.sh_offset + entry.st_name));
            std::string name;
            for (uint64_t remaining = strings.sh_size - entry.st_name; remaining > 0; --remaining) {
                char character = 0;
                if (!input.get(character)) {
                    break;
                }
                if (character == '\0') {
                    return name;
                }
                name += character;
            }
        }
    }
    return {};
}

std::optional<uint64_t> GridSize(const dim3& grid)
{
    uint64_t size = grid.x;
    for (const uint64_t dimension : {static_cast<uint64_t>(grid.y), static_cast<uint64_t>(grid.z)}) {
        if (dimension != 0 && size > std::numeric_limits<uint64_t>::max() / dimension) {
            return std::nullopt;
        }
        size *= dimension;
    }
    return size;
}

} // namespace

void KernelMetadataCollector::OnCallback(aclptiCallbackId cbid, const aclptiCallbackData& data)
{
    if (data.functionParams == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (frozen_) {
        return;
    }
    if (cbid == ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction || cbid == ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol) {
        aclptiAclrtBinaryGetFunctionParams params{};
        std::string symbolName;
        if (cbid == ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction) {
            params = *static_cast<const aclptiAclrtBinaryGetFunctionParams*>(data.functionParams);
        } else {
            const auto& lookup = *static_cast<const aclptiAclrtGetFuncBySymbolParams*>(data.functionParams);
            params.funcHandle = lookup.funcHandle;
            if (data.callbackSite == ACLPTI_API_ENTER) {
                symbolName = ResolveSymbolName(lookup.symbol);
                params.kernelName = symbolName.empty() ? nullptr : symbolName.c_str();
            }
        }
        if (params.funcHandle == nullptr) {
            return;
        }
        if (data.callbackSite == ACLPTI_API_ENTER) {
            pendingNames_.erase(params.funcHandle);
            if (params.kernelName != nullptr) {
                pendingNames_[params.funcHandle] = {params.binHandle, params.kernelName};
            }
        } else if (data.callbackSite == ACLPTI_API_EXIT) {
            const auto pending = pendingNames_.find(params.funcHandle);
            if (pending != pendingNames_.end()) {
                if (data.retval == ACL_SUCCESS && *params.funcHandle != nullptr) {
                    names_[*params.funcHandle] = std::move(pending->second);
                }
                pendingNames_.erase(pending);
            }
        }
        return;
    }
    if (data.callbackSite == ACLPTI_API_EXIT && data.retval == ACL_SUCCESS) {
        if (cbid == ACLPTI_RUNTIME_CBID_aclrtBinaryUnLoad) {
            const auto& params = *static_cast<const aclptiAclrtBinaryUnLoadParams*>(data.functionParams);
            for (auto iterator = names_.begin(); iterator != names_.end();) {
                if (iterator->second.first == params.binHandle) {
                    iterator = names_.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }
        return;
    }
    if (data.callbackSite != ACLPTI_API_ENTER) {
        return;
    }
    aclrtFuncHandle handle = nullptr;
    KernelMetadata candidate;
    candidate.deviceId = 0;
    switch (cbid) {
        case ACLPTI_RUNTIME_CBID_aclrtLaunchKernel: {
            const auto& params = *static_cast<const aclptiAclrtLaunchKernelParams*>(data.functionParams);
            handle = params.funcHandle;
            candidate.blockDim = params.numBlocks;
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs: {
            const auto& params = *static_cast<const aclptiAclrtLaunchKernelWithHostArgsParams*>(data.functionParams);
            handle = params.funcHandle;
            candidate.blockDim = params.numBlocks;
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithArgsArray: {
            const auto& params = *static_cast<const aclptiAclrtLaunchKernelWithArgsArrayParams*>(data.functionParams);
            handle = static_cast<aclrtFuncHandle>(params.func);
            candidate.blockDim = params.numBlocks;
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs: {
            const auto& params =
                *static_cast<const aclptiAclrtLaunchSIMTKernelWithHostArgsParams*>(data.functionParams);
            handle = static_cast<aclrtFuncHandle>(params.func);
            candidate.blockDim = GridSize(params.gridDim);
            break;
        }
        case ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray: {
            const auto& params =
                *static_cast<const aclptiAclrtLaunchSIMTKernelWithArgsArrayParams*>(data.functionParams);
            handle = static_cast<aclrtFuncHandle>(params.func);
            candidate.blockDim = GridSize(params.gridDim);
            break;
        }
        default:
            return;
    }
    const auto found = names_.find(handle);
    if (found != names_.end()) {
        candidate.name = found->second.second;
    }
    metadata_ = std::move(candidate);
}

KernelMetadata KernelMetadataCollector::Snapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    frozen_ = true;
    return metadata_;
}

} // namespace npucompute
