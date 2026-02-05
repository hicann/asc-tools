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
 * \file simt_stub.h
 * \brief
 */
#ifndef ASCENDC_SIMT_STUB_H
#define ASCENDC_SIMT_STUB_H

#if defined(ASCENDC_CPU_DEBUG)
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 3101) || (__NPU_ARCH__ == 5102))
#include "stub_fun.h"
#include "simt_api/dav_c310/kernel_simt_cpu.h"

#define R CAST_RINT
#define A CAST_ROUND
#define F CAST_FLOOR
#define C CAST_CEIL
#define Z CAST_TRUNC
#define O CAST_ODD

#define __launch_bounds__(x)

template <ROUND rnd = ROUND::R, RoundingSaturation sat = RoundingSaturation::RS_DISABLE_VALUE, typename SRC_TYPE>
float __cvt_float(SRC_TYPE src) {
    if (std::is_same<SRC_TYPE, half>::value) {
        return static_cast<float>(src);
    }
    if (std::is_same<SRC_TYPE, bfloat16_t>::value) {
        return static_cast<float>(src);
    }
}

template <ROUND rnd = ROUND::R, RoundingSaturation sat = RoundingSaturation::RS_DISABLE_VALUE, typename SRC_TYPE>
half __cvt_half(SRC_TYPE src) {
    if (std::is_same<SRC_TYPE, float>::value) {
        return half(src);
    }
}

template <ROUND rnd = ROUND::R, RoundingSaturation sat = RoundingSaturation::RS_DISABLE_VALUE, typename SRC_TYPE>
bfloat16_t __cvt_bfloat16_t(SRC_TYPE src) {
    if (std::is_same<SRC_TYPE, float>::value) {
        return bfloat16_t(src);
    }
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
int32_t __cvt_int32_t(SRC_TYPE x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
uint32_t __cvt_uint32_t(SRC_TYPE x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
int64_t __cvt_int64_t(SRC_TYPE x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
uint64_t __cvt_uint64_t(SRC_TYPE x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
float2 __cvt_float2(SRC_TYPE x) {
    float2 tmp;
    return tmp;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
bfloat16x2_t __cvt_bfloat16x2_t(SRC_TYPE x) {
    bfloat16x2_t tmp;
    return tmp;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
half2 __cvt_half2(SRC_TYPE x) {
    half2 tmp;
    return tmp;
}

namespace bisheng {
namespace cce {
namespace simt {

template<ROUND rnd, RoundingSaturation rst>
int32_t __cvt_int32_t(float x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
int32_t __cvt_int32_t(half x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
int32_t __cvt_int32_t(bfloat16_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
uint32_t __cvt_uint32_t(float x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
uint32_t __cvt_uint32_t(half x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
uint32_t __cvt_uint32_t(bfloat16_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
int64_t __cvt_int64_t(float x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
uint64_t __cvt_uint64_t(float x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
half __cvt_half(int32_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
half __cvt_half(uint32_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(int32_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(uint32_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(int64_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(uint64_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
bfloat16_t __cvt_bfloat16_t(int32_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
bfloat16_t __cvt_bfloat16_t(uint32_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
half __cvt_half(half x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
half __cvt_half(float x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
half __cvt_half(bfloat16_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
bfloat16_t __cvt_bfloat16_t(bfloat16_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
bfloat16_t __cvt_bfloat16_t(half x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
bfloat16_t __cvt_bfloat16_t(float x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(bfloat16_t x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(half x) {
    return 0;
}

template<ROUND rnd, RoundingSaturation rst>
float __cvt_float(float x) {
    return 0;
}

}
}
}

#ifndef SIMT_CCE
#define SIMT_CCE
namespace cce {
struct dim3 {
    uint32_t x = 1u, y = 1u, z = 1u;
    dim3(uint32_t x_) { x = x_; }
    dim3(uint32_t x_, uint32_t y_)
    {
        x = x_;
        y = y_;
    }
    dim3(uint32_t x_, uint32_t y_, uint32_t z_)
    {
        x = x_;
        y = y_;
        z = z_;
    }
};

template <auto funcPtr, typename... Args>
void async_invoke(const dim3 &dim, Args &&...args)
{
    g_threadDimX = dim.x;
    g_threadDimY = dim.y;
    g_threadDimZ = dim.z;
    AscendC::Simt::ThreadBlock &threadBlock = AscendC::Simt::ThreadBlock::GetBlockInstance();
    const uint32_t threadNum = g_threadDimX * g_threadDimY * g_threadDimZ;
    threadBlock.Init(threadNum);
    auto func = [&args...]() { funcPtr(args...); };
    for (uint32_t i = 0; i < threadNum; i++) {
        threadBlock.Schedule(func, i);
    }
    threadBlock.FinishJobs();
}
}  // namespace cce

#endif

enum L1CacheType : uint32_t { NON_CACHEABLE = 0, CACHEABLE = 1 };
enum class LD_L2CacheType : uint32_t { L2_CACHE_HINT_NORMAL_FV = 0 };
enum class ST_L2CacheType : uint32_t { L2_CACHE_HINT_NORMAL_FV = 0 };

template <LD_L2CacheType L2Cache = LD_L2CacheType::L2_CACHE_HINT_NORMAL_FV,
          L1CacheType L1CacheType = L1CacheType::NON_CACHEABLE, typename T>
T __ldg(__gm__ T* address)
{
    return *address;
}
template <ST_L2CacheType L2Cache = ST_L2CacheType::L2_CACHE_HINT_NORMAL_FV,
          L1CacheType L1CacheType = L1CacheType::NON_CACHEABLE, typename T>
void __stg(__gm__ T* address, T val)
{
    *address = val;
}

struct BlockDim {
    uint32_t &x;
    uint32_t &y;
    uint32_t &z;
    BlockDim(uint32_t &x_, uint32_t &y_, uint32_t &z_)
        : x(x_), y(y_), z(z_)
    {}
};

struct BlockIdx {
    int64_t &x;
    int64_t y;
    int64_t z;
    BlockIdx(int64_t &x_)
        : x(x_)
    {}
};

struct ThreadIdx {
    uint32_t &x;
    uint32_t &y;
    uint32_t &z;
    ThreadIdx(uint32_t &x_, uint32_t &y_, uint32_t &z_)
        : x(x_), y(y_), z(z_)
    {}
};

struct GridDim {
    int64_t &x;
    int64_t y;
    int64_t z;
    GridDim(int64_t &x_)
        : x(x_)
    {
        y = 1;
        z = 1;
    }
};

inline BlockDim blockDim(g_threadDimX, g_threadDimY, g_threadDimZ);
inline BlockIdx blockIdx(block_idx);
inline thread_local ThreadIdx threadIdx(g_threadIdxX, g_threadIdxY, g_threadIdxZ);
inline GridDim gridDim(block_num);

#ifndef SIMT_WARP_SIZE
#define SIMT_WARP_SIZE
constexpr int32_t warpSize = 32;
#endif

#endif
#endif

#endif