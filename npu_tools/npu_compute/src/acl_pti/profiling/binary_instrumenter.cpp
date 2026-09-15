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
#include "kernel_end_assets.h"
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <iterator>
#include <mutex>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace aclpti::profiling {
namespace {
namespace fs = boost::filesystem;

bool WriteProbeFiles(const fs::path& directory)
{
    const auto write = [&](const char* name, const unsigned char* bytes, std::size_t size) {
        std::ofstream file((directory / name).string(), std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes), size);
        file.close();
        return static_cast<bool>(file);
    };
    return write("kernel_end.o", kKernelEndObject, sizeof(kKernelEndObject)) &&
           write("ctrl.bin", kKernelEndControl, sizeof(kKernelEndControl));
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

struct TemporaryDirectory {
    fs::path path;
    ~TemporaryDirectory()
    {
        boost::system::error_code error;
        fs::remove_all(path, error);
    }
};

bool PrepareProbe(fs::path& assets)
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
        if (!WriteProbeFiles(temporary.path)) {
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
    const char* cann = std::getenv("ASCEND_HOME_PATH");
    if (cann == nullptr || cann[0] == '\0') {
        npucompute::detail::DebugLog("aclpti", "kernel-end requires ASCEND_HOME_PATH");
        return false;
    }
    fs::path tools = fs::path(cann) / "tools/bisheng_compiler/bin";
    if (!fs::is_regular_file(tools / "bisheng-tune")) {
        tools = fs::path(cann).parent_path() / "tools/bisheng_compiler/bin";
    }
    fs::path assets;
    if (!PrepareProbe(assets)) {
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
        file.write(static_cast<const char*>(data), size);
        file.close();
        if (!file) {
            return false;
        }
    }
    if (!Run(
            {(tools / "ld.lld").string(), "-m", "aicorelinux", "-Ttext=0", "-execute-probe",
             (assets / "kernel_end.o").string(), input.string(), "-static", "-q", "-o", linked.string()}) ||
        !Run(
            {(tools / "bisheng-tune").string(), "--action=instru-probe", "--instru-memprobe", linked.string(),
             "--dbi-config=" + (assets / "ctrl.bin").string(), "-o=" + patched.string()})) {
        return false;
    }
    std::ifstream file(patched.string(), std::ios::binary);
    output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad() && output.size() >= 4 && output[0] == '\x7f' && output[1] == 'E' && output[2] == 'L' &&
           output[3] == 'F';
}
} // namespace aclpti::profiling
