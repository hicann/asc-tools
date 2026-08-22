/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "probe_runtime.h"

namespace {

void* g_probeOutput = nullptr;
size_t g_probeBytes = 0;
uint64_t g_outputSlot = 0;
uint32_t g_offsetSlot = UINT32_MAX;
uint32_t g_freeCalls = 0;
aclrtKernelType g_kernelType = ACL_KERNEL_TYPE_AICORE;
uint16_t g_aicRatio = 1;
uint16_t g_aivRatio = 2;
aclrtBinHandle g_binary = reinterpret_cast<aclrtBinHandle>(0x1110);
aclrtFuncHandle g_function = reinterpret_cast<aclrtFuncHandle>(0x2220);

aclError FakeBinaryLoad(const void* data, size_t length, const aclrtBinaryLoadOptions*, aclrtBinHandle* binHandle)
{
    if (data == nullptr || length == 0 || binHandle == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *binHandle = g_binary;
    return ACL_SUCCESS;
}

aclError FakeBinaryGetFunction(const aclrtBinHandle, const char*, aclrtFuncHandle* funcHandle)
{
    *funcHandle = g_function;
    return ACL_SUCCESS;
}

aclError FakeBinaryGetGlobal(aclrtBinHandle, const char* name, void** address, size_t* bytes)
{
    if (std::strcmp(name, "g_sanitizerOutput") == 0) {
        *address = &g_outputSlot;
        *bytes = sizeof(g_outputSlot);
        return ACL_SUCCESS;
    }
    if (std::strcmp(name, "g_sanitizerAivOffset") == 0) {
        *address = &g_offsetSlot;
        *bytes = sizeof(g_offsetSlot);
        return ACL_SUCCESS;
    }
    return ACL_ERROR_INVALID_PARAM;
}

aclError FakeGetFunctionAttribute(aclrtFuncHandle, aclrtFuncAttribute attribute, int64_t* value)
{
    if (value == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (attribute == ACL_FUNC_ATTR_KERNEL_TYPE) {
        *value = g_kernelType;
        return ACL_SUCCESS;
    }
    if (attribute == ACL_FUNC_ATTR_KERNEL_RATIO) {
        *value = static_cast<int64_t>((static_cast<uint64_t>(g_aicRatio) << 16U) | g_aivRatio);
        return ACL_SUCCESS;
    }
    return ACL_ERROR_INVALID_PARAM;
}

aclError FakeMalloc(void** address, size_t bytes, aclrtMemMallocPolicy)
{
    g_probeOutput = std::malloc(bytes);
    g_probeBytes = bytes;
    *address = g_probeOutput;
    return g_probeOutput == nullptr ? ACL_ERROR_BAD_ALLOC : ACL_SUCCESS;
}

aclError FakeFree(void* address)
{
    ++g_freeCalls;
    std::free(address);
    g_probeOutput = nullptr;
    g_probeBytes = 0;
    return ACL_SUCCESS;
}

aclError FakeMemcpy(void* dst, size_t dstMax, const void* src, size_t bytes, aclrtMemcpyKind)
{
    if (dst == nullptr || src == nullptr || bytes > dstMax) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::memcpy(dst, src, bytes);
    return ACL_SUCCESS;
}

bool FakeTransform(
    const void* data, size_t length, const aclsan::probe::ImageTransformConfig&,
    aclsan::probe::ImageTransformResult& result, std::string&)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    result.image.assign(bytes, bytes + length);
    result.sessionDirectory = "/tmp/fake-probe-session";
    result.originalImage = "/tmp/fake-probe-session/original_device.elf";
    return true;
}

aclsan::probe::ProbeRuntimeApi MakeApi()
{
    return {
        FakeBinaryLoad,
        FakeBinaryGetFunction,
        nullptr,
        FakeBinaryGetGlobal,
        FakeGetFunctionAttribute,
        FakeMalloc,
        FakeFree,
        FakeMemcpy,
    };
}

void LoadTestFunction(
    aclsan::probe::ProbeRuntime& runtime, const aclsan::probe::ProbeRuntimeApi& api, aclrtFuncHandle& function)
{
    const std::vector<uint8_t> image{0x7f, 'E', 'L', 'F'};
    aclrtBinHandle binary = nullptr;
    std::string error;
    assert(
        runtime.LoadBinary(image.data(), image.size(), nullptr, &binary, {}, api, FakeTransform, error) == ACL_SUCCESS);
    assert(runtime.GetFunction(binary, "kernel", &function, api) == ACL_SUCCESS);
}

void WriteOneRecord()
{
    auto* output = static_cast<uint8_t*>(g_probeOutput);
    uint64_t offset = sanitizer::kProbeRecordStartBytes;
    const sanitizer::CopyAlignRecord record{
        {static_cast<uint32_t>(sanitizer::ProbeInstrType::CopyGmToUbufAlignV2B16), 0x180},
        0x4000,
        0x3000,
        (1ULL << 4) | (32ULL << 25),
        0};
    const uint64_t payloadBytes = sizeof(record);
    std::memcpy(output + offset, &payloadBytes, sizeof(payloadBytes));
    offset += sizeof(payloadBytes);
    std::memcpy(output + offset, &record, sizeof(record));
    offset += sizeof(record);
    const sanitizer::ProbeBlockHeader header{1, offset, 0, 5};
    std::memcpy(output, &header, sizeof(header));
}

void TestRuntimeReadsProbeBuffer()
{
    aclsan::probe::ProbeRuntime runtime;
    const aclsan::probe::ProbeRuntimeApi api = MakeApi();
    const std::vector<uint8_t> image{0x7f, 'E', 'L', 'F'};
    aclrtBinHandle binary = nullptr;
    std::string error;

    assert(
        runtime.LoadBinary(image.data(), image.size(), nullptr, &binary, {}, api, FakeTransform, error) == ACL_SUCCESS);
    assert(binary == g_binary);

    aclrtFuncHandle function = nullptr;
    assert(runtime.GetFunction(binary, "kernel", &function, api) == ACL_SUCCESS);
    assert(function == g_function);

    aclrtStream stream = reinterpret_cast<aclrtStream>(0x3330);
    assert(runtime.PrepareLaunch(function, 1, stream, api) == ACL_SUCCESS);
    assert(g_probeBytes == 3 * sanitizer::kProbeBlockBytes);
    assert(g_outputSlot == reinterpret_cast<uint64_t>(g_probeOutput));
    assert(g_offsetSlot == 1);
    assert(runtime.PrepareLaunch(function, 1, stream, api) == ACL_ERROR_RT_INTERNAL_ERROR);

    runtime.RecordLaunchResult(function, stream, ACL_SUCCESS);
    WriteOneRecord();
    sanitizer::ProbeParseResult result;
    assert(runtime.Collect(stream, api, result) == ACL_SUCCESS);
    assert(result.records.size() == 1);
    assert(result.records[0].record.args[0] == 0x4000);
    assert(result.records[0].transferBytes == 32);
    assert(result.records[0].coreId == 5);

    assert(runtime.Clear(api) == ACL_SUCCESS);
    assert(g_freeCalls == 1);
}

void TestRuntimeUsesKernelSpecificLayout()
{
    const aclsan::probe::ProbeRuntimeApi api = MakeApi();
    aclrtStream stream = reinterpret_cast<aclrtStream>(0x4440);

    g_kernelType = ACL_KERNEL_TYPE_CUBE;
    {
        aclsan::probe::ProbeRuntime runtime;
        aclrtFuncHandle function = nullptr;
        LoadTestFunction(runtime, api, function);
        assert(runtime.PrepareLaunch(function, 2, stream, api) == ACL_SUCCESS);
        assert(g_probeBytes == 2 * sanitizer::kProbeBlockBytes);
        assert(g_offsetSlot == 2);
        assert(runtime.Clear(api) == ACL_SUCCESS);
    }

    g_kernelType = ACL_KERNEL_TYPE_VECTOR;
    {
        aclsan::probe::ProbeRuntime runtime;
        aclrtFuncHandle function = nullptr;
        LoadTestFunction(runtime, api, function);
        assert(runtime.PrepareLaunch(function, 2, stream, api) == ACL_SUCCESS);
        assert(g_probeBytes == 2 * sanitizer::kProbeBlockBytes);
        assert(g_offsetSlot == 0);
        assert(runtime.Clear(api) == ACL_SUCCESS);
    }

    g_kernelType = ACL_KERNEL_TYPE_AICORE;
    g_aicRatio = 0;
    g_aivRatio = 0;
    {
        aclsan::probe::ProbeRuntime runtime;
        aclrtFuncHandle function = nullptr;
        LoadTestFunction(runtime, api, function);
        assert(runtime.PrepareLaunch(function, 1, stream, api) == ACL_ERROR_RT_INTERNAL_ERROR);
        assert(runtime.Clear(api) == ACL_SUCCESS);
    }
    g_aicRatio = 1;
    g_aivRatio = 2;
}

void TestRuntimeResolvesDevicePcAndClearsSymbolizerState()
{
    std::filesystem::create_directories("/tmp/fake-probe-session");
    const aclsan::probe::ProbeRuntimeApi api = MakeApi();
    aclsan::probe::ProbeRuntime runtime;
    aclrtBinHandle binary = nullptr;
    const std::vector<uint8_t> image{0x7f, 'E', 'L', 'F'};
    std::string error;

    assert(
        runtime.LoadBinary(image.data(), image.size(), nullptr, &binary, {}, api, FakeTransform, error) == ACL_SUCCESS);

    size_t runnerCalls = 0;
    const auto runner = [&runnerCalls](const std::vector<std::string>&, const std::string& logPath, std::string&) {
        ++runnerCalls;
        std::ofstream output(logPath);
        output << "CopyIn\n/src/kernel.asc:46:5\n";
        return true;
    };
    const auto result = runtime.ResolveCallStackWithRunner(0x1a0, runner);
    assert(result.available);
    assert(result.binaryId != 0);
    assert(result.frames.size() == 1);
    assert(result.frames.front().functionName == "CopyIn");
    assert(runnerCalls == 1);

    const auto cached = runtime.ResolveCallStackWithRunner(0x1a0, runner);
    assert(cached.binaryId == result.binaryId);

    assert(runtime.Clear(api) == ACL_SUCCESS);
    const auto cleared = runtime.ResolveCallStackWithRunner(0x1a0, runner);
    assert(!cleared.available);
    assert(cleared.binaryId == 0);
    assert(cleared.error == "invalid_state");

    assert(
        runtime.LoadBinary(image.data(), image.size(), nullptr, &binary, {}, api, FakeTransform, error) == ACL_SUCCESS);
    const auto reloaded = runtime.ResolveCallStackWithRunner(0x1a0, runner);
    assert(reloaded.available);
    assert(reloaded.binaryId != 0);
    assert(reloaded.binaryId != result.binaryId);
    assert(runtime.Clear(api) == ACL_SUCCESS);
    std::filesystem::remove_all("/tmp/fake-probe-session");
}

void TestRuntimeFindsSymbolizerUnderAscendHome()
{
    const std::filesystem::path ascendHome = "/tmp/fake-ascend-home";
    const std::filesystem::path symbolizer = ascendHome / "tools" / "mssanitizer" / "bin" / "llvm-symbolizer";
    std::filesystem::remove_all(ascendHome);
    std::filesystem::create_directories(symbolizer.parent_path());
    std::ofstream(symbolizer) << "#!/bin/sh\n";
    std::filesystem::permissions(symbolizer, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    setenv("ASCEND_HOME_PATH", ascendHome.c_str(), 1);
    unsetenv("ACLSAN_SYMBOLIZER");
    std::filesystem::create_directories("/tmp/fake-probe-session");

    const aclsan::probe::ProbeRuntimeApi api = MakeApi();
    aclsan::probe::ProbeRuntime runtime;
    aclrtBinHandle binary = nullptr;
    const std::vector<uint8_t> image{0x7f, 'E', 'L', 'F'};
    std::string error;
    assert(
        runtime.LoadBinary(image.data(), image.size(), nullptr, &binary, {}, api, FakeTransform, error) == ACL_SUCCESS);

    std::vector<std::string> command;
    const auto runner = [&command](
                            const std::vector<std::string>& actualCommand, const std::string& logPath, std::string&) {
        command = actualCommand;
        std::ofstream output(logPath);
        output << "CopyIn\n/src/kernel.asc:46:5\n";
        return true;
    };
    assert(runtime.ResolveCallStackWithRunner(0x1b0, runner).available);
    assert(command.front() == symbolizer.string());
    assert(runtime.Clear(api) == ACL_SUCCESS);
    std::filesystem::remove_all(ascendHome);
    std::filesystem::remove_all("/tmp/fake-probe-session");
    unsetenv("ASCEND_HOME_PATH");
}

} // namespace

int main()
{
    TestRuntimeReadsProbeBuffer();
    TestRuntimeUsesKernelSpecificLayout();
    TestRuntimeResolvesDevicePcAndClearsSymbolizerState();
    TestRuntimeFindsSymbolizerUnderAscendHome();
    return 0;
}
