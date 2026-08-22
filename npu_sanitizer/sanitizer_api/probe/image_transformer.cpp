/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "image_transformer.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iterator>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace aclsan::probe {
namespace {

namespace fs = std::filesystem;

constexpr size_t kMinimumElfBytes = 64;
constexpr size_t kMaximumElfBytes = 512ULL * 1024ULL * 1024ULL;

bool IsElf(const void* data, size_t length) noexcept
{
    if (data == nullptr || length < kMinimumElfBytes || length > kMaximumElfBytes) {
        return false;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    return bytes[0] == 0x7f && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F';
}

bool WriteImage(const fs::path& path, const void* data, size_t length, std::string& error)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "cannot create " + path.string();
        return false;
    }
    output.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
    if (!output) {
        error = "cannot write " + path.string();
        return false;
    }
    return true;
}

bool ReadImage(const fs::path& path, std::vector<uint8_t>& image, std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "cannot open " + path.string();
        return false;
    }
    image.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (!IsElf(image.data(), image.size())) {
        error = "output is not a valid ELF: " + path.string();
        image.clear();
        return false;
    }
    return true;
}

bool ReadText(const fs::path& path, std::string& text)
{
    std::ifstream input(path);
    if (!input) {
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

bool CreateSession(const std::string& workRoot, std::string& sessionDirectory, std::string& error)
{
    std::error_code fsError;
    fs::create_directories(workRoot, fsError);
    if (fsError) {
        error = "cannot create work root: " + fsError.message();
        return false;
    }
    std::string pattern = (fs::path(workRoot) / "load_XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    char* created = mkdtemp(buffer.data());
    if (created == nullptr) {
        error = "mkdtemp failed: " + std::string(std::strerror(errno));
        return false;
    }
    sessionDirectory = created;
    return true;
}

std::string CommandText(const std::vector<std::string>& command)
{
    std::ostringstream output;
    for (size_t index = 0; index < command.size(); ++index) {
        output << (index == 0 ? "" : " ") << command[index];
    }
    return output.str();
}

} // namespace

bool RunCommand(const std::vector<std::string>& command, const std::string& logPath, std::string& error)
{
    if (command.empty()) {
        error = "empty command";
        return false;
    }
    const pid_t child = fork();
    if (child < 0) {
        error = "fork failed: " + std::string(std::strerror(errno));
        return false;
    }
    if (child == 0) {
        const int logFd = open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (logFd < 0 || dup2(logFd, STDOUT_FILENO) < 0 || dup2(logFd, STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(logFd);
        std::vector<char*> arguments;
        arguments.reserve(command.size() + 1);
        for (const std::string& argument : command) {
            arguments.push_back(const_cast<char*>(argument.c_str()));
        }
        arguments.push_back(nullptr);
        execvp(arguments[0], arguments.data());
        _exit(127);
    }

    int processStatus = 0;
    while (waitpid(child, &processStatus, 0) < 0) {
        if (errno != EINTR) {
            error = "waitpid failed: " + std::string(std::strerror(errno));
            return false;
        }
    }
    if (!WIFEXITED(processStatus) || WEXITSTATUS(processStatus) != 0) {
        error = "command failed: " + CommandText(command) + ", log=" + logPath;
        return false;
    }
    return true;
}

bool TransformDeviceImageWithRunner(
    const void* data, size_t length, const ImageTransformConfig& config, ImageTransformResult& result,
    std::string& error, const CommandRunner& runner)
{
    result = {};
    if (!IsElf(data, length)) {
        error = "binary-load input is not a complete ELF";
        return false;
    }
    if (config.probeObject.empty() || config.ctrlBinary.empty() || config.symbolOrdering.empty() ||
        config.workRoot.empty() || config.argumentBytes == 0) {
        error = "probe transform configuration is incomplete";
        return false;
    }
    if (!CreateSession(config.workRoot, result.sessionDirectory, error)) {
        return false;
    }

    const fs::path session = result.sessionDirectory;
    const fs::path original = session / "original_device.elf";
    const fs::path linked = session / "kernel_with_probe.o";
    const fs::path instrumented = session / "kernel_instrumented.o";
    const fs::path linkLog = session / "ld.lld.log";
    const fs::path tuneLog = session / "bisheng_tune.log";
    result.originalImage = original.string();
    result.tuneLog = tuneLog.string();
    if (!WriteImage(original, data, length, error)) {
        return false;
    }

    const std::vector<std::string> linkCommand{
        "ld.lld",
        "-m",
        "aicorelinux",
        "-Ttext=0",
        "-execute-probe",
        "--symbol-ordering-file",
        config.symbolOrdering,
        "-z",
        "separate-loadable-segments",
        config.probeObject,
        original.string(),
        "-static",
        "-q",
        "-o",
        linked.string()};
    if (!runner(linkCommand, linkLog.string(), error)) {
        return false;
    }

    const std::vector<std::string> tuneCommand{
        "bisheng-tune",
        "--action=instru-probe",
        "--instru-memprobe",
        "--dbi-config=" + config.ctrlBinary,
        "--tune-argsize=" + std::to_string(config.argumentBytes),
        "--probe-verb=2",
        linked.string(),
        "-o",
        instrumented.string()};
    if (!runner(tuneCommand, tuneLog.string(), error)) {
        return false;
    }
    std::string tuneText;
    if (!ReadText(tuneLog, tuneText) || tuneText.find("ApiId:") == std::string::npos ||
        tuneText.find("StubTimes:") == std::string::npos) {
        error = "bisheng-tune produced no insertion evidence: " + tuneLog.string();
        return false;
    }
    return ReadImage(instrumented, result.image, error);
}

bool TransformDeviceImage(
    const void* data, size_t length, const ImageTransformConfig& config, ImageTransformResult& result,
    std::string& error)
{
    return TransformDeviceImageWithRunner(data, length, config, result, error, RunCommand);
}

} // namespace aclsan::probe
