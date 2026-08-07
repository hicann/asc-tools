#ifndef ASCSAN_CALLBACK_H
#define ASCSAN_CALLBACK_H

#include "ascsan/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AscsanResourceData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    void *ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AscsanResourceData;

typedef struct AscsanMemoryMemcpyData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    void *dst;
    const void *src;
    uint64_t bytes;
    uint32_t kind;
    void *stream;
} AscsanMemoryMemcpyData;

typedef struct AscsanMemoryMemsetData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    void *dst;
    uint64_t bytes;
    int32_t value;
    void *stream;
} AscsanMemoryMemsetData;

typedef struct AscsanBinaryData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    uint64_t binaryId;
    const char *path;
} AscsanBinaryData;

typedef struct AscsanPatchData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    const char *originalPath;
    const char *patchedPath;
    uint64_t binaryId;
    uint64_t patchPlanId;
    uint32_t pipelineMask;
} AscsanPatchData;

typedef struct AscsanLaunchData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    uint64_t launchId;
    void *function;
    void *stream;
    const char *functionName;
} AscsanLaunchData;

typedef struct AscsanSynchronizeData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    void *stream;
} AscsanSynchronizeData;

typedef struct AscsanDeviceInstructionData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    uint32_t pipeline;
    uint32_t cbid;
    uint32_t siteId;
    uint32_t blockId;
    uint64_t launchId;
    uint64_t binaryId;
    uint64_t functionId;
    uint64_t pc;
    uint64_t rawArgs[ASCSAN_RAW_ARG_MAX];
} AscsanDeviceInstructionData;

typedef struct AscsanReportData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    const char *tool;
    const char *message;
} AscsanReportData;

typedef struct AscsanErrorData {
    uint32_t version;
    uint32_t size;
    const char *apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
    const char *tool;
    const char *message;
} AscsanErrorData;

typedef void (*AscsanCallbackFunc)(
    void *userdata,
    AscsanCallbackDomain domain,
    uint32_t cbid,
    const void *cbdata);

typedef struct AscsanSubscribeDesc {
    uint32_t version;
    uint32_t size;
    const char *name;
    AscsanCallbackFunc callback;
    void *userdata;
    uint64_t flags;
} AscsanSubscribeDesc;

AscsanStatus ascsanSubscribe(const AscsanSubscribeDesc *desc, AscsanSubscriberHandle *subscriber);
AscsanStatus ascsanUnsubscribe(AscsanSubscriberHandle subscriber);
AscsanStatus ascsanEnableCallback(AscsanSubscriberHandle subscriber,
                                  AscsanCallbackDomain domain,
                                  uint32_t cbid,
                                  int enable);
AscsanStatus ascsanEnableDomain(AscsanSubscriberHandle subscriber,
                                AscsanCallbackDomain domain,
                                int enable);
AscsanStatus ascsanGetCallbackState(AscsanSubscriberHandle subscriber,
                                    AscsanCallbackDomain domain,
                                    uint32_t cbid,
                                    int *enabled);
int ascsanIsInsideCallback(void);

#ifdef __cplusplus
}
#endif

#endif
