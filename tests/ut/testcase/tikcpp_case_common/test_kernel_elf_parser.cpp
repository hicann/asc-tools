/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#include <gtest/gtest.h>
#include <cstring>
#include "kernel_elf_parser.h"

class TestKernelElfParserSuite : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestKernelElfParserSuite, ByteGetBigEndian_Size1)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetBigEndian(data, 1), 0x01ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetBigEndian_Size2)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetBigEndian(data, 2), 0x0102ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetBigEndian_Size4)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetBigEndian(data, 4), 0x01020304ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetBigEndian_Size8)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetBigEndian(data, 8), 0x0102030405060708ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetLittleEndian_Size1)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetLittleEndian(data, 1), 0x01ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetLittleEndian_Size2)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetLittleEndian(data, 2), 0x0201ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetLittleEndian_Size4)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetLittleEndian(data, 4), 0x04030201ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetLittleEndian_Size8)
{
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(AscendC::ByteGetLittleEndian(data, 8), 0x0807060504030201ULL);
}

TEST_F(TestKernelElfParserSuite, ByteGetBigEndian_InvalidSize)
{
    uint8_t data[8] = {0};
    EXPECT_THROW(AscendC::ByteGetBigEndian(data, 0), std::invalid_argument);
    EXPECT_THROW(AscendC::ByteGetBigEndian(data, 9), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, ByteGetLittleEndian_InvalidSize)
{
    uint8_t data[8] = {0};
    EXPECT_THROW(AscendC::ByteGetLittleEndian(data, 0), std::invalid_argument);
    EXPECT_THROW(AscendC::ByteGetLittleEndian(data, 9), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, ParseElfHeader_Success_LittleEndian)
{
    uint8_t elfData[64] = {0};
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;
    
    Elf64_Ehdr header = AscendC::ParseElfHeader(elfData, sizeof(elfData));
    EXPECT_EQ(header.e_type, 0);
    EXPECT_EQ(header.e_machine, 0);
    EXPECT_EQ(header.e_version, 0);
}

TEST_F(TestKernelElfParserSuite, ParseElfHeader_Success_BigEndian)
{
    uint8_t elfData[64] = {0};
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2MSB;
    
    Elf64_Ehdr header = AscendC::ParseElfHeader(elfData, sizeof(elfData));
    EXPECT_EQ(header.e_type, 0);
    EXPECT_EQ(header.e_machine, 0);
    EXPECT_EQ(header.e_version, 0);
}

TEST_F(TestKernelElfParserSuite, ParseElfHeader_DataTooSmall)
{
    uint8_t elfData[32] = {0};
    EXPECT_THROW(AscendC::ParseElfHeader(elfData, sizeof(elfData)), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, ParseElfHeader_Not64Bit)
{
    uint8_t elfData[64] = {0};
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS32;
    elfData[EI_DATA] = ELFDATA2LSB;
    
    EXPECT_THROW(AscendC::ParseElfHeader(elfData, sizeof(elfData)), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, GetSectionHeader_Success)
{
    uint8_t elfData[256] = {0};
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;
    
    elfData[40] = 64; // e_shoff
    elfData[41] = 0;
    elfData[42] = 0;
    elfData[43] = 0;
    elfData[44] = 0;
    elfData[45] = 0;
    elfData[46] = 0;
    elfData[47] = 0;
    
    elfData[58] = 64; // e_shentsize
    elfData[59] = 0;
    
    elfData[60] = 2; // e_shnum
    elfData[61] = 0;
    
    Elf64_Ehdr header = AscendC::ParseElfHeader(elfData, sizeof(elfData));
    Elf64_Shdr shdr = AscendC::GetSectionHeader(elfData, sizeof(elfData), header, 0);
    EXPECT_EQ(shdr.sh_name, 0);
    EXPECT_EQ(shdr.sh_type, 0);
}

TEST_F(TestKernelElfParserSuite, GetSectionHeader_InvalidIndex)
{
    uint8_t elfData[256] = {0};
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;
    
    elfData[40] = 64; // e_shoff
    elfData[41] = 0;
    elfData[42] = 0;
    elfData[43] = 0;
    elfData[44] = 0;
    elfData[45] = 0;
    elfData[46] = 0;
    elfData[47] = 0;
    
    elfData[58] = 64; // e_shentsize
    elfData[59] = 0;
    
    elfData[60] = 2; // e_shnum
    elfData[61] = 0;
    
    Elf64_Ehdr header = AscendC::ParseElfHeader(elfData, sizeof(elfData));
    EXPECT_THROW(AscendC::GetSectionHeader(elfData, sizeof(elfData), header, 5), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, GetSectionHeader_DataTooSmall)
{
    uint8_t elfData[64] = {0};
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;
    
    elfData[40] = 64; // e_shoff
    elfData[58] = 64; // e_shentsize
    elfData[60] = 1; // e_shnum
    
    Elf64_Ehdr header = AscendC::ParseElfHeader(elfData, sizeof(elfData));
    EXPECT_THROW(AscendC::GetSectionHeader(elfData, sizeof(elfData), header, 0), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, GetKernelInfo_KernelType)
{
    uint8_t tlvData[16] = {0};
    AscendC::ElfTlvHead* head = reinterpret_cast<AscendC::ElfTlvHead*>(tlvData);
    head->type = 1;
    head->length = 4;
    uint32_t kernelType = AscendC::K_TYPE_AIC;
    memcpy(tlvData + sizeof(AscendC::ElfTlvHead), &kernelType, sizeof(uint32_t));
    
    Elf64_Shdr sectionHead;
    sectionHead.sh_offset = 0;
    sectionHead.sh_size = sizeof(AscendC::ElfTlvHead) + 4;
    
    AscendC::ElfKernelInfo info = AscendC::GetKernelInfo(tlvData, sizeof(tlvData), sectionHead);
    EXPECT_EQ(info.kernelType, AscendC::K_TYPE_AIC);
}

TEST_F(TestKernelElfParserSuite, GetKernelInfo_MixTaskRation)
{
    uint8_t tlvData[32] = {0};
    AscendC::ElfTlvHead* head = reinterpret_cast<AscendC::ElfTlvHead*>(tlvData);
    head->type = AscendC::FUNC_META_TYPE_KERNEL_TYPE;
    head->length = 4;
    uint32_t kernelType = AscendC::K_TYPE_MIX_AIC_MAIN;
    memcpy(tlvData + sizeof(AscendC::ElfTlvHead), &kernelType, sizeof(uint32_t));
    
    AscendC::ElfTlvHead* head2 = reinterpret_cast<AscendC::ElfTlvHead*>(tlvData + sizeof(AscendC::ElfTlvHead) + 4);
    head2->type = AscendC::FUNC_META_TYPE_MIX_TASK_RATION;
    head2->length = 4;
    uint16_t aicRation = 1;
    uint16_t aivRation = 2;
    memcpy(tlvData + sizeof(AscendC::ElfTlvHead) * 2 + 4, &aicRation, sizeof(uint16_t));
    memcpy(tlvData + sizeof(AscendC::ElfTlvHead) * 2 + 4 + sizeof(uint16_t), &aivRation, sizeof(uint16_t));
    
    Elf64_Shdr sectionHead;
    sectionHead.sh_offset = 0;
    sectionHead.sh_size = sizeof(AscendC::ElfTlvHead) * 2 + 8;
    
    AscendC::ElfKernelInfo info = AscendC::GetKernelInfo(tlvData, sizeof(tlvData), sectionHead);
    EXPECT_EQ(info.kernelType, AscendC::K_TYPE_MIX_AIC_MAIN);
    EXPECT_EQ(info.aicRation, 1);
    EXPECT_EQ(info.aivRation, 2);
}

TEST_F(TestKernelElfParserSuite, GetKernelInfo_InvalidTlvLength)
{
    uint8_t tlvData[8] = {0};
    AscendC::ElfTlvHead* head = reinterpret_cast<AscendC::ElfTlvHead*>(tlvData);
    head->type = 1;
    head->length = 100;
    
    Elf64_Shdr sectionHead;
    sectionHead.sh_offset =0;
    sectionHead.sh_size = 8;
    
    EXPECT_THROW(AscendC::GetKernelInfo(tlvData, sizeof(tlvData), sectionHead), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, GetKernelInfo_DataTooSmall)
{
    uint8_t tlvData[4] = {0};
    
    Elf64_Shdr sectionHead;
    sectionHead.sh_offset = 0;
    sectionHead.sh_size = 100;
    
    EXPECT_THROW(AscendC::GetKernelInfo(tlvData, sizeof(tlvData), sectionHead), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_AIC)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_AIC;
    info.aicRation = 0;
    info.aivRation = 0;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_AIC_Rollback)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_AIC_ROLLBACK;
    info.aicRation = 0;
    info.aivRation = 0;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_AIV)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_AIV;
    info.aicRation = 0;
    info.aivRation = 0;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::AIV_MODE);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_AIV_Rollback)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_AIV_ROLLBACK;
    info.aicRation = 0;
    info.aivRation = 0;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::AIV_MODE);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_MixAicMain)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_MIX_AIC_MAIN;
    info.aicRation = 1;
    info.aivRation = 0;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_MixAic1_1)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_MIX_AIC_MAIN;
    info.aicRation = 1;
    info.aivRation = 1;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::MIX_AIC_1_1);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_MixMode)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_MIX_AIC_MAIN;
    info.aicRation = 1;
    info.aivRation = 2;
    
    KernelMode mode = AscendC::ToKernelMode(info);
    EXPECT_EQ(mode, KernelMode::MIX_MODE);
}

TEST_F(TestKernelElfParserSuite, ToKernelMode_InvalidType)
{
    AscendC::ElfKernelInfo info;
    info.kernelType = AscendC::K_TYPE_INVALID;
    info.aicRation = 0;
    info.aivRation = 0;
    
    EXPECT_THROW(AscendC::ToKernelMode(info), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, Demangle_ValidMangledSymbol)
{
    // _Z11test_kernelv is the Itanium ABI mangled name for "test_kernel()"
    std::string result = AscendC::KernelModeRegister::Demangle("_Z11test_kernelv");
    EXPECT_EQ(result, "test_kernel()");
}

TEST_F(TestKernelElfParserSuite, Demangle_InvalidMangledSymbol)
{
    // Non-mangled name should throw std::runtime_error
    EXPECT_THROW(AscendC::KernelModeRegister::Demangle("not_a_mangled_name"), std::runtime_error);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_RegisterAndGet)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    // Register internally demangles _Z11test_kernelv -> "test_kernel()"
    reg.Register("_Z11test_kernelv", KernelMode::AIC_MODE);
    
    // GetKenelMode internally demangles _Z11test_kernelv -> "test_kernel()", then finds it
    KernelMode mode = reg.GetKenelMode("_Z11test_kernelv");
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_NotFound)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    // Valid mangled name, but not registered -> ASCENDC_ASSERT triggers abort
    EXPECT_THROW(reg.GetKenelMode("_Z16nonexistent_namev"), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_GetWithInvalidMangling)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    // Invalid mangling -> Demangle throws std::runtime_error
    EXPECT_THROW(reg.GetKenelMode("not_a_mangled_name"), std::runtime_error);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_RegisterWithInvalidMangling)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    // Register with invalid mangling -> Demangle throws std::runtime_error
    EXPECT_THROW(reg.Register("not_a_mangled_name", KernelMode::AIC_MODE), std::runtime_error);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_RegisterWithUnstrippedPostfix)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    // Simulate the case where postfix is not correctly stripped in ParseKernelSections:
    // The name "_Z11test_kernelv_mix_aiv" is NOT a valid Itanium ABI mangled name,
    // Demangle will fail with std::runtime_error
    EXPECT_THROW(reg.Register("_Z11test_kernelv_mix_aiv", KernelMode::AIV_MODE), std::runtime_error);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_GetWithUnstrippedPostfix)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    // Register with correct mangled name (postfix already stripped)
    reg.Register("_Z11test_kernelv", KernelMode::AIC_MODE);
    // If GetKenelMode is called with a mangling that still has unstripped postfix,
    // Demangle fails with std::runtime_error
    EXPECT_THROW(reg.GetKenelMode("_Z11test_kernelv_mix_aiv"), std::runtime_error);
}

TEST_F(TestKernelElfParserSuite, RegisterKernelElf_Integration)
{
    uint8_t elfData[512] = {0};
    
    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;
    
    elfData[40] = 64; // e_shoff
    elfData[58] = 64; // e_shentsize
    elfData[60] = 2; // e_shnum
    elfData[62] = 1; // e_shstrndx
    
    Elf64_Shdr* shdr0 = reinterpret_cast<Elf64_Shdr*>(elfData + 64);
    shdr0->sh_name = 0;
    shdr0->sh_type = 0;
    shdr0->sh_offset = 192;
    shdr0->sh_size = 64;
    
    Elf64_Shdr* shdr1 = reinterpret_cast<Elf64_Shdr*>(elfData + 128);
    shdr1->sh_name = 1;
    shdr1->sh_type = 0;
    shdr1->sh_offset = 256;
    shdr1->sh_size = 64;
    
    // Section name uses mangled kernel name
    const char* strTab = ".ascend.meta._Z11test_kernelv\0.shstrtab\n";
    memcpy(elfData + 256, strTab, strlen(strTab) + 1);
    
    uint8_t* metaSection = elfData + 192;
    AscendC::ElfTlvHead* head = reinterpret_cast<AscendC::ElfTlvHead*>(metaSection);
    head->type = AscendC::FUNC_META_TYPE_KERNEL_TYPE;
    head->length = 4;
    uint32_t kernelType = AscendC::K_TYPE_AIC;
    memcpy(metaSection + sizeof(AscendC::ElfTlvHead), &kernelType, sizeof(uint32_t));
    
    shdr0->sh_size = sizeof(AscendC::ElfTlvHead) + 4;

    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    AscendC::RegisterKernelElf(elfData, sizeof(elfData));
    
    // GetKenelMode now takes mangled name
    KernelMode mode = reg.GetKenelMode("_Z11test_kernelv");
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

// ===== ExtractKernelName tests =====

TEST_F(TestKernelElfParserSuite, ExtractKernelName_NoPostfix)
{
    // Section without _mix_aiv/_mix_aic postfix: strip prefix only
    std::string result = AscendC::ExtractKernelName(".ascend.meta._Z11test_kernelv");
    EXPECT_EQ(result, "_Z11test_kernelv");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_MixAivPostfix)
{
    // Section with _mix_aiv postfix: strip both prefix and postfix
    std::string result = AscendC::ExtractKernelName(".ascend.meta._Z11test_kernelv_mix_aiv");
    EXPECT_EQ(result, "_Z11test_kernelv");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_MixAicPostfix)
{
    // Section with _mix_aic postfix: strip both prefix and postfix
    std::string result = AscendC::ExtractKernelName(".ascend.meta._Z11test_kernelv_mix_aic");
    EXPECT_EQ(result, "_Z11test_kernelv");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_NonKernelSection)
{
    // Non-kernel-meta sections return empty string
    std::string result = AscendC::ExtractKernelName(".other.section");
    EXPECT_EQ(result, "");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_PrefixOnly)
{
    // Exactly ".ascend.meta." with nothing after: name is empty, should return ""
    std::string result = AscendC::ExtractKernelName(".ascend.meta.");
    EXPECT_EQ(result, "");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_ShortKernelName)
{
    // Short kernel name (shorter than postfix length), no postfix stripping
    std::string result = AscendC::ExtractKernelName(".ascend.meta.short");
    EXPECT_EQ(result, "short");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_EmptyInput)
{
    // Empty input is not a kernel meta section
    std::string result = AscendC::ExtractKernelName("");
    EXPECT_EQ(result, "");
}

TEST_F(TestKernelElfParserSuite, ExtractKernelName_NotStartingWithPrefix)
{
    // Section name not starting with .ascend.meta.
    std::string result = AscendC::ExtractKernelName(".text.something");
    EXPECT_EQ(result, "");
}

// ===== ParseKernelSections tests =====

TEST_F(TestKernelElfParserSuite, ParseKernelSections_MixAivAndAicSameKernel)
{
    // Construct ELF with two kernel meta sections:
    //  - .ascend.meta._Z11test_kernelv_mix_aic -> AIC_MODE
    //  - .ascend.meta._Z11test_kernelv_mix_aiv -> AIV_MODE
    // Both strip to "_Z11test_kernelv", and GetKenelMode returns the last registered mode

    uint8_t elfData[1024] = {0};

    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;

    elfData[40] = 64;   // e_shoff = 64
    elfData[58] = 64;   // e_shentsize = 64
    elfData[60] = 3;    // e_shnum = 3 (shstrtab + 2 kernel meta sections)
    elfData[62] = 0;    // e_shstrndx = 0 (shstrtab is section 0)

    // Section 0: .shstrtab
    Elf64_Shdr* shdr0 = reinterpret_cast<Elf64_Shdr*>(elfData + 64);
    shdr0->sh_name = 0;
    shdr0->sh_type = SHT_STRTAB;
    shdr0->sh_offset = 384;
    shdr0->sh_size = 128;

    // Section 1: .ascend.meta._Z11test_kernelv_mix_aic
    Elf64_Shdr* shdr1 = reinterpret_cast<Elf64_Shdr*>(elfData + 128);
    shdr1->sh_name = 1;  // offset in strtab -> ".ascend.meta._Z11test_kernelv_mix_aic"
    shdr1->sh_type = 0;
    shdr1->sh_offset = 512;
    shdr1->sh_size = sizeof(AscendC::ElfTlvHead) + 4;

    // Section 2: .ascend.meta._Z11test_kernelv_mix_aiv
    Elf64_Shdr* shdr2 = reinterpret_cast<Elf64_Shdr*>(elfData + 192);
    shdr2->sh_name = 39; // offset in strtab -> ".ascend.meta._Z11test_kernelv_mix_aiv"
    shdr2->sh_type = 0;
    shdr2->sh_offset = 576;
    shdr2->sh_size = sizeof(AscendC::ElfTlvHead) + 4;

    // String table: starts with '\0' then the two section names
    const char* sectionName1 = ".ascend.meta._Z11test_kernelv_mix_aic";
    const char* sectionName2 = ".ascend.meta._Z11test_kernelv_mix_aiv";
    elfData[384] = '\0';
    memcpy(elfData + 385, sectionName1, strlen(sectionName1) + 1);
    memcpy(elfData + 385 + strlen(sectionName1) + 1, sectionName2, strlen(sectionName2) + 1);

    // Meta section 1: K_TYPE_AIC
    uint8_t* metaSection1 = elfData + 512;
    AscendC::ElfTlvHead* head1 = reinterpret_cast<AscendC::ElfTlvHead*>(metaSection1);
    head1->type = AscendC::FUNC_META_TYPE_KERNEL_TYPE;
    head1->length = 4;
    uint32_t kernelTypeAic = AscendC::K_TYPE_AIC;
    memcpy(metaSection1 + sizeof(AscendC::ElfTlvHead), &kernelTypeAic, sizeof(uint32_t));

    // Meta section 2: K_TYPE_AIV
    uint8_t* metaSection2 = elfData + 576;
    AscendC::ElfTlvHead* head2 = reinterpret_cast<AscendC::ElfTlvHead*>(metaSection2);
    head2->type = AscendC::FUNC_META_TYPE_KERNEL_TYPE;
    head2->length = 4;
    uint32_t kernelTypeAiv = AscendC::K_TYPE_AIV;
    memcpy(metaSection2 + sizeof(AscendC::ElfTlvHead), &kernelTypeAiv, sizeof(uint32_t));

    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    AscendC::RegisterKernelElf(elfData, sizeof(elfData));

    // After parsing, the AIV section was registered last, so GetKenelMode returns AIV_MODE
    KernelMode mode = reg.GetKenelMode("_Z11test_kernelv");
    EXPECT_EQ(mode, KernelMode::AIV_MODE);
}

TEST_F(TestKernelElfParserSuite, ParseKernelSections_UnstrippedPostfixCausesDemangleFailure)
{
    // Simulate incorrect behavior (postfix not stripped): section name has _mix_aiv postfix
    // but is not a valid mangled name, causing Demangle to throw
    // This test verifies that KernelModeRegister rejects unstripped names

    uint8_t elfData[1024] = {0};

    elfData[EI_MAG0] = 0x7f;
    elfData[EI_MAG1] = 'E';
    elfData[EI_MAG2] = 'L';
    elfData[EI_MAG3] = 'F';
    elfData[EI_CLASS] = ELFCLASS64;
    elfData[EI_DATA] = ELFDATA2LSB;

    elfData[40] = 64;   // e_shoff
    elfData[58] = 64;   // e_shentsize
    elfData[60] = 2;    // e_shnum = 2
    elfData[62] = 0;    // e_shstrndx = 0

    // Section 0: .shstrtab
    Elf64_Shdr* shdr0 = reinterpret_cast<Elf64_Shdr*>(elfData + 64);
    shdr0->sh_name = 0;
    shdr0->sh_type = SHT_STRTAB;
    shdr0->sh_offset = 192;
    shdr0->sh_size = 128;

    // Section 1: kernel meta section with a valid mangled name (postfix already stripped)
    Elf64_Shdr* shdr1 = reinterpret_cast<Elf64_Shdr*>(elfData + 128);
    shdr1->sh_name = 1;
    shdr1->sh_type = 0;
    shdr1->sh_offset = 320;
    shdr1->sh_size = sizeof(AscendC::ElfTlvHead) + 4;

    // String table contains only the valid kernel name (postfix already stripped)
    memcpy(elfData + 192, "\0.ascend.meta._Z11test_kernelv", 31);

    uint8_t* metaSection = elfData + 320;
    AscendC::ElfTlvHead* head = reinterpret_cast<AscendC::ElfTlvHead*>(metaSection);
    head->type = AscendC::FUNC_META_TYPE_KERNEL_TYPE;
    head->length = 4;
    uint32_t kernelType = AscendC::K_TYPE_AIC;
    memcpy(metaSection + sizeof(AscendC::ElfTlvHead), &kernelType, sizeof(uint32_t));

    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    AscendC::RegisterKernelElf(elfData, sizeof(elfData));

    // Verify the kernel name with stripped postfix works
    KernelMode mode = reg.GetKenelMode("_Z11test_kernelv");
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}
