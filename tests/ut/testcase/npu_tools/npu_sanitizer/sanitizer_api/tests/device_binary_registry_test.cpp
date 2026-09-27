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

#include "device_runtime/device_binary_registry.h"

#include <cassert>
#include <cstring>
#include <elf.h>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = boost::filesystem;

namespace {

const std::vector<uint8_t>& DeviceImage()
{
    static const std::vector<uint8_t> image{0x7f, 'E', 'L', 'F', 1, 2, 3, 4};
    return image;
}

void TestResolvesActiveBinaryAndClearsItOnUnload()
{
    const fs::path root = fs::temp_directory_path() /
                          ("npu-check-" + std::to_string(static_cast<unsigned long long>(geteuid()))) / "symbolizer";
    fs::remove_all(root);

    aclsan::device_runtime::DeviceBinaryRegistry registry;
    const auto& image = DeviceImage();
    const uintptr_t binary = 0x51;
    assert(registry.RecordBinaryLoadFromData(binary, true, 24, image.data(), image.size()));

    std::vector<std::string> command;
    const auto runner = [&command](
                            const std::vector<std::string>& actualCommand, const std::string& logPath, std::string&) {
        command = actualCommand;
        std::ofstream output(logPath);
        output << "CopyIn\n/src/kernel.asc:46:5\n"
               << "AddKernel\n/src/kernel.asc:58:1\n";
        return true;
    };

    const auto resolved = registry.ResolveCallStackWithRunner(0x170, runner);
    assert(resolved.available);
    assert(resolved.binaryId != 0);
    assert(resolved.pc == 0x170);
    assert(resolved.frames.size() == 2);
    assert(resolved.frames[0].functionName == "CopyIn");
    assert(resolved.frames[0].fileName == "/src/kernel.asc");
    assert(command.size() == 6);
    assert(command[1].find("--obj=" + root.string()) == 0);

    registry.RecordBinaryUnload(binary);
    const auto unloaded = registry.ResolveCallStackWithRunner(0x170, runner);
    assert(!unloaded.available);
    assert(unloaded.binaryId == 0);
    assert(unloaded.error == "invalid_state");
    assert(fs::is_empty(root));
    fs::remove_all(root);
}

void TestExplicitFunctionOwnershipEndsAtBinaryUnload()
{
    const fs::path root = fs::temp_directory_path() /
                          ("npu-check-" + std::to_string(static_cast<unsigned long long>(geteuid()))) / "symbolizer";
    fs::remove_all(root);

    aclsan::device_runtime::DeviceBinaryRegistry registry;
    const auto& image = DeviceImage();
    constexpr uintptr_t binary = 0x61;
    constexpr uintptr_t function = 0x610;
    assert(registry.RecordBinaryLoadFromData(binary, true, 24, image.data(), image.size()));
    uint32_t traceArgumentOffset = 0;
    assert(!registry.GetFunctionTraceArgumentOffset(function, traceArgumentOffset));

    registry.RecordBinaryFunctionLookup(binary, function);
    assert(registry.GetFunctionTraceArgumentOffset(function, traceArgumentOffset));
    assert(traceArgumentOffset == 24);

    registry.RecordBinaryUnload(binary);
    assert(!registry.GetFunctionTraceArgumentOffset(function, traceArgumentOffset));
    assert(fs::is_empty(root));
    fs::remove_all(root);
}

void TestLatestLookupRequiresAnInstrumentedLatestBinary()
{
    const fs::path root = fs::temp_directory_path() /
                          ("npu-check-" + std::to_string(static_cast<unsigned long long>(geteuid()))) / "symbolizer";
    fs::remove_all(root);

    aclsan::device_runtime::DeviceBinaryRegistry registry;
    const auto& image = DeviceImage();
    constexpr uintptr_t instrumentedBinary = 0x71;
    constexpr uintptr_t plainBinary = 0x72;
    constexpr uintptr_t instrumentedFunction = 0x710;
    constexpr uintptr_t plainFunction = 0x720;
    constexpr uintptr_t explicitFunction = 0x711;
    uint32_t traceArgumentOffset = 0;

    assert(registry.RecordBinaryLoadFromData(instrumentedBinary, true, 24, image.data(), image.size()));
    registry.RecordLatestBinaryFunctionLookup(instrumentedFunction);
    assert(registry.GetFunctionTraceArgumentOffset(instrumentedFunction, traceArgumentOffset));
    assert(traceArgumentOffset == 24);

    assert(registry.RecordBinaryLoadFromData(plainBinary, false, 0, nullptr, 0));
    registry.RecordLatestBinaryFunctionLookup(plainFunction);
    assert(!registry.GetFunctionTraceArgumentOffset(plainFunction, traceArgumentOffset));

    registry.RecordBinaryFunctionLookup(instrumentedBinary, explicitFunction);
    assert(registry.GetFunctionTraceArgumentOffset(explicitFunction, traceArgumentOffset));
    assert(traceArgumentOffset == 24);
    registry.Reset();
    assert(!registry.GetFunctionTraceArgumentOffset(instrumentedFunction, traceArgumentOffset));
    assert(!registry.GetFunctionTraceArgumentOffset(explicitFunction, traceArgumentOffset));
    assert(fs::is_empty(root));
    fs::remove_all(root);
}

void TestFunctionLookupReplacesPreviousBinaryOwnership()
{
    aclsan::device_runtime::DeviceBinaryRegistry registry;
    constexpr uintptr_t instrumentedBinary = 0x79;
    constexpr uintptr_t plainBinary = 0x7a;
    constexpr uintptr_t explicitFunction = 0x790;
    constexpr uintptr_t latestFunction = 0x791;
    const auto& image = DeviceImage();
    uint32_t traceArgumentOffset = 0;

    assert(registry.RecordBinaryLoadFromData(instrumentedBinary, true, 24, image.data(), image.size()));
    registry.RecordBinaryFunctionLookup(instrumentedBinary, explicitFunction);
    registry.RecordBinaryFunctionLookup(instrumentedBinary, latestFunction);
    assert(registry.GetFunctionTraceArgumentOffset(explicitFunction, traceArgumentOffset));
    assert(traceArgumentOffset == 24);
    assert(registry.GetFunctionTraceArgumentOffset(latestFunction, traceArgumentOffset));
    assert(traceArgumentOffset == 24);

    assert(registry.RecordBinaryLoadFromData(plainBinary, false, 0, nullptr, 0));
    registry.RecordBinaryFunctionLookup(plainBinary, explicitFunction);
    registry.RecordLatestBinaryFunctionLookup(latestFunction);
    assert(!registry.GetFunctionTraceArgumentOffset(explicitFunction, traceArgumentOffset));
    assert(!registry.GetFunctionTraceArgumentOffset(latestFunction, traceArgumentOffset));
}

} // namespace

TEST(DeviceBinaryRegistry, ReadOnlySectionsRemainLaunchScopedAcrossUnloadAndReuse)
{
    Elf64_Ehdr header{};
    std::memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(header);
    header.e_shoff = sizeof(header);
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = 3;
    header.e_shstrndx = 1;
    Elf64_Shdr names{};
    names.sh_type = SHT_STRTAB;
    names.sh_offset = sizeof(header) + 3 * sizeof(Elf64_Shdr);
    names.sh_size = 9;
    Elf64_Shdr rodata{};
    rodata.sh_name = 1;
    rodata.sh_type = SHT_PROGBITS;
    rodata.sh_flags = SHF_ALLOC;
    rodata.sh_offset = names.sh_offset + names.sh_size;
    rodata.sh_addr = 0x9000;
    rodata.sh_size = 16;
    std::string image(rodata.sh_offset + rodata.sh_size, '\0');
    std::memcpy(image.data(), &header, sizeof(header));
    std::memcpy(image.data() + header.e_shoff + sizeof(Elf64_Shdr), &names, sizeof(names));
    std::memcpy(image.data() + header.e_shoff + 2 * sizeof(Elf64_Shdr), &rodata, sizeof(rodata));
    std::memcpy(image.data() + names.sh_offset, "\0.rodata\0", names.sh_size);

    aclsan::device_runtime::DeviceBinaryRegistry registry;
    ASSERT_TRUE(registry.RecordLoadedImage(1, image.data(), image.size()));
    auto originalImage = image;
    rodata.sh_addr = 0x1000;
    std::memcpy(originalImage.data() + header.e_shoff + 2 * sizeof(Elf64_Shdr), &rodata, sizeof(rodata));
    ASSERT_TRUE(registry.RecordBinaryLoadFromData(1, true, 0, originalImage.data(), originalImage.size()));
    registry.RecordBinaryFunctionLookup(1, 2, "kernel");
    uintptr_t binary = 0;
    std::vector<aclsan::BinarySectionRange> snapshot;
    ASSERT_TRUE(registry.GetFunctionReadOnlySections(2, binary, snapshot));
    ASSERT_EQ(snapshot.size(), 1U);
    EXPECT_EQ(binary, 1U);
    EXPECT_EQ(snapshot[0].offset, 0x9000U);
    registry.RecordBinaryUnload(1);
    std::vector<aclsan::BinarySectionRange> next;
    EXPECT_FALSE(registry.GetFunctionReadOnlySections(2, binary, next));
    EXPECT_EQ(snapshot.size(), 1U);
    ASSERT_TRUE(registry.RecordBinaryLoadFromData(1, false, 0, image.data(), image.size()));
    registry.RecordBinaryFunctionLookup(1, 2, "kernel");
    EXPECT_FALSE(registry.GetFunctionReadOnlySections(2, binary, next));
    ASSERT_TRUE(registry.RecordLoadedImage(1, image.data(), image.size()));
    EXPECT_FALSE(registry.RecordLoadedImage(1, "invalid", 7));
    EXPECT_FALSE(registry.GetFunctionReadOnlySections(2, binary, next));
    ASSERT_TRUE(registry.RecordLoadedImage(1, image.data(), image.size()));
    registry.Reset();
    EXPECT_FALSE(registry.GetFunctionReadOnlySections(2, binary, next));
}

TEST(DeviceBinaryRegistry, Main)
{
    TestResolvesActiveBinaryAndClearsItOnUnload();
    TestExplicitFunctionOwnershipEndsAtBinaryUnload();
    TestLatestLookupRequiresAnInstrumentedLatestBinary();
    TestFunctionLookupReplacesPreviousBinaryOwnership();
}
