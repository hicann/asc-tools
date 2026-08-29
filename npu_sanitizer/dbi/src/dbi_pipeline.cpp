// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi_pipeline.h"

#include "ctrlbin_generator.h"
#include "tool_runner.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <system_error>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace aclsan {
namespace {

constexpr const char* kToolNames[] = {"bisheng", "bisheng-tune", "ld.lld", "llvm-objdump"};
std::atomic<uint64_t> g_cacheBuildId{0};

class CacheLock {
public:
    CacheLock() = default;
    ~CacheLock() { Release(); }
    CacheLock(const CacheLock&) = delete;
    CacheLock& operator=(const CacheLock&) = delete;

    void Release()
    {
        if (descriptor_ >= 0) {
            (void)flock(descriptor_, LOCK_UN);
            (void)close(descriptor_);
            descriptor_ = -1;
        }
    }

    bool Acquire(const std::filesystem::path& path, std::string& diagnostic)
    {
        descriptor_ = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (descriptor_ < 0) {
            diagnostic = "cannot open cache lock " + path.string() + ": " + std::strerror(errno);
            return false;
        }
        if (flock(descriptor_, LOCK_EX) != 0) {
            diagnostic = "cannot acquire cache lock " + path.string() + ": " + std::strerror(errno);
            (void)close(descriptor_);
            descriptor_ = -1;
            return false;
        }
        return true;
    }

private:
    int descriptor_ = -1;
};

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(std::filesystem::path path) : path_(std::move(path)) {}
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

private:
    std::filesystem::path path_;
};

class TemporaryFile {
public:
    explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path)) {}
    ~TemporaryFile()
    {
        if (!released_) {
            std::error_code error;
            std::filesystem::remove(path_, error);
        }
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    void Release() { released_ = true; }

private:
    std::filesystem::path path_;
    bool released_ = false;
};

std::string EnvironmentValue(const std::map<std::string, std::string>& environment, const char* name)
{
    const auto it = environment.find(name);
    if (it != environment.end()) {
        return it->second;
    }
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string FindInDirectory(const std::filesystem::path& directory, const char* name)
{
    const auto candidate = directory / name;
    std::error_code error;
    return std::filesystem::is_regular_file(candidate, error) ? candidate.string() : std::string{};
}

std::string FindInPath(const std::string& path, const char* name)
{
    std::size_t begin = 0;
    while (begin <= path.size()) {
        const std::size_t end = path.find(':', begin);
        const std::string item = path.substr(begin, end == std::string::npos ? end : end - begin);
        if (!item.empty()) {
            const std::string resolved = FindInDirectory(item, name);
            if (!resolved.empty()) {
                return resolved;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return {};
}

uint64_t HashText(uint64_t hash, const std::string& text)
{
    constexpr uint64_t kPrime = 1099511628211ULL;
    for (const unsigned char value : text) {
        hash ^= value;
        hash *= kPrime;
    }
    hash ^= 0xffU;
    return hash * kPrime;
}

bool IsNonEmptyFile(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && std::filesystem::file_size(path, error) != 0;
}

bool IsExecutableFile(const std::string& path) { return !path.empty() && access(path.c_str(), X_OK) == 0; }

bool IsAscendcDevkit(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path / "asc/include/kernel_operator.h", error) &&
           std::filesystem::is_regular_file(
               path / "ascendc/include/highlevel_api/kernel_tiling/kernel_tiling.h", error);
}

std::filesystem::path FindAscendcDevkit(const std::string& bisheng)
{
    std::filesystem::path candidate = std::filesystem::path(bisheng).parent_path();
    for (int depth = 0; depth < 6 && !candidate.empty(); ++depth) {
        if (IsAscendcDevkit(candidate)) {
            return candidate;
        }
        std::error_code error;
        for (std::filesystem::directory_iterator it(candidate, error), end; !error && it != end; it.increment(error)) {
            if (it->is_directory(error) && IsAscendcDevkit(it->path())) {
                return it->path();
            }
        }
        const auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return {};
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return input ? std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()) : std::string{};
}

bool WriteFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

std::string ToolFailure(const std::vector<std::string>& arguments, const ToolResult& result)
{
    std::ostringstream output;
    output << arguments.front() << " exited with " << result.exitCode;
    if (!result.standardError.empty()) {
        output << ": " << result.standardError;
    } else if (!result.standardOutput.empty()) {
        output << ": " << result.standardOutput;
    }
    return output.str();
}

bool RunChecked(
    const std::string& stage, const std::vector<std::string>& arguments, DbiResult& result,
    std::string* standardOutput = nullptr)
{
    const ToolResult toolResult = RunTool(arguments);
    if (standardOutput != nullptr) {
        *standardOutput = toolResult.standardOutput;
    }
    if (toolResult.exitCode == 0) {
        return true;
    }
    result.stage = stage;
    result.diagnostic = ToolFailure(arguments, toolResult);
    return false;
}

std::string ParseFirstTextSymbol(const std::string& text, const std::string& property)
{
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        std::vector<std::string> items{
            std::istream_iterator<std::string>(fields), std::istream_iterator<std::string>()};
        const bool textSection = items.size() > 3 && (items[3] == ".text" || items[3].compare(0, 6, ".text.") == 0);
        if (items.size() > 5 && items[1] == property && items[2] == "F" && textSection) {
            return items[5];
        }
    }
    return {};
}

std::vector<std::string> ProbeCompileFlags(const std::string& arch, const std::filesystem::path& ascendcDevkit)
{
    std::vector<std::string> flags{
        "-xcce",
        "-std=c++17",
        "-O2",
        "--cce-aicore-only",
        "--cce-aicore-arch=" + arch,
        "-mllvm",
        "-cce-aicore-record-overflow=false",
        "-mllvm",
        "-cce-aicore-record-stack-size=false"};
    if (arch.find("c220") != std::string::npos) {
        flags.insert(
            flags.end(),
            {"-mllvm", "-cce-aicore-addr-transform", "-mllvm", "-cce-aicore-long-call", "-mllvm", "-cce-aicore-relax"});
    } else {
        flags.insert(
            flags.end(),
            {"-fno-jump-tables", "-mllvm", "-cce-aicore-merge-function=false", "-mllvm", "-cce-aicore-addr-transform"});
    }
    flags.insert(
        flags.end(),
        {"-fPIC", "-pthread", "-DBUILD_DYNAMIC_PROBE", "-DTILING_KEY_VAR=0", "-I", (ascendcDevkit / "asc").string(),
         "-I", (ascendcDevkit / "asc/include").string(), "-I", (ascendcDevkit / "asc/include/basic_api").string(), "-I",
         (ascendcDevkit / "ascendc/include/highlevel_api").string()});
    return flags;
}

std::string SourceDigest(const DbiRequest& request, const std::vector<ProbeGroup>& groups)
{
    uint64_t hash = 1469598103934665603ULL;
    for (const auto group : groups) {
        const auto path = std::filesystem::path(request.sourceRoot) / "probes" / ProbeSourceName(group);
        hash = HashText(hash, path.string());
        hash = HashText(hash, ReadFile(path));
    }
    for (const char* header : {"trace_record.h", "trace_buffer_abi.h"}) {
        const auto path = std::filesystem::path(request.sourceRoot) / header;
        hash = HashText(hash, path.string());
        hash = HashText(hash, ReadFile(path));
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

} // namespace

bool ToolchainPaths::Complete() const
{
    return IsExecutableFile(bisheng) && IsExecutableFile(bishengTune) && IsExecutableFile(ldLld) &&
           IsExecutableFile(llvmObjdump);
}

std::vector<ProbeGroup> NormalizeProbeGroups(const std::vector<ProbeGroup>& groups)
{
    std::set<ProbeGroup> unique(groups.begin(), groups.end());
    // MTE2 memory operations consume configuration written by scalar SET_* instructions.
    if (unique.find(ProbeGroup::Mte2) != unique.end()) {
        unique.insert(ProbeGroup::Scalar);
    }
    return {unique.begin(), unique.end()};
}

std::vector<ProbeGroup> ProbeGroupsFromMask(uint32_t mask)
{
    const std::pair<uint32_t, ProbeGroup> groups[] = {
        {PROBE_GROUP_MTE1, ProbeGroup::Mte1},     {PROBE_GROUP_MTE2, ProbeGroup::Mte2},
        {PROBE_GROUP_MTE3, ProbeGroup::Mte3},     {PROBE_GROUP_FIXPIPE, ProbeGroup::Fixpipe},
        {PROBE_GROUP_SCALAR, ProbeGroup::Scalar}, {PROBE_GROUP_SYNC, ProbeGroup::Sync},
    };
    std::vector<ProbeGroup> selected;
    for (const auto& group : groups) {
        if ((mask & group.first) != 0) {
            selected.push_back(group.second);
        }
    }
    return NormalizeProbeGroups(selected);
}

std::string ProbeGroupName(ProbeGroup group)
{
    switch (group) {
        case ProbeGroup::Mte1:
            return "mte1";
        case ProbeGroup::Mte2:
            return "mte2";
        case ProbeGroup::Mte3:
            return "mte3";
        case ProbeGroup::Fixpipe:
            return "fixpipe";
        case ProbeGroup::Scalar:
            return "scalar";
        case ProbeGroup::Sync:
            return "sync";
    }
    return {};
}

std::string ProbeSourceName(ProbeGroup group) { return ProbeGroupName(group) + ".cpp"; }

std::string ValidateRequest(const DbiRequest& request)
{
    if (request.inputKernel.empty()) {
        return "input kernel is empty";
    }
    if (request.outputKernel.empty()) {
        return "output kernel is empty";
    }
    if (request.arch.empty()) {
        return "architecture is empty";
    }
    if (NormalizeProbeGroups(request.probeGroups).empty()) {
        return "probe set is empty";
    }
    return {};
}

ToolchainPaths ResolveToolchain(const std::string& explicitRoot, const std::map<std::string, std::string>& environment)
{
    std::vector<std::filesystem::path> directories;
    if (!explicitRoot.empty()) {
        const std::filesystem::path root(explicitRoot);
        directories.push_back(root / "tools/bisheng_compiler/bin");
        directories.push_back(root / "bin");
        directories.push_back(root);
    }
    for (const char* variable : {"ASCEND_HOME_PATH", "ASCEND_CANN_PACKAGE_PATH"}) {
        const std::string root = EnvironmentValue(environment, variable);
        if (!root.empty()) {
            directories.push_back(std::filesystem::path(root) / "tools/bisheng_compiler/bin");
        }
    }

    std::string resolved[4];
    for (const auto& directory : directories) {
        for (std::size_t index = 0; index < 4; ++index) {
            if (resolved[index].empty()) {
                resolved[index] = FindInDirectory(directory, kToolNames[index]);
            }
        }
    }
    const std::string path = EnvironmentValue(environment, "PATH");
    for (std::size_t index = 0; index < 4; ++index) {
        if (resolved[index].empty()) {
            resolved[index] = FindInPath(path, kToolNames[index]);
        }
    }
    return {resolved[0], resolved[1], resolved[2], resolved[3]};
}

std::string MakeCacheKey(
    const std::string& arch, const std::vector<ProbeGroup>& groups, const std::vector<std::string>& compilerArgs,
    const std::string& sourceDigest)
{
    uint64_t hash = 1469598103934665603ULL;
    hash = HashText(hash, arch);
    for (const auto group : NormalizeProbeGroups(groups)) {
        hash = HashText(hash, ProbeGroupName(group));
    }
    for (const auto& argument : compilerArgs) {
        hash = HashText(hash, argument);
    }
    hash = HashText(hash, sourceDigest);
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string ComputeProbeSourceDigest(const DbiRequest& request, const std::vector<ProbeGroup>& groups)
{
    return SourceDigest(request, groups);
}

DbiResult RunDbiPipeline(const DbiRequest& request)
{
    // 校验调用方传入的参数，并确认所需的探针源码均存在且非空。
    DbiResult result{};
    result.stage = "validate";
    result.diagnostic = ValidateRequest(request);
    if (!result.diagnostic.empty()) {
        return result;
    }
    const auto groups = NormalizeProbeGroups(request.probeGroups);
    for (const auto group : groups) {
        const auto source = std::filesystem::path(request.sourceRoot) / "probes" / ProbeSourceName(group);
        if (!IsNonEmptyFile(source)) {
            result.stage = "source";
            result.diagnostic = "missing Probe source: " + source.string();
            return result;
        }
    }

    // 定位完整的毕昇工具链，以及编译探针源码所需的 AscendC 开发头文件。
    const ToolchainPaths tools = ResolveToolchain(request.toolchainRoot, {});
    if (!tools.Complete()) {
        result.stage = "toolchain";
        result.diagnostic = "bisheng toolchain is incomplete: bisheng=" + tools.bisheng +
                            " bisheng-tune=" + tools.bishengTune + " ld.lld=" + tools.ldLld +
                            " llvm-objdump=" + tools.llvmObjdump;
        return result;
    }
    const auto ascendcDevkit = FindAscendcDevkit(tools.bisheng);
    if (ascendcDevkit.empty()) {
        result.stage = "toolchain";
        result.diagnostic = "AscendC development headers are unavailable";
        return result;
    }
    // 准备本次流水线使用的工作目录和跨请求复用的缓存目录。
    std::error_code error;
    std::filesystem::create_directories(request.workDirectory, error);
    if (error) {
        result.stage = "work-directory";
        result.diagnostic = error.message();
        return result;
    }
    std::filesystem::create_directories(request.cacheDirectory, error);
    if (error) {
        result.stage = "cache-directory";
        result.diagnostic = error.message();
        return result;
    }

    // 将所有影响编译结果的输入纳入缓存键，并按缓存键加锁，防止并发请求读到未构建完成的文件。
    auto compilerFlags = ProbeCompileFlags(request.arch, ascendcDevkit);
    compilerFlags.insert(compilerFlags.end(), {"-I", request.sourceRoot});
    compilerFlags.insert(compilerFlags.end(), request.extraCompilerArgs.begin(), request.extraCompilerArgs.end());
    const std::string cacheKey =
        MakeCacheKey(request.arch, groups, compilerFlags, ComputeProbeSourceDigest(request, groups));
    const auto artifactDirectory = std::filesystem::path(request.cacheDirectory) / cacheKey;
    CacheLock cacheLock;
    if (!cacheLock.Acquire(std::filesystem::path(request.cacheDirectory) / (cacheKey + ".lock"), result.diagnostic)) {
        result.stage = "cache-lock";
        return result;
    }
    std::filesystem::create_directories(artifactDirectory, error);
    if (error) {
        result.stage = "cache-directory";
        result.diagnostic = error.message();
        return result;
    }
    const auto probeObject = artifactDirectory / "probe.o";
    const auto ctrlBin = artifactDirectory / "ctrl.bin";
    const auto stagingDirectory =
        artifactDirectory / (".build-" + std::to_string(static_cast<unsigned long long>(getpid())) + "-" +
                             std::to_string(g_cacheBuildId.fetch_add(1)));
    std::filesystem::create_directories(stagingDirectory, error);
    if (error) {
        result.stage = "cache-directory";
        result.diagnostic = error.message();
        return result;
    }
    TemporaryDirectory stagingCleanup(stagingDirectory);

    // 缓存未命中时，分别编译各探针组，再将它们链接成一个可重定位的 probe.o。
    // 所有构建均在临时目录中完成，成功后再通过重命名发布到共享缓存。

    // TODO: CLI得到的目录怎么透传下来，san.so里？默认放tmp，内部可使用？
    if (!IsNonEmptyFile(probeObject)) {
        std::vector<std::string> objectPaths;
        for (const auto group : groups) {
            const auto source = std::filesystem::path(request.sourceRoot) / "probes" / ProbeSourceName(group);
            const auto object = stagingDirectory / (ProbeGroupName(group) + ".o");
            std::vector<std::string> arguments{tools.bisheng};
            arguments.insert(arguments.end(), compilerFlags.begin(), compilerFlags.end());
            arguments.insert(arguments.end(), {"-c", source.string(), "-o", object.string()});
            if (!RunChecked("compile-probe", arguments, result) || !IsNonEmptyFile(object)) {
                if (result.diagnostic.empty()) {
                    result.stage = "compile-probe";
                    result.diagnostic = "bisheng did not create " + object.string();
                }
                return result;
            }
            objectPaths.push_back(object.string());
        }
        // 将各探针目标文件合并，保证后续只需向内核链接一个探针对象。
        const auto stagedProbeObject = stagingDirectory / "probe.o";
        std::vector<std::string> linkArguments{tools.ldLld, "-r"};
        linkArguments.insert(linkArguments.end(), objectPaths.begin(), objectPaths.end());
        linkArguments.insert(linkArguments.end(), {"-o", stagedProbeObject.string()});
        if (!RunChecked("link-probe", linkArguments, result) || !IsNonEmptyFile(stagedProbeObject)) {
            if (result.diagnostic.empty()) {
                result.stage = "link-probe";
                result.diagnostic = "ld.lld did not create " + stagedProbeObject.string();
            }
            return result;
        }
        std::filesystem::rename(stagedProbeObject, probeObject, error);
        if (error) {
            result.stage = "publish-cache";
            result.diagnostic = "cannot publish " + probeObject.string() + ": " + error.message();
            return result;
        }
    }
    // 生成 bisheng-tune 使用的探针绑定配置 ctrl.bin；它与 probe.o 共用缓存键，
    // 从而保证探针对象和控制数据始终对应同一组请求参数。
    if (!IsNonEmptyFile(ctrlBin)) {
        const auto stagedCtrlBin = stagingDirectory / "ctrl.bin";
        if (!GenerateCtrlBin(stagedCtrlBin.string(), groups, result.diagnostic)) {
            result.stage = "generate-ctrlbin";
            return result;
        }
        std::filesystem::rename(stagedCtrlBin, ctrlBin, error);
        if (error) {
            result.stage = "publish-cache";
            result.diagnostic = "cannot publish " + ctrlBin.string() + ": " + error.message();
            return result;
        }
    }
    // 缓存文件发布后不再修改，后续均为请求私有操作，因此提前释放缓存锁。
    cacheLock.Release();

    // 读取原始内核和探针对象的符号表，提取待合并的内核全局符号与探针弱符号。
    std::string kernelSymbols;
    if (!RunChecked("inspect-kernel", {tools.llvmObjdump, "--syms", request.inputKernel}, result, &kernelSymbols)) {
        return result;
    }
    std::string probeSymbols;
    if (!RunChecked("inspect-probe", {tools.llvmObjdump, "--syms", probeObject.string()}, result, &probeSymbols)) {
        return result;
    }
    const std::string kernelSymbol = ParseFirstTextSymbol(kernelSymbols, "g");
    const std::string probeSymbol = ParseFirstTextSymbol(probeSymbols, "w");
    if (kernelSymbol.empty() || probeSymbol.empty()) {
        result.stage = "symbol-ordering";
        result.diagnostic = "cannot identify kernel or Probe text symbol";
        return result;
    }
    // 生成符号排序文件，确保链接后原始内核正文位于探针入口之前。
    const auto orderingFile = std::filesystem::path(request.workDirectory) / "symbol_ordering.txt";
    if (!WriteFile(orderingFile, kernelSymbol + "\n" + probeSymbol)) {
        result.stage = "symbol-ordering";
        result.diagnostic = "cannot write " + orderingFile.string();
        return result;
    }
    // 按指定符号顺序将 probe.o 与原始内核链接，生成待插桩的中间内核对象。
    const auto requestBuildId = std::to_string(g_cacheBuildId.fetch_add(1));
    const auto mergedKernel = std::filesystem::path(request.workDirectory) / "kernel_with_probe.o";
    const auto stagedMergedKernel =
        std::filesystem::path(request.workDirectory) / (".kernel_with_probe-" + requestBuildId);
    TemporaryFile mergedCleanup(stagedMergedKernel);
    const std::vector<std::string> mergeArguments{
        tools.ldLld,
        "-m",
        "aicorelinux",
        "-Ttext=0",
        "-execute-probe",
        "--symbol-ordering-file",
        orderingFile.string(),
        "-z",
        "separate-loadable-segments",
        probeObject.string(),
        request.inputKernel,
        "-static",
        "-q",
        "-o",
        stagedMergedKernel.string()};
    if (!RunChecked("merge-kernel", mergeArguments, result) || !IsNonEmptyFile(stagedMergedKernel)) {
        if (result.diagnostic.empty()) {
            result.stage = "merge-kernel";
            result.diagnostic = "ld.lld did not create " + stagedMergedKernel.string();
        }
        return result;
    }
    std::filesystem::rename(stagedMergedKernel, mergedKernel, error);
    if (error) {
        result.stage = "publish-kernel";
        result.diagnostic = "cannot publish " + mergedKernel.string() + ": " + error.message();
        return result;
    }
    mergedCleanup.Release();

    // 使用 bisheng-tune 和 ctrl.bin 对合并后的内核执行 DBI 插桩。
    // 先输出到目标文件的临时同级文件，避免工具中途失败时暴露不完整的内核。
    const auto stagedOutput =
        std::filesystem::path(request.outputKernel).parent_path() /
        (".dbi-patched-" + std::to_string(static_cast<unsigned long long>(getpid())) + "-" + requestBuildId);
    TemporaryFile outputCleanup(stagedOutput);
    std::vector<std::string> tuneArguments{
        tools.bishengTune,
        "--action=instru-probe",
        "--instru-memprobe",
        mergedKernel.string(),
        "--dbi-config=" + ctrlBin.string(),
        "-o=" + stagedOutput.string()};
    tuneArguments.insert(tuneArguments.end(), request.extraTuneArgs.begin(), request.extraTuneArgs.end());
    if (!RunChecked("bisheng-tune", tuneArguments, result) || !IsNonEmptyFile(stagedOutput)) {
        if (result.diagnostic.empty()) {
            result.stage = "bisheng-tune";
            result.diagnostic = "bisheng-tune did not create " + stagedOutput.string();
        }
        return result;
    }
    // 插桩成功后原子发布最终内核，并填写流水线成功结果。
    std::filesystem::rename(stagedOutput, request.outputKernel, error);
    if (error) {
        result.stage = "publish-kernel";
        result.diagnostic = "cannot publish " + request.outputKernel + ": " + error.message();
        return result;
    }
    outputCleanup.Release();
    result.success = true;
    result.patchedPath = request.outputKernel;
    result.stage = "complete";
    result.diagnostic.clear();
    return result;
}

} // namespace aclsan
