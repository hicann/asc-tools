/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "npu_compute/acl_pti_callback_stub.h"

#include <acl/acl.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <type_traits>

#include <dlfcn.h>
#include <unistd.h>

namespace {

constexpr std::int32_t kDeviceId = 0;
constexpr char kHardwareInfoFile[] = "HardwareInfo.jsonl";
constexpr char kSections[] = "PipeUtilization";
constexpr auto kOutputTimeout = std::chrono::seconds(60);

class AclRuntimeGuard {
public:
    ~AclRuntimeGuard()
    {
        if (automaticCleanup_) {
            Cleanup();
        }
    }

    void MarkInitialized() noexcept { initialized_ = true; }
    void MarkDeviceSet() noexcept { deviceSet_ = true; }
    void DisableAutomaticCleanup() noexcept { automaticCleanup_ = false; }

    bool Cleanup() noexcept
    {
        bool success = true;
        if (deviceSet_) {
            const aclError result = aclrtResetDevice(kDeviceId);
            if (result != ACL_SUCCESS) {
                std::fprintf(
                    stderr, "[real_hardware_callback_app] aclrtResetDevice failed: %d\n", static_cast<int>(result));
                success = false;
            }
            deviceSet_ = false;
        }
        if (initialized_) {
            const aclError result = aclFinalize();
            if (result != ACL_SUCCESS) {
                std::fprintf(stderr, "[real_hardware_callback_app] aclFinalize failed: %d\n", static_cast<int>(result));
                success = false;
            }
            initialized_ = false;
        }
        return success;
    }

private:
    bool initialized_ = false;
    bool deviceSet_ = false;
    bool automaticCleanup_ = true;
};

template <typename Function>
Function ToFunction(void* symbol)
{
    static_assert(std::is_pointer_v<Function>);
    static_assert(sizeof(Function) == sizeof(symbol));
    Function function = nullptr;
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

bool PrepareEnvironment(const std::filesystem::path& outputDirectory)
{
    if (!outputDirectory.is_absolute()) {
        std::fprintf(stderr, "[real_hardware_callback_app] output directory must be absolute\n");
        return false;
    }

    std::error_code error;
    if (!std::filesystem::is_directory(outputDirectory, error) || error) {
        std::fprintf(stderr, "[real_hardware_callback_app] output directory is unavailable\n");
        return false;
    }
    if (::access(outputDirectory.c_str(), W_OK | X_OK) != 0) {
        std::perror("[real_hardware_callback_app] output directory is not writable");
        return false;
    }
    return ::setenv("NPU_COMPUTE_OUTPUT", outputDirectory.c_str(), 1) == 0 &&
           ::setenv("NPU_COMPUTE_SECTIONS", kSections, 1) == 0;
}

bool EmitSuccessfulExit(aclptiCallbackId cbid)
{
    return AclPtiCallbackStubEmitRuntimeEvent(
               static_cast<uint32_t>(cbid), static_cast<uint32_t>(ACLPTI_API_EXIT),
               static_cast<std::int32_t>(ACL_SUCCESS)) == 1;
}

bool WaitForHardwareInfo(const std::filesystem::path& outputDirectory)
{
    const std::filesystem::path output = outputDirectory / kHardwareInfoFile;
    const auto deadline = std::chrono::steady_clock::now() + kOutputTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if (std::filesystem::is_regular_file(output, error) && !error) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <test-libnpu-compute.so> <output-directory>\n", argv[0]);
        return 2;
    }

    const std::filesystem::path injectionLibrary = std::filesystem::absolute(argv[1]);
    const std::filesystem::path outputDirectory = argv[2];
    if (!PrepareEnvironment(outputDirectory)) {
        return 3;
    }

    AclRuntimeGuard aclRuntime;
    aclError result = aclInit(nullptr);
    if (result != ACL_SUCCESS) {
        std::fprintf(stderr, "[real_hardware_callback_app] aclInit failed: %d\n", static_cast<int>(result));
        return 4;
    }
    aclRuntime.MarkInitialized();

    void* injectionHandle = ::dlopen(injectionLibrary.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (injectionHandle == nullptr) {
        std::fprintf(stderr, "[real_hardware_callback_app] dlopen failed: %s\n", ::dlerror());
        return 5;
    }
    // acltoolInitialize registers an atexit handler in this library.
    // Keep the handle loaded until process exit so that handler remains valid.

    ::dlerror();
    using InitializeFunction = int (*)();
    const InitializeFunction initialize = ToFunction<InitializeFunction>(::dlsym(injectionHandle, "acltoolInitialize"));
    const char* symbolError = ::dlerror();
    if (symbolError != nullptr || initialize == nullptr) {
        std::fprintf(
            stderr, "[real_hardware_callback_app] dlsym failed: %s\n",
            symbolError == nullptr ? "acltoolInitialize is null" : symbolError);
        return 6;
    }
    const int initializeResult = initialize();
    if (initializeResult != ACLPTI_SUCCESS) {
        std::fprintf(stderr, "[real_hardware_callback_app] acltoolInitialize failed: %d\n", initializeResult);
        return 7;
    }
    // On later failures, process exit stops the collector before the OS tears
    // down ACL. The success path performs explicit cleanup after publication.
    aclRuntime.DisableAutomaticCleanup();

    result = aclrtSetDevice(kDeviceId);
    if (result != ACL_SUCCESS) {
        std::fprintf(stderr, "[real_hardware_callback_app] aclrtSetDevice failed: %d\n", static_cast<int>(result));
        return 8;
    }
    aclRuntime.MarkDeviceSet();

    if (!EmitSuccessfulExit(ACLPTI_RUNTIME_CBID_aclrtSetDevice) ||
        !EmitSuccessfulExit(ACLPTI_RUNTIME_CBID_aclrtMalloc) ||
        !EmitSuccessfulExit(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel)) {
        std::fprintf(stderr, "[real_hardware_callback_app] callback event was not dispatched\n");
        return 9;
    }

    if (!WaitForHardwareInfo(outputDirectory)) {
        std::fprintf(stderr, "[real_hardware_callback_app] HardwareInfo.jsonl timed out\n");
        return 10;
    }
    if (!aclRuntime.Cleanup()) {
        return 11;
    }
    std::fprintf(stderr, "[real_hardware_callback_app] HardwareInfo.jsonl published\n");
    return 0;
}
