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

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <sys/stat.h>
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

// 解析十进制整数并做闭区间值域校验。内部验证选项与对外选项走同一套流程，
// 值域校验同样在解析阶段完成，越界即报用法错误，不留到运行期。
std::optional<int> ParseBoundedInt(const std::string& text, long low, long high)
{
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || value < low || value > high) {
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

// 注入库文件名。CLI 只负责定位它并把规范化绝对路径写进 ACL_API_INJECTION，
// 真正的加载由 ACL Runtime 在目标进程里完成。
constexpr const char* kInjectionLibraryName = "libnpu_check.so";

// CANN 安装根目录。
//
// 必须是 ASCEND_TOOLKIT_HOME —— CANN 的 set_env.sh 导出的是这个名字，并不存在
// ASCEND_TOOLKIT_PATH。用错名字的后果不是"找不到"而是更糟：取到空串后拼接出的
// "/lib64/libnpu_check.so" 是一个宿主机上的绝对路径，查找会静默落到系统目录里去。
constexpr const char* kAscendToolkitHomeEnv = "ASCEND_TOOLKIT_HOME";

// 定位覆盖入口，仅供测试与问题定位使用，不对外承诺兼容。
constexpr const char* kLibraryPathOverrideEnv = "NPU_CHECK_LIBRARY_PATH";

// 注入库会被加载进目标进程并以目标进程的权限运行。组可写或其他人可写意味着本用户
// 之外的人能替换它的内容，等于把任意代码执行的入口交出去，因此一律拒绝而不是警告。
bool IsSafelyOwned(const std::filesystem::path& path, std::string& reason)
{
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) {
        reason = "cannot stat '" + path.string() + "': " + std::strerror(errno);
        return false;
    }
    if ((info.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        reason = "'" + path.string() + "' is group- or world-writable and cannot be injected";
        return false;
    }
    return true;
}

} // namespace

bool ParseOptions(int argc, char** argv, Options& options, std::string& error)
{
    options = {};

    // --tool 显式指定的集合。std::set 天然去重（重复指定同一工具即幂等）且按 toolId
    // 升序，正好是下发前要求的规范化顺序，不必再单独排序去重。
    std::set<ipc::ToolId> explicitTools;
    // 已出现的工具子选项。std::map 按 optionId 升序，同样直接满足规范化要求；
    // 同一子选项重复出现按幂等处理。
    std::map<ipc::OptionId, const ipc::OptionRegistryEntry*> seenOptions;
    int applicationStart = -1;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--") {
            applicationStart = i + 1;
            break;
        }
        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return true;
        }
        if (argument.rfind('-', 0) != 0) {
            // 第一个不以 '-' 开头的参数即应用区域起点，其后参数一律原样交给被测程序，
            // 不再由 CLI 解析。因此 "--" 是可选的：写与不写解析出的 Options 完全相同。
            applicationStart = i;
            break;
        }

        std::string value;
        if (argument == "--tool") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            ipc::ToolId toolId{};
            if (!ipc::LookupTool(value, toolId)) {
                error = "unknown tool '" + value + "'; supported tools are memcheck and synccheck";
                return false;
            }
            explicitTools.insert(toolId);
            continue;
        }
        if (argument == "--log-file") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            options.logFile = AbsolutePath(value);
            continue;
        }
        if (argument == "--work-dir") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            // 转绝对路径：这个值要跨 fork 传给注入库，而子进程的当前目录不保证与 CLI
            // 相同，相对路径会在两侧解析到不同位置。
            options.workDir = AbsolutePath(value);
            continue;
        }
        if (argument == "--handshake-timeout-ms") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            const auto parsed = ParseBoundedInt(value, 100, 120000);
            if (!parsed) {
                error = "--handshake-timeout-ms must be in [100, 120000]";
                return false;
            }
            options.handshakeTimeoutMs = *parsed;
            continue;
        }
        if (argument == "--error-exitcode") {
            if (!NeedValue(argc, argv, i, value, error)) {
                return false;
            }
            const auto parsed = ParseBoundedInt(value, 1, 255);
            if (!parsed) {
                error = "--error-exitcode must be in [1, 255]";
                return false;
            }
            options.errorExitCode = *parsed;
            continue;
        }
        // 工具子选项：名称在 CLI 中全局唯一，归属由共享注册表决定，因此可以出现在
        // 所属 --tool 之前或之后，这里不依赖"当前工具"状态，也不按参数相邻关系推断。
        if (argument.rfind("--", 0) == 0 && argument.size() > 2) {
            if (const auto* entry = ipc::LookupOption(argument.substr(2)); entry != nullptr) {
                seenOptions[entry->optionId] = entry;
                continue;
            }
        }
        error = "unknown option: " + argument;
        return false;
    }

    // 完全没有出现 --tool 时工具集合取默认值 {memcheck}；只要出现过任意一个 --tool，
    // 默认值即不生效，不与显式指定的工具做并集 —— 否则用户没法把默认工具关掉。
    std::set<ipc::ToolId> enabledTools = explicitTools;
    if (enabledTools.empty()) {
        enabledTools.insert(ipc::ToolId::kMemcheck);
    }

    // 子选项的依赖校验必须在默认值生效之后进行：否则只写 --check-cache-control 而不写
    // --tool memcheck 时，会因为此刻工具集合还是空的而被误判为"所属工具未启用"。
    for (const auto& [optionId, entry] : seenOptions) {
        (void)optionId;
        if (enabledTools.count(entry->toolId) == 0) {
            error = std::string("--") + entry->name + " belongs to tool '" + ipc::ToolName(entry->toolId) +
                    "', which is not enabled";
            return false;
        }
    }

    // 规范化编码唯一：tools 按 toolId 升序，每个工具内 options 按 optionId 升序，均不重复。
    for (const ipc::ToolId toolId : enabledTools) {
        ipc::ToolRequest request;
        request.toolId = toolId;
        for (const auto& [optionId, entry] : seenOptions) {
            if (entry->toolId != toolId) {
                continue;
            }
            ipc::OptionValue optionValue;
            optionValue.optionId = optionId;
            // 布尔类子选项是"出现即为真"的开关，缺省时不发送 OptionValue。
            optionValue.value.assign(entry->valueSize, entry->presentValue);
            request.options.push_back(std::move(optionValue));
        }
        options.tools.push_back(std::move(request));
    }

    if (applicationStart < 0 || applicationStart >= argc) {
        error = "expected an application command";
        return false;
    }
    for (int i = applicationStart; i < argc; ++i) {
        options.application.emplace_back(argv[i]);
    }
    return true;
}

bool ResolveLibraryPath(const std::string& requested, std::string& resolved, std::string& error)
{
    std::vector<std::filesystem::path> candidates;
    if (!requested.empty()) {
        candidates.emplace_back(requested);
    } else if (const char* environment = std::getenv(kLibraryPathOverrideEnv);
               environment != nullptr && environment[0] != '\0') {
        candidates.emplace_back(environment);
    } else {
        // 顺序即优先级，两组候选各自解决不同的部署形态。
        //
        // 一、CANN 安装树。打包落地后这是正常路径。
        if (const char* toolkitHome = std::getenv(kAscendToolkitHomeEnv);
            toolkitHome != nullptr && toolkitHome[0] != '\0') {
            const std::filesystem::path root(toolkitHome);
            candidates.push_back(root / "lib64" / kInjectionLibraryName);
            candidates.push_back(root / "lib" / kInjectionLibraryName);
        }
        // 二、相对可执行文件自身。覆盖两种布局：构建产物同目录（demo 与开发树），
        //     以及安装树的 bin/ + lib{,64}/。
        //
        //     这一组不是临时兜底。/proc/self/exe 已经解开符号链接，因此从 PATH 调用、
        //     或经软链调用都能定位到真实安装位置；相比读环境变量，它不会因为用户忘了
        //     source set_env.sh、或环境里残留着另一个版本的路径而指错地方。
        //     在 libnpu_check.so 尚未进入 CANN 包之前，实际生效的也是这一组。
        std::array<char, PATH_MAX + 1> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), PATH_MAX);
        if (length > 0 && length < PATH_MAX) {
            executable[static_cast<size_t>(length)] = '\0';
            const auto directory = std::filesystem::path(executable.data()).parent_path();
            candidates.push_back(directory / kInjectionLibraryName);
            candidates.push_back(directory / ".." / "lib64" / kInjectionLibraryName);
            candidates.push_back(directory / ".." / "lib" / kInjectionLibraryName);
        }
    }

    // 找到了文件但权限不合格，与"压根没找到"是两种完全不同的故障，诊断必须分开报，
    // 否则用户会一直去检查路径而想不到是文件模式的问题。
    std::string rejection;
    for (const auto& candidate : candidates) {
        std::error_code filesystemError;
        const auto canonical = std::filesystem::canonical(candidate, filesystemError);
        if (filesystemError || !IsRegularFile(canonical)) {
            continue;
        }
        std::string reason;
        if (!IsSafelyOwned(canonical, reason)) {
            if (rejection.empty()) {
                rejection = reason;
            }
            continue;
        }
        resolved = canonical.string();
        return true;
    }
    if (!rejection.empty()) {
        error = "refusing to inject " + std::string(kInjectionLibraryName) + ": " + rejection;
        return false;
    }
    error = "cannot locate " + std::string(kInjectionLibraryName) + "; searched " + std::string(kAscendToolkitHomeEnv) +
            "/lib64, " + kAscendToolkitHomeEnv + "/lib and the directory of npu_check";
    return false;
}

std::string Usage()
{
    // 只列对外命令行契约：--tool、--log-file、--help/-h 以及 -- 边界规则。
    // 内部验证选项（--handshake-timeout-ms、--error-exitcode）不对外承诺兼容性，
    // 不得出现在这里。
    return "Usage: npu_check [--tool <name>]... [--log-file <path>] [--work-dir <path>]\n"
           "                 [--] <application> [args...]\n"
           "Options:\n"
           "  --tool <memcheck|synccheck>  enable a checker; repeatable and idempotent.\n"
           "                               Defaults to memcheck when no --tool is given.\n"
           "  --log-file <path>            directory or file receiving the report and\n"
           "                               the application output\n"
           "  --work-dir <path>            directory for npu_check.log and the probe cache.\n"
           "                               Created when missing and never removed.\n"
           "                               Defaults to a temporary directory.\n"
           "  -h, --help                   show this help and exit\n"
           "\n"
           "Pass -- before <application> when the application path or its arguments start\n"
           "with '-'.\n";
}

} // namespace npu::sanitizer::cli
