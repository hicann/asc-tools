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
#include "npu_compute/npu_compute.h"
#include "npu_compute_runtime.h"

#include <acl/acl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr char kSections[] = "PipeUtilization,Memory";
constexpr char kHardwareInfoFile[] = "HardwareInfo.jsonl";
constexpr char kDeviceCountFile[] = "device_count.calls";
constexpr std::array<aclptiCallbackId, 3> kHardwareInfoTriggerCbids = {
    ACLPTI_RUNTIME_CBID_aclrtLaunchKernel,
    ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs,
    ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs,
};
using namespace std::chrono_literals;

std::mutex g_deviceCountMutex;
std::condition_variable g_deviceCountCondition;
bool g_blockDeviceCount = false;
bool g_deviceCountStarted = false;
bool g_releaseDeviceCount = false;

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return false;                                                                 \
        }                                                                                 \
    } while (false)

class TempDirectory {
public:
    TempDirectory()
    {
        std::string pathTemplate =
            (boost::filesystem::temp_directory_path() / "npu-compute-runtime-callback-test-XXXXXX").string();
        pathTemplate.push_back('\0');
        char* created = ::mkdtemp(pathTemplate.data());
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TempDirectory()
    {
        if (!path_.empty()) {
            boost::system::error_code error;
            boost::filesystem::remove_all(path_, error);
        }
    }

    const boost::filesystem::path& Path() const { return path_; }

private:
    boost::filesystem::path path_;
};

bool SetScenarioEnvironment(const boost::filesystem::path& output)
{
    return ::setenv("NPU_COMPUTE_OUTPUT", output.c_str(), 1) == 0 &&
           ::setenv("NPU_COMPUTE_SECTIONS", kSections, 1) == 0 && ::unsetenv("NPU_COMPUTE_PMU_LEVEL") == 0;
}

bool TestPmuLevelEnvironment()
{
    const char* previous = std::getenv("NPU_COMPUTE_PMU_LEVEL");
    const bool hadPrevious = previous != nullptr;
    const std::string previousValue = hadPrevious ? previous : "";
    npu_compute::PmuDataLevel level = npu_compute::PmuDataLevel::Task;
    std::string error;

    CHECK(::unsetenv("NPU_COMPUTE_PMU_LEVEL") == 0);
    CHECK(npu_compute::detail::LoadPmuDataLevelFromEnvironment("NPU_COMPUTE_PMU_LEVEL", &level, &error));
    CHECK(level == npu_compute::PmuDataLevel::Block);

    CHECK(::setenv("NPU_COMPUTE_PMU_LEVEL", "", 1) == 0);
    level = npu_compute::PmuDataLevel::Task;
    CHECK(npu_compute::detail::LoadPmuDataLevelFromEnvironment("NPU_COMPUTE_PMU_LEVEL", &level, &error));
    CHECK(level == npu_compute::PmuDataLevel::Block);

    CHECK(::setenv("NPU_COMPUTE_PMU_LEVEL", "block", 1) == 0);
    CHECK(npu_compute::detail::LoadPmuDataLevelFromEnvironment("NPU_COMPUTE_PMU_LEVEL", &level, &error));
    CHECK(level == npu_compute::PmuDataLevel::Block);

    CHECK(::setenv("NPU_COMPUTE_PMU_LEVEL", "task", 1) == 0);
    CHECK(npu_compute::detail::LoadPmuDataLevelFromEnvironment("NPU_COMPUTE_PMU_LEVEL", &level, &error));
    CHECK(level == npu_compute::PmuDataLevel::Task);

    CHECK(::setenv("NPU_COMPUTE_PMU_LEVEL", "TASK", 1) == 0);
    error.clear();
    CHECK(!npu_compute::detail::LoadPmuDataLevelFromEnvironment("NPU_COMPUTE_PMU_LEVEL", &level, &error));
    CHECK(error.find("NPU_COMPUTE_PMU_LEVEL") != std::string::npos);
    CHECK(error.find("TASK") != std::string::npos);
    CHECK(error.find("block") != std::string::npos);
    CHECK(error.find("task") != std::string::npos);

    return hadPrevious ? ::setenv("NPU_COMPUTE_PMU_LEVEL", previousValue.c_str(), 1) == 0 :
                         ::unsetenv("NPU_COMPUTE_PMU_LEVEL") == 0;
}

bool CheckSubscribeAndEnableContract()
{
    using npu_compute::test::AclPtiEnableCall;

    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::CapturedAclPtiCallback() != nullptr);
    CHECK(npu_compute::test::CapturedAclPtiUserData() != nullptr);
    CHECK(
        npu_compute::test::CapturedAclPtiUserData() != static_cast<void*>(&npu_compute::NpuComputeRuntime::Instance()));
    const aclptiSubscribeHandle subscriber = npu_compute::test::CapturedAclPtiSubscriber();
    CHECK(subscriber != nullptr);

    const std::vector<AclPtiEnableCall> enableCalls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(enableCalls.size() == kHardwareInfoTriggerCbids.size());
    std::size_t previousSequence = npu_compute::test::AclPtiSubscribeSequence();
    CHECK(previousSequence > 0);
    for (std::size_t index = 0; index < enableCalls.size(); ++index) {
        const AclPtiEnableCall& call = enableCalls[index];
        CHECK(call.sequence > previousSequence);
        CHECK(call.enable);
        CHECK(call.subscriber == subscriber);
        CHECK(call.domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
        CHECK(call.cbid == kHardwareInfoTriggerCbids[index]);
        CHECK(call.result == ACLPTI_SUCCESS);
        previousSequence = call.sequence;
    }

    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 1);
    CHECK(npu_compute::test::AclPtiRangeConfigSequence() > previousSequence);
    const std::vector<std::string> sections = npu_compute::test::CapturedAclPtiSections();
    CHECK(sections == std::vector<std::string>({"PipeUtilization", "Memory"}));
    CHECK(npu_compute::test::CapturedAclPtiBlockResult() == ACLPTI_BLOCK_RESULT_ALL);
    CHECK(!npu_compute::test::CapturedAclPtiCollectPipeline());
    CHECK(!npu_compute::test::CapturedAclPtiCollectPcSampling());
    return true;
}

bool CheckDisableContract()
{
    using npu_compute::test::AclPtiEnableCall;

    const std::vector<AclPtiEnableCall> calls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(calls.size() == kHardwareInfoTriggerCbids.size() * 2);
    for (std::size_t index = 0; index < kHardwareInfoTriggerCbids.size(); ++index) {
        const AclPtiEnableCall& call = calls[kHardwareInfoTriggerCbids.size() + index];
        CHECK(!call.enable);
        CHECK(call.domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
        CHECK(call.cbid == kHardwareInfoTriggerCbids[kHardwareInfoTriggerCbids.size() - index - 1]);
        CHECK(call.result == ACLPTI_SUCCESS);
    }
    return true;
}

void InvokeCallbackDirectly(aclptiCallbackDomain domain, aclptiCallbackId cbid, const aclptiCallbackData& callbackData)
{
    const aclptiCallbackFunc callback = npu_compute::test::CapturedAclPtiCallback();
    if (callback != nullptr) {
        callback(npu_compute::test::CapturedAclPtiUserData(), domain, cbid, &callbackData);
    }
}

std::size_t CountLines(const boost::filesystem::path& path);

bool RunSuccessChild(const std::string& scenario, const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(CheckSubscribeAndEnableContract());

    if (scenario == "success-launch") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_EXIT, ACL_SUCCESS,
            nullptr));
        CHECK(boost::filesystem::is_regular_file(output / kHardwareInfoFile));
        CHECK(CountLines(output / kHardwareInfoFile) == 5);
        return true;
    }
    if (scenario == "success-host-args") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, ACLPTI_API_EXIT,
            ACL_SUCCESS, nullptr));
        CHECK(boost::filesystem::is_regular_file(output / kHardwareInfoFile));
        CHECK(CountLines(output / kHardwareInfoFile) == 5);
        return true;
    }
    if (scenario == "success-simt-host-args") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs, ACLPTI_API_EXIT,
            ACL_SUCCESS, nullptr));
        CHECK(boost::filesystem::is_regular_file(output / kHardwareInfoFile));
        CHECK(CountLines(output / kHardwareInfoFile) == 5);
        return true;
    }
    if (scenario == "success-repeated") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_EXIT, ACL_SUCCESS,
            nullptr));
        CHECK(boost::filesystem::is_regular_file(output / kHardwareInfoFile));
        CHECK(CountLines(output / kHardwareInfoFile) == 5);
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, ACLPTI_API_EXIT,
            ACL_SUCCESS, nullptr));
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs, ACLPTI_API_EXIT,
            ACL_SUCCESS, nullptr));
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_EXIT, ACL_SUCCESS,
            nullptr));
        return true;
    }
    return false;
}

void InvokeSuccessfulExitDirectly(aclptiCallbackId cbid)
{
    const aclptiCallbackData callbackData{ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_EXIT, nullptr, ACL_SUCCESS};
    InvokeCallbackDirectly(ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, callbackData);
}

bool RunNormalStopChild(const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(CheckSubscribeAndEnableContract());
    npu_compute::NpuComputeRuntime::Instance().Stop();
    CHECK(CheckDisableContract());
    for (aclptiCallbackId cbid : kHardwareInfoTriggerCbids) {
        CHECK(!npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    }
    return true;
}

bool RunStopDuringCollectionChild(const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    {
        std::lock_guard<std::mutex> lock(g_deviceCountMutex);
        g_blockDeviceCount = true;
        g_deviceCountStarted = false;
        g_releaseDeviceCount = false;
    }
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(CheckSubscribeAndEnableContract());

    std::atomic<bool> callbackReturned{false};
    std::atomic<bool> callbackDispatched{false};
    std::thread callbackThread([&callbackReturned, &callbackDispatched] {
        callbackDispatched = npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr);
        callbackReturned = true;
    });

    bool deviceCountStarted = false;
    {
        std::unique_lock<std::mutex> lock(g_deviceCountMutex);
        deviceCountStarted = g_deviceCountCondition.wait_for(lock, 10s, [] { return g_deviceCountStarted; });
    }

    std::atomic<bool> stopReturned{false};
    std::thread stopThread([&stopReturned] {
        npu_compute::NpuComputeRuntime::Instance().Stop();
        stopReturned = true;
    });

    bool callbacksDisabled = false;
    if (deviceCountStarted) {
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (npu_compute::test::CapturedAclPtiEnableCalls().size() == kHardwareInfoTriggerCbids.size() * 2) {
                callbacksDisabled = true;
                break;
            }
            std::this_thread::sleep_for(1ms);
        }
    }

    std::atomic<bool> drainReturned{false};
    std::atomic<int> drainStatus{npu_compute::kInitializeFailed};
    std::thread drainThread([&drainReturned, &drainStatus] {
        drainStatus = npu_compute::NpuComputeRuntime::Instance().ShutdownAfterPtiDrain();
        drainReturned = true;
    });
    const auto drainDeadline = std::chrono::steady_clock::now() + 500ms;
    while (!drainReturned.load() && std::chrono::steady_clock::now() < drainDeadline) {
        std::this_thread::sleep_for(1ms);
    }

    const bool callbackReturnedBeforeRelease = callbackReturned.load();
    const bool stopReturnedBeforeRelease = stopReturned.load();
    const bool drainReturnedBeforeRelease = drainReturned.load();
    {
        std::lock_guard<std::mutex> lock(g_deviceCountMutex);
        g_releaseDeviceCount = true;
    }
    g_deviceCountCondition.notify_all();
    callbackThread.join();
    stopThread.join();
    drainThread.join();

    CHECK(deviceCountStarted);
    CHECK(callbacksDisabled);
    CHECK(callbackDispatched.load());
    CHECK(!callbackReturnedBeforeRelease);
    CHECK(!stopReturnedBeforeRelease);
    CHECK(drainReturnedBeforeRelease);
    CHECK(drainStatus.load() == 0);
    CHECK(CheckDisableContract());
    CHECK(boost::filesystem::is_regular_file(output / kHardwareInfoFile));
    CHECK(CountLines(output / kHardwareInfoFile) == 5);
    return true;
}

bool RunIgnoredEventChild(const std::string& scenario, const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(CheckSubscribeAndEnableContract());

    if (scenario == "ignore-enter") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_ENTER, ACL_SUCCESS,
            nullptr));
        return true;
    }
    if (scenario == "ignore-failed-exit") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, ACLPTI_API_EXIT,
            ACL_ERROR_INVALID_PARAM, nullptr));
        return true;
    }
    if (scenario == "ignore-simt-failed-exit") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs, ACLPTI_API_EXIT,
            ACL_ERROR_INVALID_PARAM, nullptr));
        return true;
    }
    if (scenario == "ignore-set-device") {
        InvokeSuccessfulExitDirectly(ACLPTI_RUNTIME_CBID_aclrtSetDevice);
        return true;
    }
    if (scenario == "ignore-malloc") {
        InvokeSuccessfulExitDirectly(ACLPTI_RUNTIME_CBID_aclrtMalloc);
        return true;
    }
    if (scenario == "ignore-nontarget") {
        const aclptiCallbackData callbackData{
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMemcpy, ACLPTI_API_EXIT, nullptr, ACL_SUCCESS};
        InvokeCallbackDirectly(ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMemcpy, callbackData);
        return true;
    }
    if (scenario == "ignore-domain") {
        const aclptiCallbackData callbackData{
            ACLPTI_CB_DOMAIN_INVALID, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_EXIT, nullptr, ACL_SUCCESS};
        InvokeCallbackDirectly(ACLPTI_CB_DOMAIN_INVALID, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, callbackData);
        return true;
    }
    if (scenario == "ignore-mismatched-metadata") {
        const aclptiCallbackData callbackData{
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, ACLPTI_API_EXIT, nullptr,
            ACL_SUCCESS};
        InvokeCallbackDirectly(ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, callbackData);
        return true;
    }
    return false;
}

bool RunSubscribeFailureChild(const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    npu_compute::test::SetAclPtiSubscribeResult(ACLPTI_ERROR_INITIALIZATION_FAILED);
    CHECK(acltoolInitialize() == ACLPTI_ERROR_INITIALIZATION_FAILED);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::AclPtiEnableCount() == 0);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 0);
    CHECK(npu_compute::test::CapturedAclPtiCallback() == nullptr);
    CHECK(npu_compute::test::CapturedAclPtiUserData() == nullptr);
    CHECK(npu_compute::test::CapturedAclPtiSubscriber() == nullptr);
    return true;
}

bool RunEnableFailureChild(const boost::filesystem::path& output, aclptiCallbackId failedCbid)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    npu_compute::test::SetAclPtiEnableResult(failedCbid, ACLPTI_ERROR_NOT_SUPPORTED);
    CHECK(acltoolInitialize() == ACLPTI_ERROR_NOT_SUPPORTED);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 0);

    const auto failurePosition =
        std::find(kHardwareInfoTriggerCbids.begin(), kHardwareInfoTriggerCbids.end(), failedCbid);
    CHECK(failurePosition != kHardwareInfoTriggerCbids.end());
    const std::size_t successfulEnableCount =
        static_cast<std::size_t>(std::distance(kHardwareInfoTriggerCbids.begin(), failurePosition));
    const std::vector<npu_compute::test::AclPtiEnableCall> calls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(calls.size() == successfulEnableCount * 2 + 1);

    for (std::size_t index = 0; index <= successfulEnableCount; ++index) {
        CHECK(calls[index].enable);
        CHECK(calls[index].domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
        CHECK(calls[index].cbid == kHardwareInfoTriggerCbids[index]);
        CHECK(calls[index].result == (index == successfulEnableCount ? ACLPTI_ERROR_NOT_SUPPORTED : ACLPTI_SUCCESS));
    }
    for (std::size_t index = 0; index < successfulEnableCount; ++index) {
        const aclptiCallbackId enabledCbid = kHardwareInfoTriggerCbids[index];
        std::size_t disableCount = 0;
        for (std::size_t callIndex = successfulEnableCount + 1; callIndex < calls.size(); ++callIndex) {
            if (!calls[callIndex].enable && calls[callIndex].cbid == enabledCbid &&
                calls[callIndex].result == ACLPTI_SUCCESS) {
                ++disableCount;
            }
        }
        CHECK(disableCount == 1);
        CHECK(!npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, enabledCbid, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    }
    return true;
}

bool RunConfigFailureChild(const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    npu_compute::test::SetAclPtiRangeConfigResult(ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(acltoolInitialize() == ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 1);
    CHECK(CheckDisableContract());
    for (aclptiCallbackId cbid : kHardwareInfoTriggerCbids) {
        CHECK(!npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    }
    return true;
}

bool RunPmuLevelFailureChild(const boost::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    CHECK(::setenv("NPU_COMPUTE_PMU_LEVEL", "invalid-level", 1) == 0);
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == npu_compute::kInitializeFailed);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 0);
    CHECK(npu_compute::test::AclPtiEnableCount() == 0);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 0);
    return true;
}

bool RunChildScenario(const std::string& scenario, const boost::filesystem::path& output)
{
    if (scenario.rfind("success-", 0) == 0) {
        return RunSuccessChild(scenario, output);
    }
    if (scenario.rfind("ignore-", 0) == 0) {
        return RunIgnoredEventChild(scenario, output);
    }
    if (scenario == "subscribe-failure") {
        return RunSubscribeFailureChild(output);
    }
    if (scenario == "enable-failure-launch") {
        return RunEnableFailureChild(output, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel);
    }
    if (scenario == "enable-failure-host-args") {
        return RunEnableFailureChild(output, ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs);
    }
    if (scenario == "enable-failure-simt-host-args") {
        return RunEnableFailureChild(output, ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs);
    }
    if (scenario == "config-failure") {
        return RunConfigFailureChild(output);
    }
    if (scenario == "pmu-level-failure") {
        return RunPmuLevelFailureChild(output);
    }
    if (scenario == "normal-stop") {
        return RunNormalStopChild(output);
    }
    if (scenario == "stop-during-collection") {
        return RunStopDuringCollectionChild(output);
    }
    std::fprintf(stderr, "unknown child scenario: %s\n", scenario.c_str());
    return false;
}

bool LaunchChild(const char* executable, const char* scenario, const boost::filesystem::path& output)
{
    const pid_t process = ::fork();
    CHECK(process >= 0);
    if (process == 0) {
        ::execl(executable, executable, "--child", scenario, output.c_str(), static_cast<char*>(nullptr));
        std::perror("execl runtime callback child failed");
        ::_exit(127);
    }

    int status = 0;
    CHECK(::waitpid(process, &status, 0) == process);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    return true;
}

std::size_t CountLines(const boost::filesystem::path& path)
{
    std::ifstream input(path.string());
    std::size_t lines = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++lines;
    }
    return lines;
}

bool TestSuccessAndNormalExit(const char* executable)
{
    constexpr std::array<const char*, 4> scenarios = {
        "success-launch",
        "success-host-args",
        "success-simt-host-args",
        "success-repeated",
    };
    for (const char* scenario : scenarios) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        CHECK(LaunchChild(executable, scenario, temporary.Path()));
        CHECK(boost::filesystem::is_regular_file(temporary.Path() / kHardwareInfoFile));
        CHECK(CountLines(temporary.Path() / kHardwareInfoFile) == 5);
        CHECK(boost::filesystem::file_size(temporary.Path() / kDeviceCountFile) == 1);
    }
    return true;
}

bool TestIgnoredEvents(const char* executable)
{
    constexpr std::array<const char*, 8> scenarios = {
        "ignore-enter",  "ignore-failed-exit", "ignore-simt-failed-exit", "ignore-set-device",
        "ignore-malloc", "ignore-nontarget",   "ignore-domain",           "ignore-mismatched-metadata",
    };
    for (const char* scenario : scenarios) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        CHECK(LaunchChild(executable, scenario, temporary.Path()));
        CHECK(!boost::filesystem::exists(temporary.Path() / kHardwareInfoFile));
        CHECK(!boost::filesystem::exists(temporary.Path() / kDeviceCountFile));
    }
    return true;
}

bool TestInitializationFailures(const char* executable)
{
    constexpr std::array<const char*, 6> scenarios = {
        "subscribe-failure", "enable-failure-launch", "enable-failure-host-args", "enable-failure-simt-host-args",
        "config-failure",    "pmu-level-failure",
    };
    for (const char* scenario : scenarios) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        CHECK(LaunchChild(executable, scenario, temporary.Path()));
        CHECK(!boost::filesystem::exists(temporary.Path() / kHardwareInfoFile));
    }
    return true;
}

bool TestNormalStop(const char* executable)
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CHECK(LaunchChild(executable, "normal-stop", temporary.Path()));
    CHECK(!boost::filesystem::exists(temporary.Path() / kHardwareInfoFile));
    CHECK(!boost::filesystem::exists(temporary.Path() / kDeviceCountFile));
    return true;
}

bool TestStopDuringCollectionDoesNotHoldRuntimeMutex(const char* executable)
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CHECK(LaunchChild(executable, "stop-during-collection", temporary.Path()));
    return true;
}

} // namespace

extern "C" aclError aclrtGetDeviceCount(uint32_t* count)
{
    if (count == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *count = 0;
    const char* output = std::getenv("NPU_COMPUTE_OUTPUT");
    if (output == nullptr) {
        return ACL_ERROR_FAILURE;
    }
    {
        std::unique_lock<std::mutex> lock(g_deviceCountMutex);
        if (g_blockDeviceCount) {
            g_deviceCountStarted = true;
            g_deviceCountCondition.notify_all();
            g_deviceCountCondition.wait(lock, [] { return g_releaseDeviceCount; });
        }
    }
    const boost::filesystem::path countPath = boost::filesystem::path(output) / kDeviceCountFile;
    const int descriptor = ::open(countPath.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        return ACL_ERROR_FAILURE;
    }
    const char marker = '1';
    const bool written = ::write(descriptor, &marker, sizeof(marker)) == static_cast<ssize_t>(sizeof(marker));
    ::close(descriptor);
    return written ? ACL_SUCCESS : ACL_ERROR_FAILURE;
}

int main(int argc, char** argv)
{
    if (argc == 4 && std::string(argv[1]) == "--child") {
        return RunChildScenario(argv[2], argv[3]) ? 0 : 1;
    }
    if (argc != 1) {
        std::fprintf(stderr, "unexpected runtime callback test arguments\n");
        return 2;
    }
    return TestPmuLevelEnvironment() && TestSuccessAndNormalExit(argv[0]) && TestIgnoredEvents(argv[0]) &&
                   TestInitializationFailures(argv[0]) && TestNormalStop(argv[0]) &&
                   TestStopDuringCollectionDoesNotHoldRuntimeMutex(argv[0]) ?
               0 :
               1;
}
