// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#include "binary_instrumenter.h"

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

namespace aclsan {
namespace {

constexpr unsigned long kMaxCompilerArgs = 128;
std::atomic<uint64_t> g_requestId{0};

std::string Env(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string DefaultSourceRoot()
{
    const std::string configured = Env("NPU_CHECK_DBI_SOURCE_ROOT");
    if (!configured.empty()) {
        return configured;
    }
    Dl_info info{};
    const auto instrument =
        static_cast<BinaryInstrumentationResult (*)(const BinaryInstrumentationConfig&, const void*, size_t)>(
            &InstrumentBinary);
    if (dladdr(reinterpret_cast<void*>(instrument), &info) != 0 && info.dli_fname != nullptr) {
        const auto library = std::filesystem::path(info.dli_fname);
        return (library.parent_path().parent_path() / "share/aclsan/dbi").string();
    }
    return {};
}

std::string RequestDirectory(const BinaryInstrumentationConfig& config)
{
    const std::string root = config.workDirectory.empty() ? "/tmp" : config.workDirectory;
    std::ostringstream name;
    name << root << "/dbi-instrument-" << static_cast<unsigned long long>(getpid()) << "-" << g_requestId.fetch_add(1);
    return name.str();
}

DbiRequest MakeRequest(
    const BinaryInstrumentationConfig& config, const std::string& input, const std::string& output,
    const std::string& work, uint32_t traceArgumentOffset)
{
    DbiRequest request{};
    request.inputKernel = input;
    request.outputKernel = output;
    request.arch = config.arch;
    request.traceArgumentOffset = traceArgumentOffset;
    request.probeGroups = config.probeGroups;
    request.toolchainRoot = config.toolchainRoot;
    request.sourceRoot = config.sourceRoot.empty() ? DefaultSourceRoot() : config.sourceRoot;
    request.workDirectory = work;
    request.cacheDirectory = config.cacheDirectory.empty() ? work + "/probe-cache" : config.cacheDirectory;
    request.strict = config.strict;
    request.keepTemp = config.keepTemp;
    request.extraCompilerArgs = config.compilerArgs;
    request.extraTuneArgs = config.tuneArgs;
    return request;
}

bool IsFileRangeValid(size_t offset, size_t bytes, size_t fileBytes)
{
    return offset <= fileBytes && bytes <= fileBytes - offset;
}

bool ResolveTraceArgumentOffset(const void* data, size_t length, uint32_t& traceArgumentOffset, std::string& diagnostic)
{
    if (data == nullptr || length < sizeof(Elf64_Ehdr)) {
        diagnostic = "Device ELF header is missing";
        return false;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    Elf64_Ehdr header{};
    std::memcpy(&header, bytes, sizeof(header));
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_shentsize != sizeof(Elf64_Shdr) || header.e_shnum == 0 ||
        header.e_shstrndx >= header.e_shnum) {
        diagnostic = "Device ELF section table is unsupported";
        return false;
    }
    const size_t sectionTableBytes = static_cast<size_t>(header.e_shnum) * sizeof(Elf64_Shdr);
    if (!IsFileRangeValid(header.e_shoff, sectionTableBytes, length)) {
        diagnostic = "Device ELF section table is out of bounds";
        return false;
    }
    const auto ReadSection = [&](size_t index, Elf64_Shdr& section) {
        const size_t offset = static_cast<size_t>(header.e_shoff) + index * sizeof(Elf64_Shdr);
        std::memcpy(&section, bytes + offset, sizeof(section));
    };
    Elf64_Shdr sectionNames{};
    ReadSection(header.e_shstrndx, sectionNames);
    if (!IsFileRangeValid(sectionNames.sh_offset, sectionNames.sh_size, length)) {
        diagnostic = "Device ELF section-name table is out of bounds";
        return false;
    }
    const char* names = reinterpret_cast<const char*>(bytes + sectionNames.sh_offset);
    for (size_t index = 0; index < header.e_shnum; ++index) {
        Elf64_Shdr section{};
        ReadSection(index, section);
        if (section.sh_name >= sectionNames.sh_size) {
            continue;
        }
        const size_t nameBytes = sectionNames.sh_size - section.sh_name;
        const char* name = names + section.sh_name;
        if (std::memchr(name, '\0', nameBytes) == nullptr || std::strcmp(name, "__CCE_KernelArgSize") != 0) {
            continue;
        }
        if (section.sh_size == 0 || section.sh_size % sizeof(uint32_t) != 0 ||
            !IsFileRangeValid(section.sh_offset, section.sh_size, length)) {
            diagnostic = "__CCE_KernelArgSize is malformed";
            return false;
        }
        uint32_t maximumArgumentSize = 0;
        for (size_t offset = 0; offset < section.sh_size; offset += sizeof(uint32_t)) {
            uint32_t argumentSize = 0;
            std::memcpy(&argumentSize, bytes + section.sh_offset + offset, sizeof(argumentSize));
            maximumArgumentSize = std::max(maximumArgumentSize, argumentSize);
        }
        constexpr uint32_t kMinimumTraceArgumentOffset = sizeof(uint64_t);
        maximumArgumentSize = std::max(maximumArgumentSize, kMinimumTraceArgumentOffset);
        if (maximumArgumentSize > std::numeric_limits<uint32_t>::max() - 7U) {
            diagnostic = "__CCE_KernelArgSize cannot be aligned";
            return false;
        }
        traceArgumentOffset = (maximumArgumentSize + 7U) & ~7U;
        return true;
    }
    diagnostic = "__CCE_KernelArgSize is missing";
    return false;
}

DbiResult RunPipeline(const DbiRequest& request, void*) { return RunDbiPipeline(request); }

bool ReadAll(const std::filesystem::path& path, std::vector<uint8_t>& data)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

void Cleanup(const BinaryInstrumentationConfig& config, const std::string& work)
{
    if (!config.keepTemp && !work.empty()) {
        std::error_code error;
        std::filesystem::remove_all(work, error);
    }
}

void ReportInstrumentationFailure(const BinaryInstrumentationResult& result)
{
    std::cerr << "npu_check: DBI patch failed at " << result.stage;
    if (!result.diagnostic.empty()) {
        std::cerr << ": " << result.diagnostic;
    }
    std::cerr << '\n';
}

} // namespace

BinaryInstrumentationConfig DefaultBinaryInstrumentationConfig()
{
    BinaryInstrumentationConfig config{};
    config.arch = Env("NPU_CHECK_DBI_ARCH");
    config.toolchainRoot = Env("NPU_CHECK_DBI_TOOLCHAIN_ROOT");
    config.sourceRoot = Env("NPU_CHECK_DBI_SOURCE_ROOT");
    config.workDirectory = Env("NPU_CHECK_DBI_WORK_DIR");
    config.cacheDirectory = Env("NPU_CHECK_DBI_CACHE_DIR");
    config.strict = Env("NPU_CHECK_DBI_STRICT") == "1";
    config.keepTemp = Env("NPU_CHECK_DBI_KEEP_TEMP") == "1";
    const std::string groups = Env("NPU_CHECK_DBI_PROBE_SET");
    if (!groups.empty()) {
        std::istringstream input(groups);
        for (std::string group; std::getline(input, group, ',');) {
            if (group == "mte1")
                config.probeGroups.push_back(ProbeGroup::Mte1);
            else if (group == "mte2")
                config.probeGroups.push_back(ProbeGroup::Mte2);
            else if (group == "mte3")
                config.probeGroups.push_back(ProbeGroup::Mte3);
            else if (group == "fixpipe")
                config.probeGroups.push_back(ProbeGroup::Fixpipe);
            else if (group == "scalar")
                config.probeGroups.push_back(ProbeGroup::Scalar);
            else if (group == "sync")
                config.probeGroups.push_back(ProbeGroup::Sync);
        }
    }
    const std::string compilerArgCount = Env("NPU_CHECK_DBI_COMPILER_ARG_COUNT");
    if (!compilerArgCount.empty()) {
        char* end = nullptr;
        const unsigned long count = std::strtoul(compilerArgCount.c_str(), &end, 10);
        if (end != compilerArgCount.c_str() && *end == '\0' && count <= kMaxCompilerArgs) {
            for (unsigned long index = 0; index < count; ++index) {
                const std::string name = "NPU_CHECK_DBI_COMPILER_ARG_" + std::to_string(index);
                const char* value = std::getenv(name.c_str());
                if (value == nullptr) {
                    config.compilerArgs.clear();
                    break;
                }
                config.compilerArgs.emplace_back(value);
            }
        }
    }
    config.probeGroups = NormalizeProbeGroups(config.probeGroups);
    return config;
}

BinaryInstrumentationConfig DefaultBinaryInstrumentationConfig(uint32_t probeGroupMask)
{
    BinaryInstrumentationConfig config = DefaultBinaryInstrumentationConfig();
    config.probeGroups = ProbeGroupsFromMask(probeGroupMask);
    return config;
}

BinaryInstrumentationResult InstrumentBinary(
    const BinaryInstrumentationConfig& config, const void* data, size_t length, DbiPipelineRunner runner,
    void* runnerData)
{
    if (data == nullptr || length == 0 || runner == nullptr || config.arch.empty() || config.probeGroups.empty()) {
        return {};
    }

    std::string work;
    try {
        uint32_t traceArgumentOffset = config.traceArgumentOffset;
        if (traceArgumentOffset == 0) {
            std::string diagnostic;
            if (!ResolveTraceArgumentOffset(data, length, traceArgumentOffset, diagnostic)) {
                return {BinaryInstrumentationStatus::Failed, {}, "kernel-arg-offset", diagnostic, 0};
            }
        }
        work = RequestDirectory(config);
        const std::string inputPath = work + "/input.o";
        const std::string outputPath = work + "/patched.o";
        std::error_code error;
        std::filesystem::create_directories(work, error);
        if (error) {
            Cleanup(config, work);
            return {BinaryInstrumentationStatus::Failed, {}, "work-directory", error.message()};
        }

        std::ofstream input(inputPath, std::ios::binary | std::ios::trunc);
        input.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
        if (!input.good()) {
            Cleanup(config, work);
            return {BinaryInstrumentationStatus::Failed, {}, "write-input", "cannot write input binary"};
        }
        input.close();

        const DbiResult pipeline =
            runner(MakeRequest(config, inputPath, outputPath, work, traceArgumentOffset), runnerData);
        if (!pipeline.success) {
            Cleanup(config, work);
            return {BinaryInstrumentationStatus::Failed, {}, pipeline.stage, pipeline.diagnostic, 0};
        }

        std::vector<uint8_t> patched;
        if (!ReadAll(pipeline.patchedPath, patched)) {
            Cleanup(config, work);
            return {BinaryInstrumentationStatus::Failed, {}, "read-output", "cannot read patched binary", 0};
        }
        Cleanup(config, work);
        return {BinaryInstrumentationStatus::Instrumented, std::move(patched), {}, {}, traceArgumentOffset};
    } catch (const std::exception& error) {
        Cleanup(config, work);
        return {BinaryInstrumentationStatus::Failed, {}, "exception", error.what(), 0};
    } catch (...) {
        Cleanup(config, work);
        return {BinaryInstrumentationStatus::Failed, {}, "exception", "unknown DBI instrumentation failure", 0};
    }
}

BinaryInstrumentationResult InstrumentBinary(const BinaryInstrumentationConfig& config, const void* data, size_t length)
{
    return InstrumentBinary(config, data, length, &RunPipeline, nullptr);
}

RuntimeBinaryInstrumentationResult InstrumentRuntimeBinary(
    const void* data, size_t length, uint32_t probeGroupMask, InstrumentedBinaryConsumer consumer, void* consumerData,
    DbiPipelineRunner runner, void* runnerData) noexcept
{
    const char* strictText = std::getenv("NPU_CHECK_DBI_STRICT");
    const uint32_t strict = strictText != nullptr && std::strcmp(strictText, "1") == 0 ? 1U : 0U;
    try {
        const BinaryInstrumentationConfig config = DefaultBinaryInstrumentationConfig(probeGroupMask);
        const BinaryInstrumentationResult result = InstrumentBinary(config, data, length, runner, runnerData);
        if (result.status == BinaryInstrumentationStatus::Failed) {
            ReportInstrumentationFailure(result);
            return {result.status, config.strict ? 1U : 0U, 0, 0};
        }
        if (result.status == BinaryInstrumentationStatus::Skipped) {
            return {result.status, config.strict ? 1U : 0U, 0, 0};
        }
        if (consumer == nullptr) {
            const BinaryInstrumentationResult failure{
                BinaryInstrumentationStatus::Failed, {}, "consume-output", "instrumented binary consumer is null", 0};
            ReportInstrumentationFailure(failure);
            return {failure.status, config.strict ? 1U : 0U, 0, 0};
        }
        const int32_t consumerStatus = consumer(result.binary.data(), result.binary.size(), consumerData);
        return {result.status, config.strict ? 1U : 0U, consumerStatus, result.traceArgumentOffset};
    } catch (const std::exception& error) {
        const BinaryInstrumentationResult failure{
            BinaryInstrumentationStatus::Failed, {}, "exception", error.what(), 0};
        ReportInstrumentationFailure(failure);
        return {failure.status, strict, 0, 0};
    } catch (...) {
        const BinaryInstrumentationResult failure{
            BinaryInstrumentationStatus::Failed, {}, "exception", "unknown DBI instrumentation failure", 0};
        ReportInstrumentationFailure(failure);
        return {failure.status, strict, 0, 0};
    }
}

RuntimeBinaryInstrumentationResult InstrumentRuntimeBinary(
    const void* data, size_t length, uint32_t probeGroupMask, InstrumentedBinaryConsumer consumer,
    void* consumerData) noexcept
{
    return InstrumentRuntimeBinary(data, length, probeGroupMask, consumer, consumerData, &RunPipeline, nullptr);
}

} // namespace aclsan
