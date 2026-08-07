#include "api_core.h"

namespace ascsan {

AscsanStatus ApiCore::Subscribe(const AscsanSubscribeDesc *desc, AscsanSubscriberHandle *subscriber)
{
    if (desc == nullptr || subscriber == nullptr || desc->callback == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (subscriber_.has_value()) {
        return ASCSAN_STATUS_ERROR_MAX_LIMIT_REACHED;
    }
    Subscriber sub{};
    sub.handle = nextSubscriber_++;
    sub.name = desc->name != nullptr ? desc->name : "";
    sub.callback = desc->callback;
    sub.userdata = desc->userdata;
    subscriber_ = sub;
    *subscriber = sub.handle;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::Unsubscribe(AscsanSubscriberHandle subscriber)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!subscriber_.has_value() || subscriber_->handle != subscriber) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    subscriber_.reset();
    ReconfigureHookPlan();
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::EnableCallback(AscsanSubscriberHandle subscriber,
                                     AscsanCallbackDomain domain,
                                     uint32_t cbid,
                                     bool enable)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!IsKnownCbid(domain, cbid)) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (!subscriber_.has_value() || subscriber_->handle != subscriber) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    const auto key = std::make_pair(domain, cbid);
    if (enable) {
        subscriber_->enabledCallbacks.insert(key);
    } else {
        subscriber_->enabledCallbacks.erase(key);
    }
    ReconfigureHookPlan();
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::EnableDomain(AscsanSubscriberHandle subscriber,
                                   AscsanCallbackDomain domain,
                                   bool enable)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!IsSupportedDomain(domain)) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (!subscriber_.has_value() || subscriber_->handle != subscriber) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    if (enable) {
        subscriber_->enabledDomains.insert(domain);
    } else {
        subscriber_->enabledDomains.erase(domain);
    }
    ReconfigureHookPlan();
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::GetCallbackState(AscsanSubscriberHandle subscriber,
                                       AscsanCallbackDomain domain,
                                       uint32_t cbid,
                                       int *enabled) const
{
    if (enabled == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!subscriber_.has_value() || subscriber_->handle != subscriber) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    *enabled = subscriber_->enabledDomains.count(domain) != 0 ||
               subscriber_->enabledCallbacks.count({domain, cbid}) != 0;
    return ASCSAN_STATUS_SUCCESS;
}

bool ApiCore::IsSupportedDomain(AscsanCallbackDomain domain) const
{
    return domain >= ASCSAN_CB_DOMAIN_RESOURCE && domain <= ASCSAN_CB_DOMAIN_ERROR;
}

bool ApiCore::IsKnownCbid(AscsanCallbackDomain domain, uint32_t cbid) const
{
    if (!IsSupportedDomain(domain)) {
        return false;
    }
    if (cbid == 0) {
        return false;
    }
    return true;
}

bool ApiCore::HasEnabledCallbackLocked(AscsanCallbackDomain domain, uint32_t cbid) const
{
    if (!subscriber_.has_value()) {
        return false;
    }
    return subscriber_->enabledDomains.count(domain) != 0 ||
           subscriber_->enabledCallbacks.count({domain, cbid}) != 0;
}

bool ApiCore::HasEnabledDomainLocked(AscsanCallbackDomain domain) const
{
    if (!subscriber_.has_value()) {
        return false;
    }
    if (subscriber_->enabledDomains.count(domain) != 0) {
        return true;
    }
    for (const auto &entry : subscriber_->enabledCallbacks) {
        if (entry.first == domain) {
            return true;
        }
    }
    return false;
}

} // namespace ascsan
