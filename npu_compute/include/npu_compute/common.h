/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_INCLUDE_NPU_COMPUTE_COMMON_H_
#define NPU_COMPUTE_INCLUDE_NPU_COMPUTE_COMMON_H_

#ifndef NPU_COMPUTE_EXPORT
#if defined(_WIN32)
#define NPU_COMPUTE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define NPU_COMPUTE_EXPORT __attribute__((visibility("default")))
#define NPU_COMPUTE_LOCAL __attribute__((visibility("hidden")))
#else
#define NPU_COMPUTE_EXPORT
#define NPU_COMPUTE_LOCAL
#endif
#endif

#endif // NPU_COMPUTE_INCLUDE_NPU_COMPUTE_COMMON_H_
