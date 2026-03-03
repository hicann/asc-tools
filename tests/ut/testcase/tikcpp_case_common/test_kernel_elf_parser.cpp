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

TEST_F(TestKernelElfParserSuite, KernelModeRegister_RegisterAndGet)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    reg.Register(".ascend.meta.test_kernel", KernelMode::AIC_MODE);
    
    KernelMode mode = reg.GetKenelMode("test_kernel");
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_MixAivPostfix)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    reg.Register(".ascend.meta.test_kernel_mix_aiv", KernelMode::AIV_MODE);
    
    KernelMode mode = reg.GetKenelMode("test_kernel");
    EXPECT_EQ(mode, KernelMode::AIV_MODE);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_MixAicPostfix)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    reg.Register(".ascend.meta.test_kernel_mix_aic", KernelMode::AIC_MODE);
    
    KernelMode mode = reg.GetKenelMode("test_kernel");
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_NotFound)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    EXPECT_THROW(reg.GetKenelMode("nonexistent_kernel"), std::invalid_argument);
}

TEST_F(TestKernelElfParserSuite, KernelModeRegister_Priority)
{
    AscendC::KernelModeRegister& reg = AscendC::KernelModeRegister::GetInstance();
    reg.Clear();
    reg.Register(".ascend.meta.test_kernel", KernelMode::AIV_MODE);
    reg.Register(".ascend.meta.test_kernel_mix_aiv", KernelMode::MIX_MODE);
    
    KernelMode mode = reg.GetKenelMode("test_kernel");
    EXPECT_EQ(mode, KernelMode::MIX_MODE);
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
    
    const char* strTab = ".ascend.meta.test_kernel\0.shstrtab\n";
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
    
    KernelMode mode = reg.GetKenelMode("test_kernel");
    EXPECT_EQ(mode, KernelMode::AIC_MODE);
}
