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
 * \file kernel_log.h
 * \brief
 */
#ifndef ASCENDC_MODULE_KERNEL_LOG_INTF_H
#define ASCENDC_MODULE_KERNEL_LOG_INTF_H
#if defined(ASCENDC_CPU_DEBUG) && ASCENDC_CPU_DEBUG == 1
#include <string>
#include <map>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include "stub_def.h"

namespace AscendC {
#ifndef __NPU_DEVICE__

#if defined(UT_TEST) || defined(ST_TEST)
#define ASCENDC_ASSERT(cond, behavior)                          \
    do {                                                        \
        if (!(cond)) {                                          \
            behavior;                                           \
            AscendC::KernelRaise::GetInstance().Raise(SIGABRT); \
        }                                                       \
    } while (0)
#else
#define ASCENDC_ASSERT(cond, behavior) \
    do {                               \
        if (!(cond)) {                 \
            behavior;                  \
            raise(SIGABRT);            \
        }                              \
    } while (0)
#endif

#else // #ifdef __NPU_DEVICE__

#ifndef ASCC_ASCENDC_ASSERT
#define ASCC_ASCENDC_ASSERT
#define ASCENDC_ASSERT(cond, behavior)
#endif

#endif // __NPU_DEVICE__

// Check value in range [low, high]
#define ASCENDC_CHECK_VALUE_RANGE(value, rangeLow, rangeHigh, paramName, apiMsg)                       \
    do {                                                                                               \
        if (value < rangeLow || value > rangeHigh) {                                                   \
            KERNEL_LOG(KERNEL_ERROR, "Failed to check %s value in %s, its valid range is "             \
                "%s ~ %s, current value is %s.", paramName, apiMsg, std::to_string(rangeLow).c_str(),  \
                std::to_string(rangeHigh).c_str(), std::to_string(value).c_str());                     \
            raise(SIGABRT);                                                                            \
        }                                                                                              \
    } while (0)

enum class TPosition : uint8_t;

template <TPosition pos>
__aicore__ inline uint64_t TransUBAddr(uint64_t addr);

// Check tensor tposition with condition judgement
#define ASCENDC_CHECK_TPOSITION(cond, tensorName, tPosName, apiMsg, curPos)                            \
    do {                                                                                               \
        if (!(cond)) {                                                                                 \
            KERNEL_LOG(KERNEL_ERROR, "Failed to check %s tensor position in %s, supported positions "  \
                "are %s, current position is %s.", tensorName, apiMsg, tPosName, curPos.c_str());      \
            raise(SIGABRT);                                                                            \
        }                                                                                              \
    } while (0)

enum KernelFuncType : uint8_t {
    NONE_MODE,
    MASK_COUNT_MODE,    // mask
    MASK_BIT_MODE,      // mask[]
    CALCOUNT_MODE       // calcount
};

enum class LogLevel : uint8_t {
    KERNEL_DEBUG = 0,
    KERNEL_INFO = 1,
    KERNEL_WARN = 2,
    KERNEL_ERROR = 3,
};
}  // namespace AscendC

#define KERNEL_LOG(level, format, ...) KERNEL_LOG_##level(format, ##__VA_ARGS__)

#if __NPU_ARCH__ == 2201

namespace AscendC {
inline std::string GenCoreTypeStr()
{
    std::string coreTypeStr = "";
    if (g_coreType == AscendC::AIC_TYPE) {
        coreTypeStr = "AIC_";
    } else if (g_coreType == AscendC::AIV_TYPE) {
        coreTypeStr = "AIV_";
    } else {
        coreTypeStr = "MIX_";
    }
    coreTypeStr += std::to_string(sub_block_idx);
    return coreTypeStr;
}

inline std::string GenBlockStr()
{
    std::string blockStr = "Block_";
    blockStr += std::to_string(block_idx);
    return blockStr;
}
} // namespace AscendC

#define KERNEL_LOG_KERNEL_DEBUG(format, ...)                                                                   \
    do {                                                                                                       \
        std::string coreTypeStr = AscendC::GenCoreTypeStr();                                                   \
        std::string blockStr = AscendC::GenBlockStr();                                                         \
        printf("[DEBUG][%s][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), coreTypeStr.c_str(), __FILE__, \
            __LINE__, __FUNCTION__, static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                           \
    } while (0)

#define KERNEL_LOG_KERNEL_INFO(format, ...)                                                                   \
    do {                                                                                                      \
        std::string coreTypeStr = AscendC::GenCoreTypeStr();                                                  \
        std::string blockStr = AscendC::GenBlockStr();                                                        \
        printf("[INFO][%s][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), coreTypeStr.c_str(), __FILE__, \
            __LINE__, __FUNCTION__, static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                          \
    } while (0)

#define KERNEL_LOG_KERNEL_WARN(format, ...)                                                                   \
    do {                                                                                                      \
        std::string coreTypeStr = AscendC::GenCoreTypeStr();                                                  \
        std::string blockStr = AscendC::GenBlockStr();                                                        \
        printf("[WARN][%s][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), coreTypeStr.c_str(), __FILE__, \
            __LINE__, __FUNCTION__, static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                          \
    } while (0)

#define KERNEL_LOG_KERNEL_ERROR(format, ...)                                                                   \
    do {                                                                                                       \
        std::string coreTypeStr = AscendC::GenCoreTypeStr();                                                   \
        std::string blockStr = AscendC::GenBlockStr();                                                         \
        printf("[ERROR][%s][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), coreTypeStr.c_str(), __FILE__, \
            __LINE__, __FUNCTION__, static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                           \
    } while (0)

#else

#define KERNEL_LOG_KERNEL_DEBUG(format, ...)                                                                  \
    do {                                                                                                      \
        std::string blockStr = "Core_";                                                                       \
        blockStr += std::to_string(block_idx);                                                                \
        printf("[DEBUG][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), __FILE__, __LINE__, __FUNCTION__, \
            static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                                                  \
    } while (0)

#define KERNEL_LOG_KERNEL_INFO(format, ...)                                                                  \
    do {                                                                                                     \
        std::string blockStr = "Core_";                                                                      \
        blockStr += std::to_string(block_idx);                                                               \
        printf("[INFO][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), __FILE__, __LINE__, __FUNCTION__, \
            static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                                                 \
    } while (0)

#define KERNEL_LOG_KERNEL_WARN(format, ...)                                                                  \
    do {                                                                                                     \
        std::string blockStr = "Core_";                                                                      \
        blockStr += std::to_string(block_idx);                                                               \
        printf("[WARN][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), __FILE__, __LINE__, __FUNCTION__, \
            static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                                                 \
    } while (0)

#define KERNEL_LOG_KERNEL_ERROR(format, ...)                                                                  \
    do {                                                                                                      \
        std::string blockStr = "Core_";                                                                       \
        blockStr += std::to_string(block_idx);                                                                \
        printf("[ERROR][%s][%s:%d][%s][%u] " format "\n", blockStr.c_str(), __FILE__, __LINE__, __FUNCTION__, \
            static_cast<uint32_t>(getpid()), ##__VA_ARGS__);                                                  \
    } while (0)

#endif

#else

#define KERNEL_LOG(level, format, ...)
#ifndef __NPU_HOST__

#define ASCENDC_ASSERT(cond, behavior)

#else // #ifdef __NPU_HOST__

#ifndef ASCC_ASCENDC_ASSERT
#define ASCC_ASCENDC_ASSERT
#define ASCENDC_ASSERT(cond, behavior) \
    do {                               \
        if (!(cond)) {                 \
            behavior;                  \
            raise(SIGABRT);            \
        }                              \
    } while (0)
#endif

#endif // __NPU_HOST__
#define ASCENDC_CHECK_VALUE_RANGE(value, rangeLow, rangeHigh, paramName, apiMsg)
#define ASCENDC_CHECK_TPOSITION(cond, tensorName, tPosName, apiMsg, curPos)

#endif

#endif // ASCENDC_MODULE_KERNEL_LOG_INTF_H