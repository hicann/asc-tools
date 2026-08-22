/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "options.h"

#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <unistd.h>

namespace npu::sanitizer::cli {
namespace {

bool NeedValue(int argc, char** argv, int& index, std::string& value, std::string& error)
{
    if (index + 1 >= argc) {
        error = std::string("missing value for ") + argv[index];
        return false;
    }
    value = argv[++index];
    return true;
}

std::optional<int> ParseTimeout(const std::string& text)
{
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || value < 100 || value > 120000) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

std::string AbsolutePath(const std::string& path)
{
    if (path.empty()) {
        return {};
    }
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    return error ? path : absolute.lexically_normal().string();
}

bool IsRegularFile(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

} // namespace

bool ParseOptions(int argc, char** argv, Options& options, std::string& error)
{
    options = {};

    int separator = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--") {
            separator = i;
            break;
        }
        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return true;
        }
        std::string value;
        if (argument == "--tool") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.toolConfig.toolName = value;
        } else if (argument == "--library") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.libraryPath = value;
        } else if (argument == "--log-file") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.toolConfig.logFile = AbsolutePath(value);
        } else if (argument == "--work-dir") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.toolConfig.workDir = AbsolutePath(value);
        } else if (argument == "--probe-cache-dir") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.toolConfig.probeCacheDir = AbsolutePath(value);
        } else if (argument == "--compile-option") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.toolConfig.compileOptions.push_back(value);
        } else if (argument == "--handshake-timeout-ms") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            const auto timeout = ParseTimeout(value);
            if (!timeout) {
                error = "--handshake-timeout-ms must be in [100, 120000]";
                return false;
            }
            options.handshakeTimeoutMs = *timeout;
        } else if (argument == "--strict") {
            options.toolConfig.strict = true;
        } else if (argument == "--no-strict") {
            options.toolConfig.strict = false;
        } else if (argument == "--keep-temp") {
            options.toolConfig.keepTemp = true;
        } else if (argument == "--no-keep-temp") {
            options.toolConfig.keepTemp = false;
        } else {
            error = "unknown option: " + argument;
            return false;
        }
    }

    if (separator < 0 || separator + 1 >= argc) {
        error = "expected -- followed by an application command";
        return false;
    }
    if (options.toolConfig.toolName.empty()) {
        error = "--tool is required";
        return false;
    }
    if (options.toolConfig.toolName != "memcheck") {
        error = "unsupported tool '" + options.toolConfig.toolName + "'; current implementation supports memcheck";
        return false;
    }
    if (options.toolConfig.compileOptions.size() > ipc::kMaxCompileOptions) {
        error = "too many --compile-option values";
        return false;
    }
    for (int i = separator + 1; i < argc; ++i) {
        options.application.emplace_back(argv[i]);
    }
    return true;
}

bool ResolveLibraryPath(const std::string& requested, std::string& resolved, std::string& error)
{
    std::vector<std::filesystem::path> candidates;
    if (!requested.empty()) {
        candidates.emplace_back(requested);
    } else if (const char* environment = std::getenv("NPU_CHECK_LIBRARY_PATH");
               environment != nullptr && environment[0] != '\0') {
        candidates.emplace_back(environment);
    } else {
        std::array<char, PATH_MAX + 1> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), PATH_MAX);
        if (length > 0 && length < PATH_MAX) {
            executable[static_cast<size_t>(length)] = '\0';
            const auto directory = std::filesystem::path(executable.data()).parent_path();
            candidates.push_back(directory / "libnpu_check.so");
            candidates.push_back(directory / ".." / "lib" / "libnpu_check.so");
            candidates.push_back(directory / ".." / "lib64" / "libnpu_check.so");
        }
    }

    for (const auto& candidate : candidates) {
        std::error_code filesystemError;
        const auto canonical = std::filesystem::canonical(candidate, filesystemError);
        if (!filesystemError && IsRegularFile(canonical)) {
            resolved = canonical.string();
            return true;
        }
    }
    error = "cannot locate libnpu_check.so; use --library or NPU_CHECK_LIBRARY_PATH";
    return false;
}

std::string Usage()
{
    return "Usage: npu_check --tool memcheck [options] -- <application> [args...]\n"
           "Options:\n"
           "  --library PATH              libnpu_check.so path\n"
           "  --log-file PATH             combined npu_check and application log\n"
           "  --work-dir PATH             sanitizer_api work directory\n"
           "  --probe-cache-dir PATH      patched probe cache directory\n"
           "  --compile-option OPTION     repeatable user compilation metadata\n"
           "  --strict | --no-strict      treat unknown GM addresses as errors or warnings\n"
           "  --keep-temp | --no-keep-temp preserve or remove the private session directory\n"
           "  --handshake-timeout-ms N    injection handshake timeout, 100..120000\n";
}

} // namespace npu::sanitizer::cli
