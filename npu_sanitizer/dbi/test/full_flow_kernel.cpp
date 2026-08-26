// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "kernel_operator.h"

extern "C" __global__ __aicore__ void FullFlowKernel(GM_ADDR input, GM_ADDR output, GM_ADDR workspace)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AICORE);
    AscendC::GlobalTensor<uint32_t> inputGm;
    AscendC::GlobalTensor<uint32_t> outputGm;
    inputGm.SetGlobalBuffer((__gm__ uint32_t*)input, 8);
    outputGm.SetGlobalBuffer((__gm__ uint32_t*)output, 8);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buffer;
    pipe.InitBuffer(buffer, 8 * sizeof(uint32_t));
    auto local = buffer.Get<uint32_t>();
    AscendC::DataCopy(local, inputGm, 8);
    AscendC::DataCopy(outputGm, local, 8);
    (void)workspace;
}
