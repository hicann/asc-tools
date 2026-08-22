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

#include "image_transformer.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace aclsan::probe {
namespace {

std::string HexPc(uint64_t pc)
{
    std::ostringstream output;
    output << "0x" << std::hex << pc;
    return output.str();
}

std::string NormalizeError(const std::string& error)
{
    std::string normalized = error.empty() ? "command_failed" : error;
    for (char& character : normalized) {
        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            character = ' ';
        }
    }
    return normalized;
}

bool ParseUint32(const std::string& text, uint32_t& value)
{
    if (text.empty()) {
        return false;
    }
    uint64_t parsed = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
        parsed = parsed * 10 + static_cast<uint32_t>(character - '0');
        if (parsed > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool ParseLocation(const std::string& source, std::string& fileName, uint32_t& line, uint32_t& column)
{
    const size_t columnSeparator = source.rfind(':');
    if (columnSeparator == std::string::npos || columnSeparator == 0) {
        return false;
    }
    const size_t lineSeparator = source.rfind(':', columnSeparator - 1);
    if (lineSeparator == std::string::npos || lineSeparator == 0) {
        return false;
    }
    if (!ParseUint32(source.substr(lineSeparator + 1, columnSeparator - lineSeparator - 1), line) ||
        !ParseUint32(source.substr(columnSeparator + 1), column)) {
        return false;
    }
    fileName = source.substr(0, lineSeparator);
    return !fileName.empty();
}

bool ReadFrames(const std::string& logPath, std::vector<CallStackFrame>& frames)
{
    std::ifstream input(logPath);
    if (!input) {
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    if (lines.empty() || lines.size() % 2 != 0) {
        return false;
    }

    frames.clear();
    frames.reserve(lines.size() / 2);
    for (size_t index = 0; index < lines.size(); index += 2) {
        if (lines[index] == "??" || lines[index + 1].find("??") == 0) {
            frames.clear();
            return false;
        }
        CallStackFrame frame;
        frame.functionName = lines[index];
        if (!ParseLocation(lines[index + 1], frame.fileName, frame.line, frame.column)) {
            frames.clear();
            return false;
        }
        frame.inlineDepth = static_cast<uint32_t>(frames.size());
        frames.push_back(std::move(frame));
    }
    return !frames.empty();
}

CallStackResult Unavailable(uint64_t pc, std::string error)
{
    CallStackResult result;
    result.pc = pc;
    result.error = std::move(error);
    return result;
}

} // namespace

DeviceSymbolizer::DeviceSymbolizer(DeviceSymbolizerConfig config) : config_(std::move(config)) {}

CallStackResult DeviceSymbolizer::ResolveCallStack(uint64_t pc) const
{
    return ResolveCallStackWithRunner(pc, RunCommand);
}

CallStackResult DeviceSymbolizer::ResolveCallStackWithRunner(uint64_t pc, const CommandRunner& runner) const
{
    if (config_.symbolizer.empty() || config_.image.empty() || config_.workDirectory.empty() || !runner) {
        return Unavailable(pc, "invalid_configuration");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const FrameCacheKey key{config_.image, pc};
    const auto cached = cache_.find(key);
    if (cached != cache_.end()) {
        return cached->second;
    }

    const std::string pcText = HexPc(pc);
    const std::string logPath = config_.workDirectory + "/symbolizer_" + pcText + ".log";
    const std::vector<std::string> command{config_.symbolizer, "--obj=" + config_.image, "--inlines",
                                           "--demangle",       "--functions=short",      pcText};

    std::string error;
    CallStackResult result;
    result.pc = pc;
    if (!runner(command, logPath, error)) {
        result.error = NormalizeError(error);
    } else if (!ReadFrames(logPath, result.frames)) {
        result.error = "invalid_symbolizer_output";
    } else {
        result.available = true;
    }
    cache_.emplace(key, result);
    return result;
}

void DeviceSymbolizer::Reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

} // namespace aclsan::probe
