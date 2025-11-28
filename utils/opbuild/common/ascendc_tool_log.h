/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#ifndef ASCENDC_TOOL_LOG_H
#define ASCENDC_TOOL_LOG_H

#include <sys/syscall.h>
#include <unistd.h>
#include <inttypes.h>
#include "mmpa/mmpa_api.h"
#include "external/ge_common/ge_api_error_codes.h"
#include "alog_pub.h"

#define ASCENDC_MODULE_NAME static_cast<int32_t>(ASCENDCKERNEL)

#if !(defined(UT_TEST) || defined(ST_TEST))
#define ASCENDLOGE(format, ...)                                                                                        \
    do {                                                                                                               \
        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_ERROR) == 1) {                                               \
            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_ERROR, "  %d [%s:%d][%s]" format "\n", mmGetTid(),         \
                       __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);               \
        }                                                                                                              \
    } while (0)

#define ASCENDLOGW(format, ...)                                                                                        \
    do {                                                                                                               \
        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_WARN) == 1) {                                                \
            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_WARN, "  %d [%s:%d][%s]" format "\n", mmGetTid(),          \
                       __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);               \
        }                                                                                                              \
    } while (0)

#define ASCENDLOGI(format, ...)                                                                                        \
    do {                                                                                                               \
        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_INFO) == 1) {                                                \
            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_INFO, "  %d [%s:%d][%s]" format "\n", mmGetTid(),          \
                       __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);               \
        }                                                                                                              \
    } while (0)

#define ASCENDLOGD(format, ...)                                                                                        \
    do {                                                                                                               \
        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_DEBUG) == 1) {                                               \
            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_DEBUG, " %d [%s:%d][%s]" format "\n", mmGetTid(),          \
                       __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);               \
        }                                                                                                              \
    } while (0)
#else
#define ASCENDLOGE
#define ASCENDLOGW
#define ASCENDLOGI
#define ASCENDLOGD
#endif

#endif // ASCENDC_TOOL_LOG_H