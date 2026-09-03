/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "npu_compute_runtime.h"

#include "common/debug_log.h"
#include "hardware_device_api.h"
#include "hardware_info_json.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <unistd.h>

namespace npu_compute {
namespace {

constexpr std::array<aclptiCallbackId, 3> kHardwareInfoTriggerCallbackIds = {
    ACLPTI_RUNTIME_CBID_aclrtLaunchKernel,
    ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs,
    ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs,
};
constexpr double kMsopprofA5FallbackFrequencyMhz = 1650.0;

bool IsHardwareInfoTriggerCallback(aclptiCallbackId cbid)
{
    return std::find(kHardwareInfoTriggerCallbackIds.begin(), kHardwareInfoTriggerCallbackIds.end(), cbid) !=
           kHardwareInfoTriggerCallbackIds.end();
}

bool DebugEnabled()
{
    const char* value = std::getenv("NPU_COMPUTE_DEBUG");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

bool LoadOutputDirectory(boost::filesystem::path* outputDirectory, std::string* error)
{
    const char* value = std::getenv("NPU_COMPUTE_OUTPUT");
    if (value == nullptr || value[0] == '\0') {
        *error = "NPU_COMPUTE_OUTPUT is not set or is empty";
        return false;
    }

    boost::filesystem::path candidate(value);
    if (!candidate.is_absolute()) {
        *error = "NPU_COMPUTE_OUTPUT must be an absolute path";
        return false;
    }

    boost::system::error_code filesystemError;
    if (!boost::filesystem::is_directory(candidate, filesystemError)) {
        *error = filesystemError ? "inspect NPU_COMPUTE_OUTPUT failed: " + filesystemError.message() :
                                   "NPU_COMPUTE_OUTPUT is not a directory";
        return false;
    }
    if (::access(candidate.c_str(), W_OK | X_OK) != 0) {
        *error = "NPU_COMPUTE_OUTPUT is not writable";
        return false;
    }

    *outputDirectory = std::move(candidate);
    return true;
}

std::string_view Trim(std::string_view value)
{
    constexpr std::string_view whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool ParsePositiveDouble(std::string_view text, double* result)
{
    const std::string_view trimmed = Trim(text);
    if (trimmed.empty() || result == nullptr) {
        return false;
    }
    std::string owned(trimmed);
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(owned.c_str(), &end);
    if (errno != 0 || end == owned.c_str() || *end != '\0' || parsed <= 0.0 || !std::isfinite(parsed)) {
        return false;
    }
    *result = parsed;
    return true;
}

void LoadCsvAiCoreCountsFromDevice(PmuCsvConfig* config)
{
    if (config == nullptr || (config->aicCoreCount != 0 && config->aivCoreCount != 0)) {
        return;
    }

    DynamicHardwareDeviceApi api;
    std::uint32_t aiCubeCount = 0;
    std::uint32_t aiVectorCount = 0;
    DiagnosticSink diagnostics = [](std::string_view message) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV hardware count fallback diagnostic: %.*s", static_cast<int>(message.size()),
            message.data());
    };
    if (!CollectAiCoreCounts(api, &aiCubeCount, &aiVectorCount, &diagnostics)) {
        npu_compute::detail::DebugLog("npu-compute", "CSV hardware count fallback unavailable");
        return;
    }
    config->aicCoreCount = aiCubeCount;
    config->aivCoreCount = aiVectorCount;
    npu_compute::detail::DebugLog(
        "npu-compute", "CSV hardware config: source=DeviceAttributes aicCoreCount=%u aivCoreCount=%u",
        config->aicCoreCount, config->aivCoreCount);
}

void LoadCsvHardwareInfoMetadata(PmuCsvConfig* config, bool loadFrequencies)
{
    if (config == nullptr) {
        return;
    }
    const boost::filesystem::path path = boost::filesystem::path(config->outputDirectory) / "HardwareInfo.jsonl";
    std::ifstream input(path.string());
    if (!input.is_open()) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV hardware metadata fallback: HardwareInfo unavailable path=%s", path.c_str());
        LoadCsvAiCoreCountsFromDevice(config);
        return;
    }
    const std::string jsonl((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    HardwareInfoFrequencies frequencies;
    std::string error;
    if (!ParseHardwareInfoFrequenciesJsonl(jsonl, &frequencies, &error)) {
        npu_compute::detail::DebugLog(
            "npu-compute", "CSV hardware metadata fallback: parse HardwareInfo failed path=%s reason=%s", path.c_str(),
            error.c_str());
        LoadCsvAiCoreCountsFromDevice(config);
        return;
    }
    if (loadFrequencies) {
        config->aicFrequencyMhz = static_cast<double>(frequencies.aiCubeFrequencyMhz);
        config->aivFrequencyMhz = static_cast<double>(frequencies.aiVectorFrequencyMhz);
    }
    config->aicCoreCount = frequencies.aiCubeCount;
    config->aivCoreCount = frequencies.aiVectorCount;
    npu_compute::detail::DebugLog(
        "npu-compute",
        "CSV hardware config: source=HardwareInfo fallbackFrequency=%f aicFrequency=%f "
        "aivFrequency=%f aicCoreCount=%u aivCoreCount=%u",
        config->frequencyMhz, config->aicFrequencyMhz, config->aivFrequencyMhz, config->aicCoreCount,
        config->aivCoreCount);
}

} // namespace

namespace {

aclptiResult OnDataModuleShutdown(void* userData)
{
    auto* runtime = static_cast<NpuComputeRuntime*>(userData);
    if (runtime == nullptr) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    return runtime->ShutdownAfterPtiDrain() == 0 ? ACLPTI_SUCCESS : ACLPTI_ERROR_INTERNAL;
}

} // namespace

NpuComputeRuntime& NpuComputeRuntime::Instance()
{
    static NpuComputeRuntime instance;
    return instance;
}

int NpuComputeRuntime::Initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string error;
    boost::filesystem::path outputDirectory;
    if (!LoadOutputDirectory(&outputDirectory, &error)) {
        std::fprintf(stderr, "[libnpu-compute] invalid NPU_COMPUTE_OUTPUT: %s\n", error.c_str());
        return kInitializeFailed;
    }
    if (!section_config_.LoadFromEnvironment("NPU_COMPUTE_SECTIONS", &error)) {
        std::fprintf(stderr, "[libnpu-compute] invalid NPU_COMPUTE_SECTIONS: %s\n", error.c_str());
        return kInitializeFailed;
    }
    csv_config_.outputDirectory = outputDirectory.string();
    csv_config_.mirrorOutputDirectory.clear();
    if (const char* mirrorOutputDirectory = std::getenv("NPU_COMPUTE_CSV_OUTPUT_DIR");
        mirrorOutputDirectory != nullptr && mirrorOutputDirectory[0] != '\0') {
        csv_config_.mirrorOutputDirectory = mirrorOutputDirectory;
    }
    csv_frequency_override_ = false;
    csv_hardware_metadata_loaded_ = false;
    csv_config_.frequencyMhz = kMsopprofA5FallbackFrequencyMhz;
    csv_config_.aicFrequencyMhz = 0.0;
    csv_config_.aivFrequencyMhz = 0.0;
    csv_config_.aicCoreCount = 0;
    csv_config_.aivCoreCount = 0;
    if (const char* frequency = std::getenv("NPU_COMPUTE_FREQUENCY_MHZ");
        frequency != nullptr && frequency[0] != '\0') {
        double parsed = 0.0;
        if (!ParsePositiveDouble(frequency, &parsed)) {
            std::fprintf(stderr, "[libnpu-compute] invalid NPU_COMPUTE_FREQUENCY_MHZ: %s\n", frequency);
            return kInitializeFailed;
        }
        csv_config_.frequencyMhz = parsed;
        csv_config_.aicFrequencyMhz = parsed;
        csv_config_.aivFrequencyMhz = parsed;
        csv_frequency_override_ = true;
    }
    if (const char* socName = std::getenv("NPU_COMPUTE_SOC"); socName != nullptr && socName[0] != '\0') {
        csv_config_.socName = socName;
    }

    auto consumer = PmuDataConsumer::Create(
        [this](std::shared_ptr<const aclptiProfilingDataResult> result) { return ProcessPmuData(std::move(result)); });
    if (consumer == nullptr || consumer->Start() != ACLPTI_SUCCESS) {
        return kInitializeFailed;
    }
    std::weak_ptr<PmuDataConsumer> weakConsumer = consumer;
    const aclptiResult registerStatus =
        aclptiRegisterProfilingDataCallback([weakConsumer](std::shared_ptr<const aclptiProfilingDataResult> result) {
            const auto activeConsumer = weakConsumer.lock();
            return activeConsumer == nullptr ? ACLPTI_ERROR_INVALID_STATE : activeConsumer->Submit(std::move(result));
        });
    if (registerStatus != ACLPTI_SUCCESS) {
        consumer->ShutdownAndDrain();
        return kInitializeFailed;
    }
    const aclptiResult shutdownRegisterStatus = aclptiRegisterDataModuleShutdownCallback(&OnDataModuleShutdown, this);
    if (shutdownRegisterStatus != ACLPTI_SUCCESS) {
        consumer->ShutdownAndDrain();
        return kInitializeFailed;
    }
    pmu_consumer_ = std::move(consumer);

    if (!hardware_info_collector_.Initialize(outputDirectory, &error)) {
        std::fprintf(stderr, "[libnpu-compute] initialize HardwareInfo collector failed: %s\n", error.c_str());
        pmu_consumer_->ShutdownAndDrain();
        pmu_consumer_.reset();
        return kInitializeFailed;
    }

    aclptiResult result = aclptiSubscribe(
        &subscriber_, &NpuComputeRuntime::HardwareInfoTriggerCallback, static_cast<void*>(&hardware_info_collector_),
        nullptr);
    if (result != ACLPTI_SUCCESS) {
        std::fprintf(stderr, "[libnpu-compute] aclptiSubscribe failed: %d\n", result);
        pmu_consumer_->ShutdownAndDrain();
        pmu_consumer_.reset();
        hardware_info_collector_.Stop();
        return result;
    }
    if (subscriber_ == nullptr) {
        std::fprintf(stderr, "[libnpu-compute] aclptiSubscribe returned a null handle\n");
        pmu_consumer_->ShutdownAndDrain();
        pmu_consumer_.reset();
        hardware_info_collector_.Stop();
        return kInitializeFailed;
    }
    std::fprintf(stderr, "[libnpu-compute] subscriber initialized\n");

    enabled_hardware_callback_count_ = 0;
    for (aclptiCallbackId cbid : kHardwareInfoTriggerCallbackIds) {
        result = aclptiEnableCallback(true, subscriber_, ACLPTI_CB_DOMAIN_RUNTIME_API, cbid);
        if (result != ACLPTI_SUCCESS) {
            std::fprintf(stderr, "[libnpu-compute] aclptiEnableCallback failed for cbid=%u: %d\n", cbid, result);
            DisableHardwareCallbacks();
            pmu_consumer_->ShutdownAndDrain();
            pmu_consumer_.reset();
            hardware_info_collector_.Stop();
            return result;
        }
        ++enabled_hardware_callback_count_;
        if (DebugEnabled()) {
            std::fprintf(stderr, "[libnpu-compute] enabled ACL PTI callback cbid=%u\n", cbid);
        }
    }

    result = aclptiRangeProfilerSetConfig(section_config_.Params());
    if (result != ACLPTI_SUCCESS) {
        std::fprintf(stderr, "[libnpu-compute] aclptiRangeProfilerSetConfig failed: %d\n", result);
        DisableHardwareCallbacks();
        pmu_consumer_->ShutdownAndDrain();
        pmu_consumer_.reset();
        hardware_info_collector_.Stop();
        return result;
    }
    std::fprintf(stderr, "[libnpu-compute] configured sections=%s\n", section_config_.JoinedSections().c_str());
    return ACLPTI_SUCCESS;
}

void NpuComputeRuntime::Stop() noexcept
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        DisableHardwareCallbacks();
    }
    hardware_info_collector_.Stop();
}

void NpuComputeRuntime::DisableHardwareCallbacks() noexcept
{
    while (enabled_hardware_callback_count_ > 0) {
        const aclptiCallbackId cbid = kHardwareInfoTriggerCallbackIds[enabled_hardware_callback_count_ - 1];
        const aclptiResult result = aclptiEnableCallback(false, subscriber_, ACLPTI_CB_DOMAIN_RUNTIME_API, cbid);
        if (result != ACLPTI_SUCCESS) {
            std::fprintf(stderr, "[libnpu-compute] disable ACL PTI callback failed for cbid=%u: %d\n", cbid, result);
        } else if (DebugEnabled()) {
            std::fprintf(stderr, "[libnpu-compute] disabled ACL PTI callback cbid=%u\n", cbid);
        }
        --enabled_hardware_callback_count_;
    }
}

int NpuComputeRuntime::ShutdownAfterPtiDrain()
{
    std::shared_ptr<PmuDataConsumer> consumer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        consumer = std::move(pmu_consumer_);
    }
    if (consumer == nullptr) {
        return 0;
    }
    return consumer->ShutdownAndDrain() == ACLPTI_SUCCESS ? 0 : kInitializeFailed;
}

aclptiResult NpuComputeRuntime::ProcessPmuData(std::shared_ptr<const aclptiProfilingDataResult> result)
{
    if (result == nullptr) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    for (const auto& [blockKey, row] : result->pmuLogs) {
        if (blockKey.blockId != row.blockId || blockKey.subBlockId != row.subBlockId ||
            blockKey.coreType != row.coreType || blockKey.coreId != row.coreId) {
            return ACLPTI_ERROR_ASSEMBLE;
        }
    }
    PmuCsvConfig csvConfig;
    // Kernel EXIT is delivered after PMU processing on the replay call path.
    // Stopping here would suppress the callback that publishes HardwareInfo.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!csv_hardware_metadata_loaded_) {
            LoadCsvHardwareInfoMetadata(&csv_config_, !csv_frequency_override_);
            csv_hardware_metadata_loaded_ = true;
        }
        csvConfig = csv_config_;
    }
    const aclptiResult csvStatus = PmuCsvWriter::Write(*result, section_config_.Sections(), csvConfig);
    if (csvStatus != ACLPTI_SUCCESS) {
        return csvStatus;
    }
    // Upstream decode failures are retained in result->status/errorStats while
    // valid PMU rows are still written. The callback status reflects only CSV
    // processing, not whether the source aggregate was complete.
    return ACLPTI_SUCCESS;
}

void NpuComputeRuntime::HardwareInfoTriggerCallback(
    void* userData, aclptiCallbackDomain domain, aclptiCallbackId cbid, const aclptiCallbackData* callbackData) noexcept
{
    if (userData == nullptr) {
        std::fprintf(stderr, "[libnpu-compute] HardwareInfo trigger callback received null userData\n");
        return;
    }
    if (domain != ACLPTI_CB_DOMAIN_RUNTIME_API || callbackData == nullptr || callbackData->domain != domain ||
        callbackData->cbid != cbid || !IsHardwareInfoTriggerCallback(cbid)) {
        return;
    }
    const bool accepted = callbackData->callbackSite == ACLPTI_API_EXIT && callbackData->retval == ACL_SUCCESS;
    if (DebugEnabled()) {
        std::fprintf(
            stderr, "[libnpu-compute] runtime callback domain=%d cbid=%u site=%d retval=%d accepted=%d\n",
            static_cast<int>(domain), cbid, static_cast<int>(callbackData->callbackSite),
            static_cast<int>(callbackData->retval), accepted ? 1 : 0);
    }
    if (!accepted) {
        return;
    }
    try {
        auto* collector = static_cast<HardwareInfoCollector*>(userData);
        collector->CollectOnKernelLaunch();
    } catch (...) {
        std::fprintf(stderr, "[libnpu-compute] HardwareInfo trigger callback failed\n");
    }
}

} // namespace npu_compute
