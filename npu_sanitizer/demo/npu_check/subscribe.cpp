#include "acltool_api.h"
#include "internal/aclsan_log.h"

#include <cstdlib>
#include <cstring>

namespace {

struct ToolContext {
    const char* name;
};

ToolContext g_toolContext{"npucheck"};

// 作用：严格读取工具开关，只接受显式的 0 或 1。
bool ReadToolSwitch(const char* name, bool* enabled)
{
    if (name == nullptr || enabled == nullptr) {
        return false;
    }

    const char* value = std::getenv(name);
    if (value != nullptr && std::strcmp(value, "0") == 0) {
        *enabled = 0;
        return true;
    }
    if (value != nullptr && std::strcmp(value, "1") == 0) {
        *enabled = 1;
        return true;
    }
    return false;
}

// 作用：把固定的 domain/id 映射为统一 callback 使用的事件标签。
const char* ResolveEventLabel(AclsanCallbackDomain domain, AclsanCallbackId id)
{
    if (domain == ACLSAN_CB_DOMAIN_RESOURCE) {
        if (id == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
            return "memcheck:aclrtMalloc";
        }
    }
    if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION) {
        if (id == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
            return "memcheck:DataCopy";
        }
        if (id == ACLSAN_CBID_DEVICE_SYNC) {
            return "synccheck:DeviceSync";
        }
    }
    if (domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE && id == ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END) {
        return "synccheck:StreamSyncEnd";
    }
    return nullptr;
}

bool IsValidCommonData(const AclsanCallbackCommonData& common, std::size_t expectedSize)
{
    return common.size >= expectedSize && common.apiName != nullptr;
}

bool IsValidMemoryAccessData(const AclsanDeviceMemoryAccessData* data)
{
    return data != nullptr && data->header.size >= sizeof(AclsanDeviceMemoryAccessData) && data->accessCount != 0 &&
           data->accessIndex < data->accessCount && data->layoutKind == ACLSAN_MEM_LAYOUT_RANGE;
}

void PrintAclrtMallocCallback(const ToolContext& context, const AclsanResourceData& data)
{
    ASC_SAN_INFO(
        "[CALLBACK memcheck:aclrtMalloc] userdata=%s function=%s address=%p size=%llu result=%d", context.name,
        data.common.apiName, data.ptr, static_cast<unsigned long long>(data.bytes), data.common.result);
}

void PrintMemoryAccessCallback(const ToolContext& context, const AclsanDeviceMemoryAccessData& data)
{
    ASC_SAN_INFO(
        "[CALLBACK memcheck:DataCopy] access=%u/%u userdata=%s", data.accessIndex + 1, data.accessCount, context.name);
    ASC_SAN_INFO(
        "[MEMORY ACCESS] exec=%llu pc=0x%llx core=%u block=%u address=0x%llx mode=%u bytes=%llu",
        static_cast<unsigned long long>(data.header.instrExecId), static_cast<unsigned long long>(data.header.pc),
        data.header.coreId, data.header.blockId, static_cast<unsigned long long>(data.address), data.accessMode,
        static_cast<unsigned long long>(data.layout.range.bytes));
}

void PrintSyncCallback(const ToolContext& context, const AclsanDeviceSyncData& data)
{
    ASC_SAN_INFO(
        "[CALLBACK synccheck:DeviceSync] launch=%llu userdata=%s", static_cast<unsigned long long>(data.launchId),
        context.name);
    ASC_SAN_INFO(
        "[SYNC INSTRUCTION] pc=0x%llx exec=%llu block=%u core=%u kind=%u action=%u scope=%u "
        "src_pipe=%u dst_pipe=%u mode=%u object=%llu instr_type=%u",
        static_cast<unsigned long long>(data.pc), static_cast<unsigned long long>(data.instrExecId), data.blockId,
        data.phyCoreId, data.syncKind, data.action, data.scope, data.srcPipe, data.dstPipe, data.mode,
        static_cast<unsigned long long>(data.objectId), data.instrType);
}

void PrintSynchronizeCallback(const ToolContext& context, const AclsanSynchronizeData& data)
{
    ASC_SAN_INFO(
        "[CALLBACK synccheck:StreamSyncEnd] userdata=%s function=%s stream=%p result=%d ready_to_check=%u",
        context.name, data.common.apiName, data.stream, data.common.result, data.common.result == 0 ? 1U : 0U);
}

// 作用：作为 subscriber 的唯一入口，按 domain/id 解析对应 callback data。
void AclsanCallback(void* userdata, AclsanCallbackDomain domain, AclsanCallbackId id, const void* cbdata)
{
    // userdata 保存工具上下文，并由所有 callback route 共享。
    const auto* context = static_cast<const ToolContext*>(userdata);
    const char* label = ResolveEventLabel(domain, id);
    if (context == nullptr || context->name == nullptr || label == nullptr || cbdata == nullptr) {
        ASC_SAN_ERROR(
            "AclsanCallback: unsupported or invalid callback data for domain=%u id=%u", static_cast<uint32_t>(domain),
            static_cast<uint32_t>(id));
        return;
    }

    if (domain == ACLSAN_CB_DOMAIN_RESOURCE && id == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
        const auto* data = static_cast<const AclsanResourceData*>(cbdata);
        if (data == nullptr || !IsValidCommonData(data->common, sizeof(AclsanResourceData))) {
            ASC_SAN_ERROR("AclsanCallback: invalid aclrtMalloc callback data");
            return;
        }
        PrintAclrtMallocCallback(*context, *data);
    } else if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION && id == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
        const auto* data = static_cast<const AclsanDeviceMemoryAccessData*>(cbdata);
        if (!IsValidMemoryAccessData(data)) {
            ASC_SAN_ERROR("AclsanCallback: invalid memory-access data");
            return;
        }
        PrintMemoryAccessCallback(*context, *data);
    } else if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION && id == ACLSAN_CBID_DEVICE_SYNC) {
        const auto* data = static_cast<const AclsanDeviceSyncData*>(cbdata);
        PrintSyncCallback(*context, *data);
    } else if (domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE && id == ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END) {
        const auto* data = static_cast<const AclsanSynchronizeData*>(cbdata);
        if (data == nullptr || !IsValidCommonData(data->common, sizeof(AclsanSynchronizeData))) {
            ASC_SAN_ERROR("AclsanCallback: invalid synchronize callback data");
            return;
        }
        PrintSynchronizeCallback(*context, *data);
    }
}

// 作用：启用一个 callback，失败时打印精确键，供用户定位不支持或无法提交的配置。
AclsanStatus EnableCallback(AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId id)
{
    const AclsanStatus result = aclsanEnableCallback(1, subscriber, domain, id);
    if (result != ACLSAN_STATUS_SUCCESS) {
        ASC_SAN_ERROR(
            "acltoolInitialize: aclsanEnableCallback failed, result=%u, domain=%u, id=%u; Runtime Hook state "
            "was not committed, restart after fixing the error",
            result, static_cast<uint32_t>(domain), static_cast<uint32_t>(id));
    }
    return result;
}

} // namespace

#define CHECK_ACLSAN_STATUS(expression)                      \
    do {                                                     \
        const AclsanStatus checkAclsanStatus = (expression); \
        if (checkAclsanStatus != ACLSAN_STATUS_SUCCESS) {    \
            return checkAclsanStatus;                        \
        }                                                    \
    } while (0)

// 作用：注册统一 callback，并通过逐次 Enable 即时提交所选 callback 对应的 Runtime Hook。
extern "C" AclsanStatus acltoolInitialize(void)
{
    bool memcheckEnabled = 0;
    bool synccheckEnabled = 0;
    if (!ReadToolSwitch("ASC_SAN_MEMCHECK", &memcheckEnabled) ||
        !ReadToolSwitch("ASC_SAN_SYNCCHECK", &synccheckEnabled)) {
        ASC_SAN_ERROR("acltoolInitialize: tool switches must be exactly 0 or 1");
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }

    AclsanSubscriberHandle subscriber = nullptr;
    const AclsanStatus result = aclsanSubscribe(&subscriber, AclsanCallback, &g_toolContext);
    if (result != ACLSAN_STATUS_SUCCESS) {
        ASC_SAN_ERROR("acltoolInitialize: aclsanSubscribe failed, result=%u", result);
        return result;
    }
    if (subscriber == nullptr) {
        ASC_SAN_ERROR("acltoolInitialize: aclsanSubscribe returned success with a null subscriber");
        return ACLSAN_STATUS_ERROR_INTERNAL;
    }
    if (memcheckEnabled != 0) {
        CHECK_ACLSAN_STATUS(EnableCallback(subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC));
        CHECK_ACLSAN_STATUS(
            EnableCallback(subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS));
    }
    if (synccheckEnabled != 0) {
        CHECK_ACLSAN_STATUS(EnableCallback(subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC));
        CHECK_ACLSAN_STATUS(
            EnableCallback(subscriber, ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END));
    }
    return ACLSAN_STATUS_SUCCESS;
}

#undef CHECK_ACLSAN_STATUS
