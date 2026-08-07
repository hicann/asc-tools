#ifndef ASCSAN_INTERNAL_API_H
#define ASCSAN_INTERNAL_API_H

#include "ascsan/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal SPI for sanitizer_api.so providers.
 *
 * These entry points are intentionally separated from ascsan_api.h. They are
 * used by in-repo provider modules such as injection.so, cann_sanitizer.so,
 * trace fetch backends, and tests. They are not part of the stable checker API.
 */

typedef struct AscsanRuntimeMemoryAllocParams {
    uint32_t version;
    uint32_t size;
    void *ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AscsanRuntimeMemoryAllocParams;

typedef struct AscsanRuntimeMemoryFreeParams {
    uint32_t version;
    uint32_t size;
    void *ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AscsanRuntimeMemoryFreeParams;

typedef struct AscsanRuntimeMemcpyParams {
    uint32_t version;
    uint32_t size;
    void *dst;
    uint64_t dstMax;
    const void *src;
    uint64_t bytes;
    uint32_t kind;
    void *stream;
} AscsanRuntimeMemcpyParams;

typedef struct AscsanRuntimeMemsetParams {
    uint32_t version;
    uint32_t size;
    void *dst;
    uint64_t dstMax;
    int32_t value;
    uint64_t bytes;
    void *stream;
} AscsanRuntimeMemsetParams;

const char *ascsanGetVersionString(void);

AscsanStatus ascsanExportLaunchConfigToFd(const AscsanLaunchConfig *config, int fd);
AscsanStatus ascsanImportLaunchConfigFromFd(int fd, AscsanLaunchConfig *config);
AscsanStatus ascsanApplyLaunchConfig(const AscsanLaunchConfig *config);
const AscsanLaunchConfig *ascsanGetLaunchConfig(void);

AscsanStatus ascsanOnRuntimeEvent(const AscsanRuntimeEvent *event);
AscsanStatus ascsanConfigureRuntimeHook(const AscsanRuntimeHookPlan *plan);
AscsanStatus ascsanGetRuntimeHookState(AscsanRuntimeHookState *state);

AscsanStatus ascsanIngestRawTraces(const AscsanRawTraceRecord *records, uint64_t count);
AscsanStatus ascsanReportError(const char *tool, const char *message);
AscsanStatus ascsanFlushReports(void);

#ifdef __cplusplus
}
#endif

#endif
