/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef KERNEL_ELF_PARSER_H
#define KERNEL_ELF_PARSER_H

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <elf.h>

#include "stub_def.h"

namespace AscendC{

constexpr uint16_t FUNC_META_TYPE_KERNEL_TYPE = 1U;
constexpr uint16_t FUNC_META_TYPE_MIX_TASK_RATION = 3U;
const std::string KERNEL_SECTION_NAME_PREFIX = ".ascend.meta.";
const std::string KERNEL_MIX_AIV_POSTFIX = "_mix_aiv";
const std::string KERNEL_MIX_AIC_POSTFIX = "_mix_aic";

typedef struct {
    uint16_t type;
    uint16_t length;
} ElfTlvHead;

typedef enum KernelType : unsigned int {
    K_TYPE_INVALID = 0,
    K_TYPE_AICORE = 1,
    K_TYPE_AIC = 2,
    K_TYPE_AIV = 3,
    K_TYPE_MIX_AIC_MAIN = 4,
    K_TYPE_MIX_AIV_MAIN = 5,
    K_TYPE_AIC_ROLLBACK = 6,
    K_TYPE_AIV_ROLLBACK = 7,
    K_TYPE_MAX
} KernelTypeAsc;

struct ElfKernelInfo {
    uint32_t kernelType = 0;
    uint16_t aicRation = 0;
    uint16_t aivRation = 0;
};

class KernelModeRegister{
public:
    static KernelModeRegister& GetInstance()
    {
        static KernelModeRegister instance;
        return instance;
    }

    void Register(const std::string& kernelName, KernelMode kernelMode)
    {
        kernelModeMap[kernelName] = kernelMode;
    }

    void Clear()
    {
        kernelModeMap.clear();
    }

    KernelMode GetKenelMode(const char* kernelName)
    {
        std::string searchKernelName = KERNEL_SECTION_NAME_PREFIX + std::string(kernelName);
        auto it = kernelModeMap.find(searchKernelName + KERNEL_MIX_AIV_POSTFIX);
        if (it != kernelModeMap.end()) {
            return it->second;
        }
        it = kernelModeMap.find(searchKernelName + KERNEL_MIX_AIC_POSTFIX);
        if (it != kernelModeMap.end()) {
            return it->second;
        }
        it = kernelModeMap.find(searchKernelName);
        if (it != kernelModeMap.end()) {
            return it->second;
        }
        throw std::invalid_argument("Kernel mode not found for kernel: " + std::string(kernelName));
    }
private:
    std::unordered_map<std::string, KernelMode> kernelModeMap;
};

inline uint64_t ByteGetBigEndian(const uint8_t field[], const int32_t size)
{
    uint64_t ret = 0UL;

    switch (size) {
        case 1:
            ret = static_cast<uint64_t>(*field);
            break;
        case 2:
            ret = (static_cast<uint64_t>(field[1U])) | ((static_cast<uint64_t>(field[0U])) << 8U); // shift 8 bit
            break;
        case 3:
            ret = (static_cast<uint64_t>(field[2U])) | ((static_cast<uint64_t>(field[1U])) << 8U) |
                ((static_cast<uint64_t>(field[0U])) << 16U);
            break;
        case 4:
            ret = (static_cast<uint64_t>(field[3U])) | ((static_cast<uint64_t>(field[2U])) << 8U) |
                ((static_cast<uint64_t>(field[1U])) << 16U) | ((static_cast<uint64_t>(field[0U])) << 24U);
            break;
        case 5:
            ret = (static_cast<uint64_t>(field[4U])) | ((static_cast<uint64_t>(field[3U])) << 8U) |
                ((static_cast<uint64_t>(field[2U])) << 16U) | ((static_cast<uint64_t>(field[1U])) << 24U) |
                ((static_cast<uint64_t>(field[0U])) << 32U);
            break;
        case 6:
            ret = (static_cast<uint64_t>(field[5U])) | ((static_cast<uint64_t>(field[4U])) << 8U) |
                ((static_cast<uint64_t>(field[3U])) << 16U) | ((static_cast<uint64_t>(field[2U])) << 24U) |
                ((static_cast<uint64_t>(field[1U])) << 32U) | ((static_cast<uint64_t>(field[0U])) << 40U);
            break;
        case 7:
            ret = (static_cast<uint64_t>(field[6U])) | ((static_cast<uint64_t>(field[5U])) << 8U) |
                ((static_cast<uint64_t>(field[4U])) << 16U) | ((static_cast<uint64_t>(field[3U])) << 24U) |
                ((static_cast<uint64_t>(field[2U])) << 32U) | ((static_cast<uint64_t>(field[1U])) << 40U) |
                ((static_cast<uint64_t>(field[0U])) << 48U);
            break;
        case 8:
            ret = (static_cast<uint64_t>(field[7U])) | ((static_cast<uint64_t>(field[6U])) << 8U) |
                ((static_cast<uint64_t>(field[5U])) << 16U) | ((static_cast<uint64_t>(field[4U])) << 24U) |
                ((static_cast<uint64_t>(field[3U])) << 32U) | ((static_cast<uint64_t>(field[2U])) << 40U) |
                ((static_cast<uint64_t>(field[1U])) << 48U) | ((static_cast<uint64_t>(field[0U])) << 56U);
            break;
        default:
            throw std::invalid_argument("Invalid data length: size = " + std::to_string(size) + ", support 1~8 only");
            break;
    }

    return ret;
}

inline uint64_t ByteGetLittleEndian(const uint8_t field[], const int32_t size)
{
    uint64_t ret = 0UL;

    switch (size) {
        case 1:
            ret = static_cast<uint64_t>(*field);
            break;
        case 2:
            ret = (static_cast<uint64_t>(field[0U])) | ((static_cast<uint64_t>(field[1U])) << 8U); // shift 8 bit
            break;
        case 3:
            ret = (static_cast<uint64_t>(field[0U])) |
                ((static_cast<uint64_t>(field[1U])) << 8U) | ((static_cast<uint64_t>(field[2U])) << 16U);
            break;
        case 4:
            ret = (static_cast<uint64_t>(field[0U])) | ((static_cast<uint64_t>(field[1U])) << 8U) |
                ((static_cast<uint64_t>(field[2U])) << 16U) | ((static_cast<uint64_t>(field[3U])) << 24U);
            break;
        case 5:
            ret = (static_cast<uint64_t>(field[0U])) | ((static_cast<uint64_t>(field[1U])) << 8U) |
                ((static_cast<uint64_t>(field[2U])) << 16U) | ((static_cast<uint64_t>(field[3U])) << 24U) |
                ((static_cast<uint64_t>(field[4U])) << 32U);
            break;
        case 6: /* Fall through.  */
            ret = (static_cast<uint64_t>(field[0U])) | ((static_cast<uint64_t>(field[1U])) << 8U) |
                ((static_cast<uint64_t>(field[2U])) << 16U) | ((static_cast<uint64_t>(field[3U])) << 24U) |
                ((static_cast<uint64_t>(field[4U])) << 32U) | ((static_cast<uint64_t>(field[5U])) << 40U);
            break;
        case 7:
            ret = (static_cast<uint64_t>(field[0U])) | ((static_cast<uint64_t>(field[1U])) << 8U) |
                ((static_cast<uint64_t>(field[2U])) << 16U) | ((static_cast<uint64_t>(field[3U])) << 24U) |
                ((static_cast<uint64_t>(field[4U])) << 32U) | ((static_cast<uint64_t>(field[5U])) << 40U) |
                ((static_cast<uint64_t>(field[6U])) << 48U);
            break;
        case 8:
            ret = (static_cast<uint64_t>(field[0U])) | ((static_cast<uint64_t>(field[1U])) << 8U) |
                ((static_cast<uint64_t>(field[2U])) << 16U) | ((static_cast<uint64_t>(field[3U])) << 24U) |
                ((static_cast<uint64_t>(field[4U])) << 32U) | ((static_cast<uint64_t>(field[5U])) << 40U) |
                ((static_cast<uint64_t>(field[6U])) << 48U) | ((static_cast<uint64_t>(field[7U])) << 56U);
            break;
        default:
            throw std::invalid_argument("Invalid data length: size = " + std::to_string(size) + ", support 1~8 only");
            break;
    }

    return ret;
}

thread_local static uint64_t (*GetByte)(const uint8_t [], const int32_t) = nullptr;

inline Elf64_Ehdr ParseElfHeader(const uint8_t* const elfData, size_t dataSize)
{
    if (dataSize < sizeof(Elf64_Ehdr)) {
        throw std::invalid_argument("Input data size is too small for 64-bit ELF header, get input dataSize: " + std::to_string(dataSize) + 
            ", requires at least: " + std::to_string(sizeof(Elf64_Ehdr)));
    }

    /* Determine how to read the rest of the header.  */
    switch (elfData[EI_DATA]) {
        case ELFDATANONE:
        case ELFDATA2LSB:
            GetByte = &ByteGetLittleEndian;
            break;
        case ELFDATA2MSB:
            GetByte = &ByteGetBigEndian;
            break;
        default:
            GetByte = &ByteGetLittleEndian;
            break;
    }

    const bool is32bitElf = (elfData[EI_CLASS] != ELFCLASS64);

    /* Read in the rest of the header.  */
    if (is32bitElf) {
        throw std::invalid_argument("Only support input elf is 64-bit format.");
    }

    Elf64_Ehdr header;
    size_t offset = EI_NIDENT; // Skip e_ident

    header.e_type = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_machine = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_version = GetByte(elfData + offset, 4); offset += 4;
    header.e_entry = GetByte(elfData + offset, 8); offset += 8;
    header.e_phoff = GetByte(elfData + offset, 8); offset += 8;
    header.e_shoff = GetByte(elfData + offset, 8); offset += 8;
    header.e_flags = static_cast<uint32_t>(GetByte(elfData + offset, 4)); offset += 4;
    header.e_ehsize = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_phentsize = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_phnum = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_shentsize = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_shnum = static_cast<uint16_t>(GetByte(elfData + offset, 2)); offset += 2;
    header.e_shstrndx = static_cast<uint16_t>(GetByte(elfData + offset, 2));
    return header;
};

inline Elf64_Shdr GetSectionHeader(const uint8_t* const elfData, size_t dataSize, Elf64_Ehdr header, uint16_t index)
{
    if (index >= header.e_shnum) {
        throw std::invalid_argument("Invalid section index, get index: " + std::to_string(index) + 
            ", but section number is: " + std::to_string(header.e_shnum));
    }
    size_t shOffset = header.e_shoff + index * header.e_shentsize;    
    if (shOffset + sizeof(Elf64_Shdr) > dataSize) {
        throw std::invalid_argument("Data size is to small for parse section header[" + std::to_string(index) + "]");
    }
    const uint8_t* data = elfData + shOffset;
    Elf64_Shdr shdr;

    shdr.sh_name = static_cast<uint32_t>(GetByte(data, 4)); data += 4;
    shdr.sh_type = static_cast<uint32_t>(GetByte(data, 4)); data += 4;
    shdr.sh_flags = GetByte(data, 8); data += 8;
    shdr.sh_addr = GetByte(data, 8); data += 8;
    shdr.sh_offset = GetByte(data, 8); data += 8;
    shdr.sh_size = GetByte(data, 4); data += 8;
    shdr.sh_link = static_cast<uint32_t>(GetByte(data, 4)); data += 4;
    shdr.sh_info = static_cast<uint32_t>(GetByte(data, 4)); data += 4;
    shdr.sh_addralign = GetByte(data, 8); data += 8;
    shdr.sh_entsize = GetByte(data, 8);

    return shdr;
};

inline ElfKernelInfo GetKernelInfo(const uint8_t* const elfData, size_t dataSize, Elf64_Shdr kernelMetaSectionHead)
{
    uint64_t remainLen = kernelMetaSectionHead.sh_size;
    if (remainLen + kernelMetaSectionHead.sh_offset > dataSize) {
        throw std::invalid_argument("Data size is to small for parse kernel meta section");
    }
    const uint8_t* curData = elfData + kernelMetaSectionHead.sh_offset;
    ElfKernelInfo kernelInfo;
    while (remainLen > sizeof(ElfTlvHead)) {
        const ElfTlvHead *tlvHead = reinterpret_cast<const ElfTlvHead*>(curData);
        const uint16_t tlvType = static_cast<uint16_t>(GetByte(reinterpret_cast<const uint8_t *>(&(tlvHead->type)),
            sizeof(uint16_t)));
        const uint16_t tlvLength = static_cast<uint16_t>(GetByte(reinterpret_cast<const uint8_t *>(&(tlvHead->length)),
            sizeof(uint16_t)));
        if ((sizeof(ElfTlvHead) + tlvLength) > remainLen) {
            throw std::invalid_argument("Invalid TLV length in kernel meta section");
        }

        if (tlvType == FUNC_META_TYPE_KERNEL_TYPE) {
            if (tlvLength != sizeof(uint32_t)) {
                throw std::invalid_argument("Invalid kernel type length in kernel meta section");
            }
            kernelInfo.kernelType = static_cast<uint32_t>(GetByte(reinterpret_cast<const uint8_t *>(curData + sizeof(ElfTlvHead)), sizeof(uint32_t)));
        } else if (tlvType == FUNC_META_TYPE_MIX_TASK_RATION) {
            if (tlvLength != sizeof(uint16_t) * 2) {
                throw std::invalid_argument("Invalid mix task ration length in kernel meta section");            
            }
            kernelInfo.aicRation =
                static_cast<uint16_t>(GetByte(reinterpret_cast<const uint8_t *>(curData + sizeof(ElfTlvHead)), sizeof(uint16_t)));
            kernelInfo.aivRation =
                static_cast<uint16_t>(GetByte(reinterpret_cast<const uint8_t *>(curData + sizeof(ElfTlvHead) + sizeof(uint16_t)), sizeof(uint16_t)));
        }
        curData += sizeof(ElfTlvHead) + tlvLength;
        remainLen = remainLen - (sizeof(ElfTlvHead) + tlvLength);
    }
    return kernelInfo;
}

inline KernelMode ToKernelMode(ElfKernelInfo kernelInfo)
{
    if (kernelInfo.kernelType == K_TYPE_INVALID) {
        throw std::invalid_argument("get invalid kernel type");
    }
    if (kernelInfo.kernelType == K_TYPE_MIX_AIC_MAIN) {
        if (kernelInfo.aicRation == 1 && kernelInfo.aivRation == 0) {
            return KernelMode::AIC_MODE;
        }
        else if (kernelInfo.aicRation == 1 && kernelInfo.aivRation == 1) {
            return KernelMode::MIX_AIC_1_1; // MIX_AIC_1_1
        }
        else if (kernelInfo.aicRation == 1 && kernelInfo.aivRation == 2) { // aic num 1, aiv num 2
            return KernelMode::MIX_MODE; // MIX_MODE
        }
    } else if (kernelInfo.kernelType == K_TYPE_AIC || kernelInfo.kernelType == K_TYPE_AIC_ROLLBACK) {
        return KernelMode::AIC_MODE; // => AIC_MODE
    } else if (kernelInfo.kernelType == K_TYPE_AIV || kernelInfo.kernelType == K_TYPE_AIV_ROLLBACK || kernelInfo.kernelType == K_TYPE_MIX_AIV_MAIN) {
        return KernelMode::AIV_MODE; // AIV_MODE
    }
    return KernelMode::MIX_MODE;
}

inline void ParseKernelSections(const uint8_t* const elfData, size_t dataSize, Elf64_Ehdr header,
    Elf64_Shdr shStrTabHdr)
{
    const uint8_t* shStrTab = elfData + shStrTabHdr.sh_offset;
    if (shStrTabHdr.sh_offset + shStrTabHdr.sh_size > dataSize) {
        throw std::invalid_argument("Data size is to small for parse section header string table");
    }

    for (int i = 0; i < header.e_shnum; ++i) {
        Elf64_Shdr shdr = GetSectionHeader(elfData, dataSize, header, i);
        std::string sectionName(reinterpret_cast<const char*>(shStrTab) + shdr.sh_name);
        if ( sectionName.length() > KERNEL_SECTION_NAME_PREFIX.length() &&
            sectionName.compare(0, KERNEL_SECTION_NAME_PREFIX.length(), KERNEL_SECTION_NAME_PREFIX) == 0) {
                try{
                    ElfKernelInfo kernelInfo = GetKernelInfo(elfData, dataSize, shdr); 
                    KernelMode kernelMode = ToKernelMode(kernelInfo);
                    KernelModeRegister::GetInstance().Register(sectionName, kernelMode);
                } catch (std::invalid_argument &e) {
                    throw std::invalid_argument("Failed to get kernel mode from section " + sectionName + ": " + e.what());
                }
        }
    }
}

inline void RegisterKernelElf(const uint8_t* const elfData, size_t dataSize)
{
    Elf64_Ehdr header = ParseElfHeader(elfData, dataSize);
    Elf64_Shdr shStrTabHdr = GetSectionHeader(elfData, dataSize, header, header.e_shstrndx);
    ParseKernelSections(elfData, dataSize, header, shStrTabHdr);
}

}
#endif // KERNEL_ELF_PARSER_H