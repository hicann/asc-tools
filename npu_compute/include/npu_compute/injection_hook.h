/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_COMPUTE_INJECTION_HOOK_H_
#define NPU_COMPUTE_INJECTION_HOOK_H_

#include <stddef.h>
#include <stdint.h>

#include "acl/acl_rt.h"
#include "npu_compute/common.h"
#include "profiling/prof_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef aclError (*aclrtSetDeviceFunc)(int32_t deviceId);
typedef aclError (*aclrtResetDeviceFunc)(int32_t deviceId);
typedef aclError (*aclrtCreateStreamFunc)(aclrtStream* stream);
typedef aclError (*aclrtDestroyStreamFunc)(aclrtStream stream);
typedef aclError (*aclrtMallocFunc)(void** devPtr, size_t size, aclrtMemMallocPolicy policy);
typedef aclError (*aclrtFreeFunc)(void* devPtr);
typedef aclError (*aclrtMemcpyFunc)(void* dst, size_t destMax, const void* src, size_t count, aclrtMemcpyKind kind);
typedef aclError (*aclrtMemsetFunc)(void* devPtr, size_t maxCount, int32_t value, size_t count);
typedef aclError (*aclrtSynchronizeStreamFunc)(aclrtStream stream);
typedef aclError (*aclrtBinaryLoadFromDataFunc)(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle);
typedef aclError (*aclrtBinaryGetFunctionFunc)(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle);
typedef aclError (*aclrtBinaryGetFunctionByEntryFunc)(
    aclrtBinHandle binHandle, uint64_t funcEntry, aclrtFuncHandle* funcHandle);
typedef aclError (*aclrtLaunchKernelWithHostArgsFunc)(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
    size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, size_t placeHolderNum);
typedef aclError (*aclrtLaunchKernelFunc)(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, const void* argsData, size_t argsSize, aclrtStream stream);
typedef aclError (*aclrtGetFuncBySymbolFunc)(const void* symbol, aclrtFuncHandle* funcHandle);
typedef aclError (*aclrtBinaryUnLoadFunc)(aclrtBinHandle binHandle);

typedef enum {
    ACL_RT_API_aclrtLaunchKernelWithHostArgs = 0,
    ACL_RT_API_aclrtMemcpy = 1,
    ACL_RT_API_aclrtBinaryLoadFromData = 2,
    ACL_RT_API_aclrtBinaryGetFunction = 3,
    ACL_RT_API_aclrtMalloc = 4,
    ACL_RT_API_aclrtMemset = 5,
    ACL_RT_API_aclrtFree = 6,
    ACL_RT_API_aclrtCreateStream = 7,
    ACL_RT_API_aclrtDestroyStream = 8,
    ACL_RT_API_aclrtSetDevice = 9,
    ACL_RT_API_aclrtResetDevice = 10,
    ACL_RT_API_aclrtSynchronizeStream = 11,
    ACL_RT_API_aclrtBinaryGetFunctionByEntry = 12,
    ACL_RT_API_aclrtLaunchKernel = 13,
    ACL_RT_API_aclrtGetFuncBySymbol = 14,
    ACL_RT_API_aclrtBinaryUnLoad = 15,
    ACL_RT_API_MAX
} aclrtApiId;

NPU_COMPUTE_EXPORT int32_t acltoolUploaderInit(MsprofRawDataCallback uploader);
NPU_COMPUTE_EXPORT int32_t acltoolHookInit(void);
NPU_COMPUTE_EXPORT int32_t acltoolClearCallback(aclrtApiId id);
NPU_COMPUTE_EXPORT void* acltoolGetOriginalRuntimeApi(aclrtApiId id);

#define NPU_COMPUTE_DECLARE_REGISTRATION(exportName, apiName) \
    NPU_COMPUTE_EXPORT int32_t acltoolRegister##exportName##Callbacks(apiName##Func callback)

NPU_COMPUTE_DECLARE_REGISTRATION(AclrtSetDevice, aclrtSetDevice);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtResetDevice, aclrtResetDevice);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtCreateStream, aclrtCreateStream);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtDestroyStream, aclrtDestroyStream);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtMalloc, aclrtMalloc);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtFree, aclrtFree);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtMemcpy, aclrtMemcpy);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtMemset, aclrtMemset);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtSynchronizeStream, aclrtSynchronizeStream);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtBinaryLoadFromData, aclrtBinaryLoadFromData);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtBinaryGetFunction, aclrtBinaryGetFunction);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtBinaryGetFunctionByEntry, aclrtBinaryGetFunctionByEntry);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtLaunchKernelWithHostArgs, aclrtLaunchKernelWithHostArgs);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtLaunchKernel, aclrtLaunchKernel);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtGetFuncBySymbol, aclrtGetFuncBySymbol);
NPU_COMPUTE_DECLARE_REGISTRATION(AclrtBinaryUnLoad, aclrtBinaryUnLoad);

#undef NPU_COMPUTE_DECLARE_REGISTRATION

#ifdef __cplusplus
}
#endif

#endif // NPU_COMPUTE_INJECTION_HOOK_H_
