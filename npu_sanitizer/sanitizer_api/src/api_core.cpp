/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "api_core.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace ascsan {
namespace {

thread_local bool g_insideCallback = false;

class CallbackGuard {
public:
    CallbackGuard() : old_(g_insideCallback) { g_insideCallback = true; }

    ~CallbackGuard() { g_insideCallback = old_; }

private:
    bool old_;
};

void EnsureDir(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        std::cerr << "[ascsan-api] mkdir failed path=" << path << " errno=" << errno << "\n";
    }
}

AscsanStatus ReadFull(int fd, void* buf, size_t bytes)
{
    auto* cursor = static_cast<char*>(buf);
    size_t total = 0;
    while (total < bytes) {
        const ssize_t rc = read(fd, cursor + total, bytes - total);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ASCSAN_STATUS_ERROR_IO;
        }
        if (rc == 0) {
            return ASCSAN_STATUS_ERROR_IO;
        }
        total += static_cast<size_t>(rc);
    }
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus WriteFull(int fd, const void* buf, size_t bytes)
{
    const auto* cursor = static_cast<const char*>(buf);
    size_t total = 0;
    while (total < bytes) {
        const ssize_t rc = write(fd, cursor + total, bytes - total);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ASCSAN_STATUS_ERROR_IO;
        }
        total += static_cast<size_t>(rc);
    }
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace

ApiCore& ApiCore::Instance()
{
    static ApiCore core;
    return core;
}

AscsanStatus ApiCore::Initialize(const AscsanInitParams* params)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (initialized_) {
        return ASCSAN_STATUS_SUCCESS;
    }
    if (params != nullptr) {
        if (params->version != ASCSAN_API_VERSION || params->size < sizeof(AscsanInitParams)) {
            return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
        }
        if (params->launchConfig != nullptr) {
            config_ = *params->launchConfig;
        }
    }
    if (config_.version == 0) {
        config_.version = ASCSAN_API_VERSION;
        config_.size = sizeof(config_);
    }
    EnsureDir(config_.workDir);
    EnsureDir(config_.probeCacheDir);
    initialized_ = true;
    finalized_ = false;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::Finalize()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    FlushReports();
    subscriber_.reset();
    subscriberToken_.reset();
    retiredSubscriberTokens_.clear();
    patchSites_.clear();
    reports_.clear();
    for (auto& entry : memories_) {
        std::free(entry.first);
    }
    memories_.clear();
    initialized_ = false;
    finalized_ = true;
    return ASCSAN_STATUS_SUCCESS;
}

const char* ApiCore::VersionString() const { return "ascsan sanitizer_api p0"; }

AscsanStatus ApiCore::ExportLaunchConfigToFd(const AscsanLaunchConfig* config, int fd)
{
    if (config == nullptr || fd < 0) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    return WriteFull(fd, config, sizeof(*config));
}

AscsanStatus ApiCore::ImportLaunchConfigFromFd(int fd, AscsanLaunchConfig* config)
{
    if (config == nullptr || fd < 0) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    AscsanStatus status = ReadFull(fd, config, sizeof(*config));
    close(fd);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }
    if (config->version != ASCSAN_API_VERSION || config->size != sizeof(*config)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    config_ = *config;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::ApplyLaunchConfig(const AscsanLaunchConfig* config)
{
    if (config == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_) {
        return ASCSAN_STATUS_ERROR_NOT_INITIALIZED;
    }
    if (config->version != ASCSAN_API_VERSION || config->size != sizeof(*config)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    config_ = *config;
    EnsureDir(config_.workDir);
    EnsureDir(config_.probeCacheDir);
    return ASCSAN_STATUS_SUCCESS;
}

const AscsanLaunchConfig* ApiCore::GetLaunchConfig() const { return &config_; }

AscsanStatus ApiCore::ValidateInitialized() const
{
    return initialized_ && !finalized_ ? ASCSAN_STATUS_SUCCESS : ASCSAN_STATUS_ERROR_NOT_INITIALIZED;
}

bool ApiCore::IsInsideCallback() const { return g_insideCallback; }

void ApiCore::Dispatch(AscsanCallbackDomain domain, uint32_t cbid, const void* cbdata)
{
    Subscriber target{};
    bool hasTarget = false;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (subscriber_.has_value() && subscriber_->callback != nullptr &&
            (subscriber_->enabledDomains.count(domain) != 0 ||
             subscriber_->enabledCallbacks.count({domain, cbid}) != 0)) {
            target = *subscriber_;
            hasTarget = true;
        }
    }
    if (!hasTarget) {
        return;
    }

    CallbackGuard guard;
    target.callback(target.userdata, domain, cbid, cbdata);
}

AscsanStatus ApiCore::ReportError(const char* tool, const char* message)
{
    std::string report = "[";
    report += tool != nullptr ? tool : "unknown";
    report += "] ERROR ";
    report += message != nullptr ? message : "<null>";
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        reports_.push_back(report);
    }
    std::cerr << "[ascsan-api] " << report << "\n";

    AscsanReportData reportData{};
    reportData.common.version = ASCSAN_API_VERSION;
    reportData.common.size = sizeof(reportData);
    reportData.common.apiName = "ascsanReportError";
    reportData.tool = tool;
    reportData.message = message;
    AscsanErrorData errorData{};
    errorData.common.version = ASCSAN_API_VERSION;
    errorData.common.size = sizeof(errorData);
    errorData.common.apiName = "ascsanReportError";
    errorData.tool = tool;
    errorData.message = message;
    Dispatch(ASCSAN_CB_DOMAIN_REPORT, ASCSAN_CBID_REPORT_RECORD, &reportData);
    Dispatch(ASCSAN_CB_DOMAIN_ERROR, ASCSAN_CBID_ERROR_RECORD, &errorData);
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::FlushReports()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::cout << "[ascsan-api] summary: reports=" << reports_.size() << "\n";
    for (const auto& report : reports_) {
        std::cout << "[ascsan-api] summary item: " << report << "\n";
    }
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace ascsan
