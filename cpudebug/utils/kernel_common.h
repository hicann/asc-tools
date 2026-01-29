/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/*!
 * \file kernel_common.h
 * \brief
 */
#ifndef ASCENDC_KERNEL_COMMON_H
#define ASCENDC_KERNEL_COMMON_H

#include "kernel_utils.h"
#include "kernel_process_lock.h"
#include "kernel_struct_mm.h"
#include "kernel_process_lock.h"
#include "kernel_event.h"

#ifndef WORKSPACE_PARAM_OFFSET
#define WORKSPACE_PARAM_OFFSET 0xffffffff
#endif

__BLOCK_LOCAL__ __inline__ __gm__ uint8_t* g_sysWorkspaceReserved;

#if defined(ASCENDC_CPU_DEBUG)
__aicore__ __gm__ uint8_t* __gm__ GetSysWorkSpacePtr();
#else
__aicore__ inline __gm__ uint8_t* __gm__ GetSysWorkSpacePtr()
{
#if (WORKSPACE_PARAM_OFFSET != 0xffffffff)
    return ((GM_ADDR *)get_para_base())[WORKSPACE_PARAM_OFFSET];
#else
    return g_sysWorkspaceReserved;
#endif
}
#endif

namespace AscendC {

__aicore__ inline int64_t GetBlockNum();

__aicore__ inline int64_t GetBlockIdx();

#if defined(__NPU_ARCH__) &&                                                                                    \
    ((__NPU_ARCH__ == 5102) || (__NPU_ARCH__ == 2103) || (__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) ||    \
     (__NPU_ARCH__ == 3113) || (__NPU_ARCH__ == 3101))

__aicore__ inline constexpr uint32_t GetVecLen()
{
    return VECTOR_REG_WIDTH;
}
#endif
} // namespace AscendC

#endif
