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

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
    const std::string& work)
{
    DbiRequest request{};
    request.inputKernel = input;
    request.outputKernel = output;
    request.arch = config.arch;
    request.argSize = config.argSize;
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
    const std::string argSize = Env("NPU_CHECK_DBI_ARG_SIZE");
    if (!argSize.empty()) {
        char* end = nullptr;
        const unsigned long value = std::strtoul(argSize.c_str(), &end, 10);
        if (end != argSize.c_str() && *end == '\0' && value <= UINT32_MAX) {
            config.argSize = static_cast<uint32_t>(value);
        }
    }
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
    if (data == nullptr || length == 0 || runner == nullptr || config.arch.empty() || config.argSize == 0 ||
        config.probeGroups.empty()) {
        return {};
    }

    std::string work;
    try {
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

        const DbiResult pipeline = runner(MakeRequest(config, inputPath, outputPath, work), runnerData);
        if (!pipeline.success) {
            Cleanup(config, work);
            return {BinaryInstrumentationStatus::Failed, {}, pipeline.stage, pipeline.diagnostic};
        }

        std::vector<uint8_t> patched;
        if (!ReadAll(pipeline.patchedPath, patched)) {
            Cleanup(config, work);
            return {BinaryInstrumentationStatus::Failed, {}, "read-output", "cannot read patched binary"};
        }
        Cleanup(config, work);
        return {BinaryInstrumentationStatus::Instrumented, std::move(patched), {}, {}};
    } catch (const std::exception& error) {
        Cleanup(config, work);
        return {BinaryInstrumentationStatus::Failed, {}, "exception", error.what()};
    } catch (...) {
        Cleanup(config, work);
        return {BinaryInstrumentationStatus::Failed, {}, "exception", "unknown DBI instrumentation failure"};
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
            return {result.status, config.strict ? 1U : 0U, 0};
        }
        if (result.status == BinaryInstrumentationStatus::Skipped) {
            return {result.status, config.strict ? 1U : 0U, 0};
        }
        if (consumer == nullptr) {
            const BinaryInstrumentationResult failure{
                BinaryInstrumentationStatus::Failed, {}, "consume-output", "instrumented binary consumer is null"};
            ReportInstrumentationFailure(failure);
            return {failure.status, config.strict ? 1U : 0U, 0};
        }
        const int32_t consumerStatus = consumer(result.binary.data(), result.binary.size(), consumerData);
        return {result.status, config.strict ? 1U : 0U, consumerStatus};
    } catch (const std::exception& error) {
        const BinaryInstrumentationResult failure{BinaryInstrumentationStatus::Failed, {}, "exception", error.what()};
        ReportInstrumentationFailure(failure);
        return {failure.status, strict, 0};
    } catch (...) {
        const BinaryInstrumentationResult failure{
            BinaryInstrumentationStatus::Failed, {}, "exception", "unknown DBI instrumentation failure"};
        ReportInstrumentationFailure(failure);
        return {failure.status, strict, 0};
    }
}

RuntimeBinaryInstrumentationResult InstrumentRuntimeBinary(
    const void* data, size_t length, uint32_t probeGroupMask, InstrumentedBinaryConsumer consumer,
    void* consumerData) noexcept
{
    return InstrumentRuntimeBinary(data, length, probeGroupMask, consumer, consumerData, &RunPipeline, nullptr);
}

} // namespace aclsan
