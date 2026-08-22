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

#include <acl/acl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr char kSections[] = "PipeUtilization,Memory";
constexpr char kHardwareInfoFile[] = "HardwareInfo.jsonl";
constexpr char kDeviceCountFile[] = "device_count.calls";
constexpr std::array<aclptiCallbackId, 3> kHardwareReadyCbids = {
    ACLPTI_RUNTIME_CBID_aclrtSetDevice,
    ACLPTI_RUNTIME_CBID_aclrtMalloc,
    ACLPTI_RUNTIME_CBID_aclrtLaunchKernel,
};

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
            (std::filesystem::temp_directory_path() / "npu-compute-runtime-callback-test-XXXXXX").string();
        pathTemplate.push_back('\0');
        char* created = ::mkdtemp(pathTemplate.data());
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TempDirectory()
    {
        if (!path_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }
    }

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

bool SetScenarioEnvironment(const std::filesystem::path& output)
{
    ::unsetenv("NPU_COMPUTE_DISABLE_HARDWARE_INFO");
    return ::setenv("NPU_COMPUTE_OUTPUT", output.c_str(), 1) == 0 &&
           ::setenv("NPU_COMPUTE_SECTIONS", kSections, 1) == 0;
}

bool CheckSubscribeAndEnableContract()
{
    using npu_compute::test::AclPtiEnableCall;

    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::CapturedAclPtiCallback() != nullptr);
    CHECK(npu_compute::test::CapturedAclPtiUserData() != nullptr);
    const aclptiSubscribeHandle subscriber = npu_compute::test::CapturedAclPtiSubscriber();
    CHECK(subscriber != nullptr);

    const std::vector<AclPtiEnableCall> enableCalls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(enableCalls.size() == kHardwareReadyCbids.size());
    std::size_t previousSequence = npu_compute::test::AclPtiSubscribeSequence();
    CHECK(previousSequence > 0);
    for (std::size_t index = 0; index < enableCalls.size(); ++index) {
        const AclPtiEnableCall& call = enableCalls[index];
        CHECK(call.sequence > previousSequence);
        CHECK(call.enable);
        CHECK(call.subscriber == subscriber);
        CHECK(call.domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
        CHECK(call.cbid == kHardwareReadyCbids[index]);
        CHECK(call.result == ACLPTI_SUCCESS);
        previousSequence = call.sequence;
    }

    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 1);
    CHECK(npu_compute::test::AclPtiRangeConfigSequence() > previousSequence);
    const std::vector<std::string> sections = npu_compute::test::CapturedAclPtiSections();
    CHECK(sections == std::vector<std::string>({"PipeUtilization", "Memory"}));
    return true;
}

void InvokeCallbackDirectly(aclptiCallbackDomain domain, aclptiCallbackId cbid, const aclptiCallbackData& callbackData)
{
    const aclptiCallbackFunc callback = npu_compute::test::CapturedAclPtiCallback();
    if (callback != nullptr) {
        callback(npu_compute::test::CapturedAclPtiUserData(), domain, cbid, &callbackData);
    }
}

bool RunSuccessChild(const std::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(CheckSubscribeAndEnableContract());
    CHECK(npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    CHECK(npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    return true;
}

bool RunIgnoredEventChild(const std::string& scenario, const std::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(CheckSubscribeAndEnableContract());

    if (scenario == "ignore-enter") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACLPTI_API_ENTER, ACL_SUCCESS, nullptr));
        return true;
    }
    if (scenario == "ignore-failed-exit") {
        CHECK(npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, ACL_ERROR_INVALID_PARAM,
            nullptr));
        return true;
    }
    if (scenario == "ignore-nontarget") {
        const aclptiCallbackData callbackData{
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMemcpy, ACLPTI_API_EXIT, nullptr, ACL_SUCCESS};
        InvokeCallbackDirectly(ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMemcpy, callbackData);
        return true;
    }
    if (scenario == "ignore-mismatched-metadata") {
        const aclptiCallbackData callbackData{
            ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, nullptr, ACL_SUCCESS};
        InvokeCallbackDirectly(ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtSetDevice, callbackData);
        return true;
    }
    return false;
}

bool RunSubscribeFailureChild(const std::filesystem::path& output)
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

bool RunEnableFailureChild(const std::filesystem::path& output, aclptiCallbackId failedCbid)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    npu_compute::test::SetAclPtiEnableResult(failedCbid, ACLPTI_ERROR_NOT_SUPPORTED);
    CHECK(acltoolInitialize() == ACLPTI_ERROR_NOT_SUPPORTED);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 0);

    const auto failurePosition = std::find(kHardwareReadyCbids.begin(), kHardwareReadyCbids.end(), failedCbid);
    CHECK(failurePosition != kHardwareReadyCbids.end());
    const std::size_t successfulEnableCount =
        static_cast<std::size_t>(std::distance(kHardwareReadyCbids.begin(), failurePosition));
    const std::vector<npu_compute::test::AclPtiEnableCall> calls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(calls.size() == successfulEnableCount * 2 + 1);

    for (std::size_t index = 0; index <= successfulEnableCount; ++index) {
        CHECK(calls[index].enable);
        CHECK(calls[index].domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
        CHECK(calls[index].cbid == kHardwareReadyCbids[index]);
        CHECK(calls[index].result == (index == successfulEnableCount ? ACLPTI_ERROR_NOT_SUPPORTED : ACLPTI_SUCCESS));
    }
    for (std::size_t index = 0; index < successfulEnableCount; ++index) {
        const aclptiCallbackId enabledCbid = kHardwareReadyCbids[index];
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

bool RunConfigFailureChild(const std::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    npu_compute::test::ResetAclPtiCallbackStub();
    npu_compute::test::SetAclPtiRangeConfigResult(ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(acltoolInitialize() == ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 1);
    const std::vector<npu_compute::test::AclPtiEnableCall> calls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(calls.size() == kHardwareReadyCbids.size() * 2);
    for (aclptiCallbackId cbid : kHardwareReadyCbids) {
        CHECK(!npu_compute::test::InvokeAclPtiCallback(
            ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    }
    return true;
}

bool RunHardwareInfoDisabledChild(const std::filesystem::path& output)
{
    CHECK(SetScenarioEnvironment(output));
    CHECK(::setenv("NPU_COMPUTE_DISABLE_HARDWARE_INFO", "1", 1) == 0);
    npu_compute::test::ResetAclPtiCallbackStub();
    CHECK(acltoolInitialize() == ACLPTI_SUCCESS);
    CHECK(npu_compute::test::AclPtiSubscribeCount() == 1);
    CHECK(npu_compute::test::CapturedAclPtiCallback() == nullptr);
    CHECK(npu_compute::test::CapturedAclPtiUserData() == nullptr);
    CHECK(npu_compute::test::CapturedAclPtiSubscriber() != nullptr);
    CHECK(npu_compute::test::AclPtiEnableCount() == 0);
    CHECK(npu_compute::test::AclPtiRangeConfigCount() == 1);
    const std::vector<std::string> sections = npu_compute::test::CapturedAclPtiSections();
    CHECK(sections == std::vector<std::string>({"PipeUtilization", "Memory"}));
    CHECK(!npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    return true;
}

bool RunChildScenario(const std::string& scenario, const std::filesystem::path& output)
{
    if (scenario == "success") {
        return RunSuccessChild(output);
    }
    if (scenario.rfind("ignore-", 0) == 0) {
        return RunIgnoredEventChild(scenario, output);
    }
    if (scenario == "subscribe-failure") {
        return RunSubscribeFailureChild(output);
    }
    if (scenario == "enable-failure-set-device") {
        return RunEnableFailureChild(output, ACLPTI_RUNTIME_CBID_aclrtSetDevice);
    }
    if (scenario == "enable-failure-malloc") {
        return RunEnableFailureChild(output, ACLPTI_RUNTIME_CBID_aclrtMalloc);
    }
    if (scenario == "enable-failure-launch") {
        return RunEnableFailureChild(output, ACLPTI_RUNTIME_CBID_aclrtLaunchKernel);
    }
    if (scenario == "config-failure") {
        return RunConfigFailureChild(output);
    }
    if (scenario == "hardware-info-disabled") {
        return RunHardwareInfoDisabledChild(output);
    }
    std::fprintf(stderr, "unknown child scenario: %s\n", scenario.c_str());
    return false;
}

bool LaunchChild(const char* executable, const char* scenario, const std::filesystem::path& output)
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

std::size_t CountLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::size_t lines = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++lines;
    }
    return lines;
}

bool TestSuccessAndNormalExit(const char* executable)
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CHECK(LaunchChild(executable, "success", temporary.Path()));
    CHECK(std::filesystem::is_regular_file(temporary.Path() / kHardwareInfoFile));
    CHECK(CountLines(temporary.Path() / kHardwareInfoFile) == 5);
    CHECK(std::filesystem::file_size(temporary.Path() / kDeviceCountFile) == 1);
    return true;
}

bool TestIgnoredEvents(const char* executable)
{
    constexpr std::array<const char*, 4> scenarios = {
        "ignore-enter",
        "ignore-failed-exit",
        "ignore-nontarget",
        "ignore-mismatched-metadata",
    };
    for (const char* scenario : scenarios) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        CHECK(LaunchChild(executable, scenario, temporary.Path()));
        CHECK(!std::filesystem::exists(temporary.Path() / kHardwareInfoFile));
        CHECK(!std::filesystem::exists(temporary.Path() / kDeviceCountFile));
    }
    return true;
}

bool TestInitializationFailures(const char* executable)
{
    constexpr std::array<const char*, 5> scenarios = {
        "subscribe-failure", "enable-failure-set-device", "enable-failure-malloc", "enable-failure-launch",
        "config-failure",
    };
    for (const char* scenario : scenarios) {
        TempDirectory temporary;
        CHECK(!temporary.Path().empty());
        CHECK(LaunchChild(executable, scenario, temporary.Path()));
        CHECK(!std::filesystem::exists(temporary.Path() / kHardwareInfoFile));
    }
    return true;
}

bool TestHardwareInfoDisabled(const char* executable)
{
    TempDirectory temporary;
    CHECK(!temporary.Path().empty());
    CHECK(LaunchChild(executable, "hardware-info-disabled", temporary.Path()));
    CHECK(!std::filesystem::exists(temporary.Path() / kHardwareInfoFile));
    CHECK(!std::filesystem::exists(temporary.Path() / kDeviceCountFile));
    return true;
}

} // namespace

extern "C" aclError aclrtGetDeviceCount(std::uint32_t* count)
{
    if (count == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *count = 0;
    const char* output = std::getenv("NPU_COMPUTE_OUTPUT");
    if (output == nullptr) {
        return ACL_ERROR_FAILURE;
    }
    const std::filesystem::path countPath = std::filesystem::path(output) / kDeviceCountFile;
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
    return TestSuccessAndNormalExit(argv[0]) && TestIgnoredEvents(argv[0]) && TestInitializationFailures(argv[0]) &&
                   TestHardwareInfoDisabled(argv[0]) ?
               0 :
               1;
}
