/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACLPTI_EXPORT_H_
#define ACLPTI_EXPORT_H_

#ifndef ACLPTI_EXPORT
#if defined(_WIN32)
#define ACLPTI_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define ACLPTI_EXPORT __attribute__((visibility("default")))
#else
#define ACLPTI_EXPORT
#endif
#endif

#endif // ACLPTI_EXPORT_H_
