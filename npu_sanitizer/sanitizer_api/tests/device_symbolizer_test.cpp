/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_symbolizer.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using aclsan::probe::CallStackResult;
using aclsan::probe::DeviceSymbolizer;
using aclsan::probe::DeviceSymbolizerConfig;

fs::path TestDirectory(const char* name)
{
    const fs::path directory = fs::temp_directory_path() / name;
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;
}

void TestParsesFramesAndCachesByImageAndPc()
{
    const fs::path directory = TestDirectory("aclsan-device-symbolizer-test");
    const DeviceSymbolizerConfig config{"/tools/llvm-symbolizer", "/work/kernel.elf", directory.string()};
    DeviceSymbolizer symbolizer(config);
    size_t runnerCalls = 0;
    std::vector<std::string> command;
    const auto runner = [&runnerCalls, &command](
                            const std::vector<std::string>& actualCommand, const std::string& logPath, std::string&) {
        ++runnerCalls;
        command = actualCommand;
        std::ofstream output(logPath);
        output << "CopyIn\n/src/kernel.asc:46:5\n"
               << "AddKernel\n/src/kernel.asc:58:1\n";
        return true;
    };

    const CallStackResult first = symbolizer.ResolveCallStackWithRunner(0x170, runner);
    const CallStackResult second = symbolizer.ResolveCallStackWithRunner(0x170, runner);

    assert(first.available);
    assert(first.error.empty());
    assert(first.frames.size() == 2);
    assert(first.frames[0].functionName == "CopyIn");
    assert(first.frames[0].fileName == "/src/kernel.asc");
    assert(first.frames[0].line == 46);
    assert(first.frames[0].column == 5);
    assert(first.frames[0].inlineDepth == 0);
    assert(first.frames[1].functionName == "AddKernel");
    assert(first.frames[1].fileName == "/src/kernel.asc");
    assert(first.frames[1].line == 58);
    assert(first.frames[1].column == 1);
    assert(first.frames[1].inlineDepth == 1);
    assert(second.frames.size() == 2);
    assert(runnerCalls == 1);
    assert(
        command == std::vector<std::string>(
                       {"/tools/llvm-symbolizer", "--obj=/work/kernel.elf", "--inlines", "--demangle",
                        "--functions=short", "0x170"}));
    fs::remove_all(directory);
}

void TestRejectsInvalidSymbolizerOutput()
{
    const fs::path directory = TestDirectory("aclsan-device-symbolizer-invalid-test");
    const DeviceSymbolizerConfig config{"/tools/llvm-symbolizer", "/work/kernel.elf", directory.string()};
    const std::vector<std::string> outputs{
        "", "CopyIn\n", "??\n??\n", "CopyIn\n/src/kernel.asc:not-a-line:5\n", "CopyIn\n/src/kernel.asc:42\n"};
    for (const std::string& text : outputs) {
        DeviceSymbolizer symbolizer(config);
        const auto runner = [&text](const std::vector<std::string>&, const std::string& logPath, std::string&) {
            std::ofstream output(logPath);
            output << text;
            return true;
        };
        const CallStackResult result = symbolizer.ResolveCallStackWithRunner(0x180, runner);
        assert(!result.available);
        assert(result.frames.empty());
        assert(result.error == "invalid_symbolizer_output");
    }
    fs::remove_all(directory);
}

void TestParsesLocationFromTheRight()
{
    const fs::path directory = TestDirectory("aclsan-device-symbolizer-location-test");
    const DeviceSymbolizerConfig config{"/tools/llvm-symbolizer", "/work/kernel.elf", directory.string()};
    DeviceSymbolizer symbolizer(config);
    const auto runner = [](const std::vector<std::string>&, const std::string& logPath, std::string&) {
        std::ofstream output(logPath);
        output << "CopyIn\n/src/generated:kernel.asc:46:5\n";
        return true;
    };

    const CallStackResult result = symbolizer.ResolveCallStackWithRunner(0x1a0, runner);

    assert(result.available);
    assert(result.frames.size() == 1);
    assert(result.frames[0].fileName == "/src/generated:kernel.asc");
    assert(result.frames[0].line == 46);
    assert(result.frames[0].column == 5);
    fs::remove_all(directory);
}

void TestReportsRunnerFailure()
{
    const fs::path directory = TestDirectory("aclsan-device-symbolizer-runner-test");
    const DeviceSymbolizerConfig config{"/tools/llvm-symbolizer", "/work/kernel.elf", directory.string()};
    DeviceSymbolizer symbolizer(config);
    const auto runner = [](const std::vector<std::string>&, const std::string&, std::string& error) {
        error = "simulated failure";
        return false;
    };

    const CallStackResult result = symbolizer.ResolveCallStackWithRunner(0x190, runner);

    assert(!result.available);
    assert(result.error == "simulated failure");
    fs::remove_all(directory);
}

} // namespace

int main()
{
    TestParsesFramesAndCachesByImageAndPc();
    TestRejectsInvalidSymbolizerOutput();
    TestParsesLocationFromTheRight();
    TestReportsRunnerFailure();
    return 0;
}
