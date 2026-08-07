#ifndef ASCSAN_CANN_SANITIZER_H
#define ASCSAN_CANN_SANITIZER_H

#include "ascsan/ascsan_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASCSAN_CANN_EVENT_NAME_MAX 128

typedef struct AscsanCannSanitizerStats {
    uint32_t version;
    uint32_t size;
    char toolName[ASCSAN_TOOL_NAME_MAX];
    char lastApiName[ASCSAN_CANN_EVENT_NAME_MAX];
    char lastInstructionOpName[ASCSAN_CANN_EVENT_NAME_MAX];
    uint64_t callbacks;
    uint64_t resourceCallbacks;
    uint64_t memoryCallbacks;
    uint64_t binaryCallbacks;
    uint64_t patchCallbacks;
    uint64_t launchCallbacks;
    uint64_t syncCallbacks;
    uint64_t deviceInstructionCallbacks;
    uint64_t reportCallbacks;
    uint64_t errorCallbacks;
    uint64_t memoryTransferEvents;
    uint64_t syncEvents;
    uint64_t fixpipeEvents;
    uint64_t checkerEvents;
    uint64_t checkerInstructions;
    uint64_t checkerWindows;
    uint64_t checkerCompletedWindows;
    uint64_t checkerReports;
    uint32_t lastDomain;
    uint32_t lastCbid;
    int lastResult;
    uint64_t lastCorrelationId;
    uint64_t lastResourceId;
    uint64_t lastResourceBytes;
    uint32_t lastResourceMemorySpace;
    uint32_t lastResourceDeviceId;
    uint64_t lastMemoryBytes;
    uint64_t lastMemorySrc;
    uint64_t lastMemoryDst;
    uint32_t lastMemoryKind;
    uint64_t lastPatchBinaryId;
    uint64_t lastPatchPlanId;
    uint32_t lastPatchPipelineMask;
    uint32_t lastInstructionSiteId;
    uint32_t lastInstructionPipeline;
    uint64_t lastInstructionPc;
    uint64_t lastInstructionSrc;
    uint64_t lastInstructionDst;
    uint64_t lastInstructionBytes;
} AscsanCannSanitizerStats;

AscsanStatus ascsanCannSanitizerInitialize(const AscsanLaunchConfig *config);
AscsanStatus ascsanCannSanitizerFinalize(void);
AscsanStatus ascsanCannSanitizerGetStats(AscsanCannSanitizerStats *stats);

/*
 * Injection entry points. P0 accepts a nullable initInfo and imports
 * AscsanLaunchConfig from ASCSAN_CONFIG_FD when no explicit config is passed.
 */
int acltoolInitalize(const void *initInfo);
void CannComputeInit(void);

#ifdef __cplusplus
}
#endif

#endif
