/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_ACL_SAN_LOG_H
#define ASCSAN_ACL_SAN_LOG_H

#include "plog_sink.h"

// Format adapters only: levels and output are owned by the common CANN plog sink.
#define ASC_SAN_DEBUG(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::kDebug, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ASC_SAN_INFO(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::kInfo, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ASC_SAN_WARNING(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::kWarning, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ASC_SAN_ERROR(...) \
    ::aclsan::WritePlogFormat(::aclsan::PlogLevel::kError, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define ACLSAN_CHECK_NULLPTR(apiName, argument)                       \
    do {                                                              \
        if ((argument) == nullptr) {                                  \
            ASC_SAN_ERROR("%s: %s is nullptr", (apiName), #argument); \
            return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;             \
        }                                                             \
    } while (false)

#endif
