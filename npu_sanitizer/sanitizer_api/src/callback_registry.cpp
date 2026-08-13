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

namespace aclsan {
namespace {

constexpr uint64_t kSubscriberTokenMagic = 0x41434c53414e5355ull;

} // namespace

AclsanStatus ApiCore::Subscribe(const AclsanSubscribeDesc* desc, AclsanSubscriberHandle* subscriber)
{
    if (desc == nullptr || subscriber == nullptr || desc->callback == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (subscriber_.has_value()) {
        return ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED;
    }

    auto token = std::make_unique<AclsanSubscriberToken_st>();
    token->magic = kSubscriberTokenMagic;
    token->generation = nextSubscriberGeneration_++;
    token->active = true;

    Subscriber sub{};
    sub.handle = token.get();
    sub.name = desc->name != nullptr ? desc->name : "";
    sub.callback = desc->callback;
    sub.userdata = desc->userdata;
    sub.flags = desc->flags;
    subscriberToken_ = std::move(token);
    subscriber_ = sub;
    *subscriber = sub.handle;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::Unsubscribe(AclsanSubscriberHandle subscriber)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto* token = ValidateSubscriberLocked(subscriber);
    if (token == nullptr) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    token->active = false;
    token->magic = 0;
    subscriber_.reset();
    if (subscriberToken_) {
        retiredSubscriberTokens_.push_back(std::move(subscriberToken_));
    }
    ReconfigureHookPlan();
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::EnableCallback(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, bool enable)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!IsKnownCbid(domain, cbid)) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (ValidateSubscriberLocked(subscriber) == nullptr) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    const auto key = std::make_pair(domain, cbid);
    if (enable) {
        subscriber_->enabledCallbacks.insert(key);
    } else {
        subscriber_->enabledCallbacks.erase(key);
    }
    ReconfigureHookPlan();
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::EnableDomain(AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, bool enable)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!IsSupportedDomain(domain)) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (ValidateSubscriberLocked(subscriber) == nullptr) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    if (enable) {
        subscriber_->enabledDomains.insert(domain);
    } else {
        subscriber_->enabledDomains.erase(domain);
    }
    ReconfigureHookPlan();
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::GetCallbackState(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, int* enabled) const
{
    if (enabled == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (ValidateSubscriberLocked(subscriber) == nullptr) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    *enabled =
        subscriber_->enabledDomains.count(domain) != 0 || subscriber_->enabledCallbacks.count({domain, cbid}) != 0;
    return ACLSAN_STATUS_SUCCESS;
}

bool ApiCore::IsSupportedDomain(AclsanCallbackDomain domain) const
{
    return domain >= ACLSAN_CB_DOMAIN_RESOURCE && domain <= ACLSAN_CB_DOMAIN_ERROR;
}

bool ApiCore::IsKnownCbid(AclsanCallbackDomain domain, uint32_t cbid) const
{
    if (!IsSupportedDomain(domain)) {
        return false;
    }
    switch (domain) {
        case ACLSAN_CB_DOMAIN_RESOURCE:
            return cbid >= ACLSAN_CBID_RESOURCE_MEMORY_ALLOC && cbid <= ACLSAN_CBID_RESOURCE_FUNCTION_GET;
        case ACLSAN_CB_DOMAIN_MEMORY:
            return cbid >= ACLSAN_CBID_MEMORY_MEMCPY_BEGIN && cbid <= ACLSAN_CBID_MEMORY_MEMSET_END;
        case ACLSAN_CB_DOMAIN_BINARY:
            return cbid >= ACLSAN_CBID_BINARY_LOAD_BEGIN && cbid <= ACLSAN_CBID_BINARY_LOAD_END;
        case ACLSAN_CB_DOMAIN_PATCH:
            return cbid >= ACLSAN_CBID_PATCH_BEGIN && cbid <= ACLSAN_CBID_PATCH_SITE_MAP_CREATED;
        case ACLSAN_CB_DOMAIN_LAUNCH:
            return cbid >= ACLSAN_CBID_LAUNCH_BEGIN && cbid <= ACLSAN_CBID_LAUNCH_END;
        case ACLSAN_CB_DOMAIN_SYNCHRONIZE:
            return cbid >= ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END && cbid <= ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END;
        case ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION:
            return cbid >= ACLSAN_CBID_DEVICE_MEMORY_ACCESS && cbid <= ACLSAN_CBID_DEVICE_ERROR;
        case ACLSAN_CB_DOMAIN_REPORT:
            return cbid == ACLSAN_CBID_REPORT_RECORD;
        case ACLSAN_CB_DOMAIN_ERROR:
            return cbid == ACLSAN_CBID_ERROR_RECORD;
        default:
            return false;
    }
}

AclsanSubscriberToken_st* ApiCore::ValidateSubscriberLocked(AclsanSubscriberHandle subscriber) const
{
    if (subscriber == ACLSAN_INVALID_SUBSCRIBER_HANDLE) {
        return nullptr;
    }
    if (!subscriber_.has_value() || subscriber_->handle != subscriber) {
        return nullptr;
    }
    if (!subscriberToken_ || subscriberToken_.get() != subscriber) {
        return nullptr;
    }
    if (!subscriber->active || subscriber->magic != kSubscriberTokenMagic) {
        return nullptr;
    }
    return subscriber;
}

bool ApiCore::HasEnabledCallbackLocked(AclsanCallbackDomain domain, uint32_t cbid) const
{
    if (!subscriber_.has_value()) {
        return false;
    }
    return subscriber_->enabledDomains.count(domain) != 0 || subscriber_->enabledCallbacks.count({domain, cbid}) != 0;
}

bool ApiCore::HasEnabledDomainLocked(AclsanCallbackDomain domain) const
{
    if (!subscriber_.has_value()) {
        return false;
    }
    if (subscriber_->enabledDomains.count(domain) != 0) {
        return true;
    }
    for (const auto& entry : subscriber_->enabledCallbacks) {
        if (entry.first == domain) {
            return true;
        }
    }
    return false;
}

} // namespace aclsan
