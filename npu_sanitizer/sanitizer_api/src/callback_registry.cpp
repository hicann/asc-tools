#include "api_core.h"

namespace ascsan {
namespace {

constexpr uint64_t kSubscriberTokenMagic = 0x41534353414e5355ull;

} // namespace

AscsanStatus ApiCore::Subscribe(const AscsanSubscribeDesc *desc, AscsanSubscriberHandle *subscriber)
{
    if (desc == nullptr || subscriber == nullptr || desc->callback == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (subscriber_.has_value()) {
        return ASCSAN_STATUS_ERROR_MAX_LIMIT_REACHED;
    }

    auto token = std::make_unique<AscsanSubscriberToken_st>();
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
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::Unsubscribe(AscsanSubscriberHandle subscriber)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto *token = ValidateSubscriberLocked(subscriber);
    if (token == nullptr) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    token->active = false;
    token->magic = 0;
    subscriber_.reset();
    if (subscriberToken_) {
        retiredSubscriberTokens_.push_back(std::move(subscriberToken_));
    }
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
    if (ValidateSubscriberLocked(subscriber) == nullptr) {
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
    if (ValidateSubscriberLocked(subscriber) == nullptr) {
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
    if (ValidateSubscriberLocked(subscriber) == nullptr) {
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
    switch (domain) {
        case ASCSAN_CB_DOMAIN_RESOURCE:
            return cbid >= ASCSAN_CBID_RESOURCE_MEMORY_ALLOC &&
                   cbid <= ASCSAN_CBID_RESOURCE_FUNCTION_GET;
        case ASCSAN_CB_DOMAIN_MEMORY:
            return cbid >= ASCSAN_CBID_MEMORY_MEMCPY_BEGIN &&
                   cbid <= ASCSAN_CBID_MEMORY_MEMSET_END;
        case ASCSAN_CB_DOMAIN_BINARY:
            return cbid >= ASCSAN_CBID_BINARY_LOAD_BEGIN &&
                   cbid <= ASCSAN_CBID_BINARY_LOAD_END;
        case ASCSAN_CB_DOMAIN_PATCH:
            return cbid >= ASCSAN_CBID_PATCH_BEGIN &&
                   cbid <= ASCSAN_CBID_PATCH_SITE_MAP_CREATED;
        case ASCSAN_CB_DOMAIN_LAUNCH:
            return cbid >= ASCSAN_CBID_LAUNCH_BEGIN &&
                   cbid <= ASCSAN_CBID_LAUNCH_END;
        case ASCSAN_CB_DOMAIN_SYNCHRONIZE:
            return cbid >= ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END &&
                   cbid <= ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END;
        case ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION:
            return cbid >= ASCSAN_CBID_DEVICE_MEMORY_ACCESS &&
                   cbid <= ASCSAN_CBID_DEVICE_ERROR;
        case ASCSAN_CB_DOMAIN_REPORT:
            return cbid == ASCSAN_CBID_REPORT_RECORD;
        case ASCSAN_CB_DOMAIN_ERROR:
            return cbid == ASCSAN_CBID_ERROR_RECORD;
        default:
            return false;
    }
}

AscsanSubscriberToken_st *ApiCore::ValidateSubscriberLocked(AscsanSubscriberHandle subscriber) const
{
    if (subscriber == ASCSAN_INVALID_SUBSCRIBER_HANDLE) {
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
