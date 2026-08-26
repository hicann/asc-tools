// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.
#include "acl_hook.h"

#include <dlfcn.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <cstdio>
#include <sstream>
#include <utility>

namespace aclsan {
namespace {

using AclrtApiFunc = int (*)(void);
using InjectionGet = aclError (*)(const char*, AclrtApiFunc*, AclrtApiFunc*);
using InjectionSet = aclError (*)(const char*, AclrtApiFunc);

constexpr const char* kDataName = "aclrtBinaryLoadFromData";
constexpr unsigned long kMaxCompilerArgs = 128;
std::mutex g_hookMutex;
OriginalDataLoad g_originalData = nullptr;
AclHookConfig g_config{};
bool g_dataHookInstalled = false;
thread_local bool g_inHook = false;
std::atomic<uint64_t> g_requestId{0};

template <typename Function>
AclrtApiFunc AsGeneric(Function function)
{
    static_assert(sizeof(Function) == sizeof(AclrtApiFunc));
    AclrtApiFunc generic = nullptr;
    std::memcpy(&generic, &function, sizeof(generic));
    return generic;
}

template <typename Function>
Function FromGeneric(AclrtApiFunc function)
{
    static_assert(sizeof(Function) == sizeof(AclrtApiFunc));
    Function typed = nullptr;
    std::memcpy(&typed, &function, sizeof(typed));
    return typed;
}

InjectionGet ResolveInjectionGet()
{
    return reinterpret_cast<InjectionGet>(dlsym(RTLD_DEFAULT, "aclrtApiInjectionGetFunc"));
}

InjectionSet ResolveInjectionSet()
{
    return reinterpret_cast<InjectionSet>(dlsym(RTLD_DEFAULT, "aclrtApiInjectionSetFunc"));
}

std::string Env(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

bool StrictModeEnabled() noexcept
{
    const char* value = std::getenv("NPU_CHECK_DBI_STRICT");
    return value != nullptr && std::strcmp(value, "1") == 0;
}

std::string DefaultSourceRoot()
{
    const std::string configured = Env("NPU_CHECK_DBI_SOURCE_ROOT");
    if (!configured.empty()) {
        return configured;
    }
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&InstallAclHooks), &info) != 0 && info.dli_fname != nullptr) {
        const auto lib = std::filesystem::path(info.dli_fname);
        return (lib.parent_path().parent_path() / "share/aclsan/dbi").string();
    }
    return {};
}

std::string RequestDirectory(const AclHookConfig& config)
{
    const std::string root = config.workDirectory.empty() ? "/tmp" : config.workDirectory;
    std::ostringstream name;
    name << root << "/dbi-hook-" << static_cast<unsigned long long>(getpid()) << "-" << g_requestId.fetch_add(1);
    return name.str();
}

DbiRequest MakeRequest(
    const AclHookConfig& config, const std::string& input, const std::string& output, const std::string& work)
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

class HookGuard {
public:
    HookGuard() { g_inHook = true; }
    ~HookGuard() { g_inHook = false; }
};

bool ReadAll(const std::filesystem::path& path, std::vector<uint8_t>& data)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

void Cleanup(const AclHookConfig& config, const std::string& work)
{
    if (!config.keepTemp) {
        std::error_code error;
        std::filesystem::remove_all(work, error);
    }
}

void ReportPatchFailure(const DbiResult& result)
{
    std::cerr << "npu_check: DBI patch failed at " << result.stage;
    if (!result.diagnostic.empty()) {
        std::cerr << ": " << result.diagnostic;
    }
    std::cerr << '\n';
}

} // namespace

AclHookConfig DefaultHookConfig()
{
    AclHookConfig config{};
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
    return config;
}

AclHookConfig DefaultHookConfig(uint32_t probeGroupMask)
{
    AclHookConfig config = DefaultHookConfig();
    config.probeGroups = ProbeGroupsFromMask(probeGroupMask);
    return config;
}

bool IsBinaryLoadHookReentrant() noexcept { return g_inHook; }

aclError HandleBinaryLoadFromData(
    const AclHookConfig& config, const void* data, size_t length, const aclrtBinaryLoadOptions* options,
    aclrtBinHandle* handle, OriginalDataLoad original, PatchRunner runner, void* runnerData, bool* loadedPatched)
{
    if (loadedPatched != nullptr) {
        *loadedPatched = false;
    }
    if (original == nullptr || data == nullptr || length == 0 || g_inHook || runner == nullptr || config.arch.empty() ||
        config.argSize == 0 || config.probeGroups.empty()) {
        return original == nullptr ? ACL_ERROR_UNINITIALIZE : original(data, length, options, handle);
    }
    HookGuard guard;
    std::string work;
    std::vector<uint8_t> patched;
    bool patchReady = false;
    try {
        work = RequestDirectory(config);
        const std::string input = work + "/input.o";
        const std::string output = work + "/patched.o";
        std::error_code error;
        std::filesystem::create_directories(work, error);
        std::ofstream file(input, std::ios::binary | std::ios::trunc);
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
        if (file.good()) {
            file.close();
            const DbiRequest request = MakeRequest(config, input, output, work);
            const DbiResult result = runner(request, runnerData);
            if (result.success) {
                patchReady = ReadAll(result.patchedPath, patched);
            } else {
                ReportPatchFailure(result);
            }
        }
    } catch (...) {
        patchReady = false;
    }
    if (!patchReady) {
        Cleanup(config, work);
        return config.strict ? ACL_ERROR_FAILURE : original(data, length, options, handle);
    }
    const aclError status = original(patched.data(), patched.size(), options, handle);
    if (status == ACL_SUCCESS && loadedPatched != nullptr) {
        *loadedPatched = true;
    }
    Cleanup(config, work);
    return status;
}

aclError HandleBinaryLoadFromDataWithDefaultConfig(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* handle,
    OriginalDataLoad original, uint32_t probeGroupMask, bool* loadedPatched) noexcept
{
    if (loadedPatched != nullptr) {
        *loadedPatched = false;
    }
    if (original == nullptr) {
        return ACL_ERROR_UNINITIALIZE;
    }
    const bool strict = StrictModeEnabled();
    try {
        return HandleBinaryLoadFromData(
            DefaultHookConfig(probeGroupMask), data, length, options, handle, original, &RunPipeline, nullptr,
            loadedPatched);
    } catch (...) {
        return strict ? ACL_ERROR_FAILURE : original(data, length, options, handle);
    }
}

extern "C" aclError AclrtBinaryLoadFromDataHook(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* handle)
{
    AclHookConfig config;
    OriginalDataLoad original = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_hookMutex);
        config = g_config;
        original = g_originalData;
    }
    return HandleBinaryLoadFromData(config, data, length, options, handle, original, &RunPipeline, nullptr);
}

bool InstallAclHooks(const AclHookConfig& config, std::string& error)
{
    std::lock_guard<std::mutex> lock(g_hookMutex);
    if (g_dataHookInstalled) {
        error = "ACL DBI hooks are already installed or pending restoration";
        return false;
    }
    const InjectionGet getFunction = ResolveInjectionGet();
    const InjectionSet setFunction = ResolveInjectionSet();
    if (getFunction == nullptr || setFunction == nullptr) {
        error = "ACL injection SPI is unavailable";
        return false;
    }
    AclrtApiFunc origin = nullptr;
    AclrtApiFunc current = nullptr;
    if (getFunction(kDataName, &origin, &current) != ACL_SUCCESS || origin == nullptr || current == nullptr) {
        error = "cannot resolve " + std::string(kDataName) + " through ACL injection SPI";
        return false;
    }
    g_originalData = FromGeneric<OriginalDataLoad>(current);
    if (setFunction(kDataName, AsGeneric(&AclrtBinaryLoadFromDataHook)) != ACL_SUCCESS) {
        error = "cannot install " + std::string(kDataName) + " hook";
        g_originalData = nullptr;
        return false;
    }
    g_dataHookInstalled = true;
    g_config = config;
    error.clear();
    return true;
}

bool UninstallAclHooks()
{
    std::lock_guard<std::mutex> lock(g_hookMutex);
    const InjectionSet setFunction = ResolveInjectionSet();
    if (setFunction != nullptr && g_dataHookInstalled && g_originalData != nullptr &&
        setFunction(kDataName, AsGeneric(g_originalData)) == ACL_SUCCESS) {
        g_dataHookInstalled = false;
        g_originalData = nullptr;
    }
    if (!g_dataHookInstalled) {
        g_config = {};
        return true;
    } else {
        std::cerr << "npu_check: failed to restore one or more ACL DBI hooks\n";
        return false;
    }
}

} // namespace aclsan

extern "C" int NpuCheckInstallAclHooks(
    const aclsan::AclHookInstallOptions* options, char* errorBuffer, size_t errorCapacity)
{
    auto setError = [errorBuffer, errorCapacity](const char* message) {
        if (errorBuffer != nullptr && errorCapacity != 0) {
            std::snprintf(errorBuffer, errorCapacity, "%s", message);
        }
    };
    if (options == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (options->arch == nullptr || options->arch[0] == '\0' || options->argSize == 0 || options->probeGroupMask == 0 ||
        (options->probeGroupMask & ~aclsan::PROBE_GROUP_ALL) != 0) {
        setError("ACL DBI hook requires architecture, argument size, and a Probe group");
        return ACL_ERROR_INVALID_PARAM;
    }
    if (options->compilerArgCount != 0 && options->compilerArgs == nullptr) {
        setError("compiler argument array is null");
        return ACL_ERROR_INVALID_PARAM;
    }
    aclsan::AclHookConfig config{};
    config.arch = options->arch == nullptr ? "" : options->arch;
    config.argSize = options->argSize;
    config.toolchainRoot = options->toolchainRoot == nullptr ? "" : options->toolchainRoot;
    config.sourceRoot = options->sourceRoot == nullptr ? "" : options->sourceRoot;
    config.workDirectory = options->workDirectory == nullptr ? "" : options->workDirectory;
    config.cacheDirectory = options->cacheDirectory == nullptr ? "" : options->cacheDirectory;
    config.strict = options->strict != 0;
    config.keepTemp = options->keepTemp != 0;
    config.probeGroups = aclsan::ProbeGroupsFromMask(options->probeGroupMask);
    for (size_t index = 0; index < options->compilerArgCount; ++index) {
        if (options->compilerArgs[index] != nullptr) {
            config.compilerArgs.emplace_back(options->compilerArgs[index]);
        }
    }
    std::string error;
    if (aclsan::InstallAclHooks(config, error)) {
        return ACL_SUCCESS;
    }
    setError(error.c_str());
    return ACL_ERROR_FAILURE;
}

extern "C" int NpuCheckUninstallAclHooks() { return aclsan::UninstallAclHooks() ? ACL_SUCCESS : ACL_ERROR_FAILURE; }
