/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "binary_instrumenter.h"
#include "common/debug_log.h"

#include <cerrno>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <iterator>
#include <mutex>
#include <spawn.h>
#include <string>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace aclpti::profiling {
namespace {
namespace fs = boost::filesystem;

// Kernel-end probe source. It is embedded in the library instead of being packaged as a separate
// resource, written to the probe cache directory and compiled by Bisheng on first instrumentation.
// Keep this text in sync with the probe contract: bar.all, a fixed NOP/DFX_REGION sequence and the
// three implicit Bisheng probe ABI parameters (which the probe does not dereference).
constexpr char kKernelEndProbeSource[] =
    R"PROBE(extern __attribute__((noinline, weak))[aicore] void __npu_compute_before_kernel_end(
    __gm__ unsigned char*, unsigned long long, unsigned int)
{
    asm volatile("bar.all" ::: "memory");
    asm volatile(".rept 32\n\tnop\n\t.endr");
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xd88));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xd99));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdaa));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdbb));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdcc));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xddd));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdee));
    asm volatile("DFX_REGION.S %0" ::"l"(0xdff));
    asm volatile(".rept 3500\n\tnop\n\t.endr");
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdff));
}
)PROBE";

// Fixed version 0 control record binding instrId 397 to the probe symbol.
// Layout (little endian): header<I H H H H> | binding<H H H> | names<I size + symbol + 4 zero bytes>.
constexpr unsigned char kKernelEndControl[] = {
    0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x23,
    0x00, 0x00, 0x00, 0x5f, 0x5f, 0x6e, 0x70, 0x75, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x75, 0x74, 0x65, 0x5f, 0x62, 0x65,
    0x66, 0x6f, 0x72, 0x65, 0x5f, 0x6b, 0x65, 0x72, 0x6e, 0x65, 0x6c, 0x5f, 0x65, 0x6e, 0x64, 0x00, 0x00, 0x00, 0x00,
};

constexpr const char* kProbeSourceName = "probe.cpp";
constexpr const char* kProbeObjectName = "kernel_end.o";
constexpr const char* kControlRecordName = "ctrl.bin";

struct KernelEndTools {
    fs::path linker;   // ld.lld
    fs::path tuner;    // bisheng-tune
    fs::path compiler; // bisheng
};

bool WriteProbeFiles(const fs::path& directory)
{
    const auto write = [&](const char* name, const void* bytes, std::size_t size) {
        std::ofstream file((directory / name).string(), std::ios::binary);
        file.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
        file.close();
        return static_cast<bool>(file);
    };
    return write(kProbeSourceName, kKernelEndProbeSource, sizeof(kKernelEndProbeSource) - 1) &&
           write(kControlRecordName, kKernelEndControl, sizeof(kKernelEndControl));
}

bool Run(std::initializer_list<std::string> arguments)
{
    std::vector<char*> argv;
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    pid_t child = -1;
    const int error = posix_spawnp(&child, argv[0], nullptr, nullptr, argv.data(), environ);
    int status = 0;
    pid_t waited = -1;
    if (error == 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    if (error != 0 || waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        npucompute::detail::DebugLog("aclpti", "kernel-end tool failed: %s spawn=%d status=%d", argv[0], error, status);
        return false;
    }
    return true;
}

bool CompileProbe(const fs::path& directory, const fs::path& compiler)
{
    return Run(
        {compiler.string(), "-x", "cce", "--cce-aicore-only", "--npu-arch=dav-3510", "-O2", "-g", "-c",
         (directory / kProbeSourceName).string(), "-o", (directory / kProbeObjectName).string()});
}

bool FindTool(const fs::path& directory, const char* name, fs::path& tool)
{
    const fs::path candidate = directory / name;
    if (fs::is_regular_file(candidate)) {
        tool = candidate;
        return true;
    }
    return false;
}

bool ResolveTools(KernelEndTools& tools)
{
    const char* cann = std::getenv("ASCEND_HOME_PATH");
    if (cann == nullptr || cann[0] == '\0') {
        npucompute::detail::DebugLog("aclpti", "kernel-end requires ASCEND_HOME_PATH");
        return false;
    }
    const fs::path root(cann);
    std::vector<fs::path> directories = {
        root / "tools/bisheng_compiler/bin",
        root.parent_path() / "tools/bisheng_compiler/bin",
        root / "ccec_compiler/bin",
    };
    struct utsname machine {};
    if (uname(&machine) == 0) {
        directories.push_back(root / (std::string(machine.machine) + "-linux/ccec_compiler/bin"));
    }
    const char* override = std::getenv("NPU_COMPUTE_BISHENG");
    if (override != nullptr && override[0] != '\0') {
        tools.compiler = override;
    }
    for (const auto& directory : directories) {
        if (tools.linker.empty()) {
            FindTool(directory, "ld.lld", tools.linker);
        }
        if (tools.tuner.empty()) {
            FindTool(directory, "bisheng-tune", tools.tuner);
        }
        if (tools.compiler.empty()) {
            FindTool(directory, "bisheng", tools.compiler);
        }
    }
    if (tools.linker.empty() || tools.tuner.empty()) {
        npucompute::detail::DebugLog("aclpti", "kernel-end requires ld.lld and bisheng-tune under %s", cann);
        return false;
    }
    if (tools.compiler.empty()) {
        // Fall back to PATH; posix_spawnp resolves the bare name.
        tools.compiler = "bisheng";
    }
    return true;
}

struct TemporaryDirectory {
    fs::path path;
    ~TemporaryDirectory()
    {
        boost::system::error_code error;
        fs::remove_all(path, error);
    }
};

bool PrepareProbe(const KernelEndTools& tools, fs::path& assets)
{
    static std::mutex mutex;
    static TemporaryDirectory cache;
    std::lock_guard<std::mutex> lock(mutex);
    if (cache.path.empty()) {
        char pattern[] = "/tmp/npu-compute-kernel-end-probe-XXXXXX";
        if (mkdtemp(pattern) == nullptr) {
            return false;
        }
        TemporaryDirectory temporary{pattern};
        if (!WriteProbeFiles(temporary.path) || !CompileProbe(temporary.path, tools.compiler)) {
            npucompute::detail::DebugLog("aclpti", "probe_compile failed for the kernel-end probe");
            return false;
        }
        // Publish only complete assets. Failed attempts are cleaned up and may be retried.
        // Keep the cache alive for all binary loads in this process; clean it up on exit.
        cache.path.swap(temporary.path);
    }
    assets = cache.path;
    return true;
}
} // namespace

bool InstrumentKernelEnd(const void* data, std::size_t size, std::vector<char>& output)
{
    output.clear();
    KernelEndTools tools;
    if (!ResolveTools(tools)) {
        return false;
    }
    fs::path assets;
    if (!PrepareProbe(tools, assets)) {
        npucompute::detail::DebugLog("aclpti", "failed to generate kernel-end assets");
        return false;
    }
    char pattern[] = "/tmp/npu-compute-kernel-end-XXXXXX";
    if (mkdtemp(pattern) == nullptr) {
        return false;
    }
    TemporaryDirectory temporary{pattern};
    const auto input = temporary.path / "input.o";
    const auto linked = temporary.path / "linked.o";
    const auto patched = temporary.path / "patched.o";
    {
        std::ofstream file(input.string(), std::ios::binary);
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        file.close();
        if (!file) {
            return false;
        }
    }
    if (!Run(
            {tools.linker.string(), "-m", "aicorelinux", "-Ttext=0", "-execute-probe",
             (assets / kProbeObjectName).string(), input.string(), "-static", "-q", "-o", linked.string()}) ||
        !Run(
            {tools.tuner.string(), "--action=instru-probe", "--instru-memprobe", linked.string(),
             "--dbi-config=" + (assets / kControlRecordName).string(), "-o=" + patched.string()})) {
        return false;
    }
    std::ifstream file(patched.string(), std::ios::binary);
    output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad() && output.size() >= 4 && output[0] == '\x7f' && output[1] == 'E' && output[2] == 'L' &&
           output[3] == 'F';
}
} // namespace aclpti::profiling
