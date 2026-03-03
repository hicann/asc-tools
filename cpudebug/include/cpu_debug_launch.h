/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#ifndef ASCENDC_CPU_KERNEL_LAUNCH
#define ASCENDC_CPU_KERNEL_LAUNCH

#include "acl/acl.h"
#include "stub_def.h"
#include "tikicpulib.h"
#include "kern_fwk.h"
#include "kernel_elf_parser.h"

namespace AscendC{
extern "C" __attribute__ ((visibility("hidden"))) __attribute__((weak)) uint32_t __asc_LaunchAndProfiling(const char *kernelName,
    uint32_t blockDim, void *stream, void **args, uint32_t size, const uint32_t ubufDynamicSize);


template <typename T, typename... Args>
inline void AscCPUKernelLaunch(unsigned numBlocks, void* dynicsize, aclrtStream stream, const char *mangling, T kernelFunc, Args... args)
{
    __asc_LaunchAndProfiling(mangling, numBlocks, stream, nullptr, 0, 0);
    AscendC::SetKernelMode(KernelModeRegister::GetInstance().GetKenelMode(mangling));
    AscendC::RunKernelFunctionOnCpu(kernelFunc, mangling, numBlocks, args...);
}
}

#endif // ASCENDC_CPU_KERNEL_LAUNCH
