#ifndef ASCSAN_MEMORY_H
#define ASCSAN_MEMORY_H

#include "ascsan/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AscsanMemoryAllocDesc {
    uint32_t version;
    uint32_t size;
    AscsanMemorySpace space;
    uint32_t deviceId;
    uint64_t bytes;
    uint64_t alignment;
    uint64_t flags;
    void *stream;
} AscsanMemoryAllocDesc;

typedef struct AscsanMemoryInfo {
    uint32_t version;
    uint32_t size;
    void *ptr;
    uint64_t bytes;
    AscsanMemorySpace space;
    uint32_t deviceId;
    uint64_t memoryId;
    uint64_t flags;
} AscsanMemoryInfo;

AscsanStatus ascsanMemoryAlloc(const AscsanMemoryAllocDesc *desc,
                               void **ptr,
                               AscsanMemoryHandle *memory);
AscsanStatus ascsanMemoryFree(void *ptr);
AscsanStatus ascsanMemoryMemcpy(void *dst,
                                uint64_t dstMax,
                                const void *src,
                                uint64_t bytes,
                                AscsanMemcpyKind kind);
AscsanStatus ascsanMemoryMemcpyAsync(void *dst,
                                     uint64_t dstMax,
                                     const void *src,
                                     uint64_t bytes,
                                     AscsanMemcpyKind kind,
                                     void *stream);
AscsanStatus ascsanMemoryMemset(void *dst, uint64_t dstMax, int32_t value, uint64_t bytes);
AscsanStatus ascsanMemoryMemsetAsync(void *dst,
                                     uint64_t dstMax,
                                     int32_t value,
                                     uint64_t bytes,
                                     void *stream);
AscsanStatus ascsanMemorySynchronizeStream(void *stream);
AscsanStatus ascsanMemoryGetInfo(const void *ptr, AscsanMemoryInfo *info);

AscsanStatus ascsanDeviceMalloc(void **devPtr, uint64_t bytes);
AscsanStatus ascsanDeviceFree(void *devPtr);
AscsanStatus ascsanMemcpyD2H(void *dstHost, const void *srcDevice, uint64_t bytes);
AscsanStatus ascsanMemcpyH2D(void *dstDevice, const void *srcHost, uint64_t bytes);
AscsanStatus ascsanStreamSynchronize(void *stream);

#ifdef __cplusplus
}
#endif

#endif

