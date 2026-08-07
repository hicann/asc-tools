#ifndef ASCSAN_API_H
#define ASCSAN_API_H

#include "ascsan/ascsan_callback.h"
#include "ascsan/ascsan_memory.h"
#include "ascsan/ascsan_patch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AscsanInitParams {
    uint32_t version;
    uint32_t size;
    const AscsanLaunchConfig *launchConfig;
    const void *runtimeInitInfo;
    const char *installRoot;
    const char *workDir;
    uint64_t flags;
} AscsanInitParams;

AscsanStatus ascsanInitialize(const AscsanInitParams *params);
AscsanStatus ascsanFinalize(void);

#ifdef __cplusplus
}
#endif

#endif
