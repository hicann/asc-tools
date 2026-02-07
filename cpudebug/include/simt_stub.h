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
#include "kernel_simt_cpu.h"

#define R CAST_RINT
#define A CAST_ROUND
#define F CAST_FLOOR
#define C CAST_CEIL
#define Z CAST_TRUNC
#define O CAST_ODD

#define __launch_bounds__(x)

template <ROUND rnd = ROUND::R, RoundingSaturation sat = RoundingSaturation::RS_DISABLE_VALUE, typename SRC_TYPE>
float __cvt_float(SRC_TYPE src) {
    static_assert(std::is_same_v<SRC_TYPE, int32_t> ||
                  std::is_same_v<SRC_TYPE, uint32_t> ||
                  std::is_same_v<SRC_TYPE, uint64_t> ||
                  std::is_same_v<SRC_TYPE, int64_t> ||
                  std::is_same_v<SRC_TYPE, half> ||
                  std::is_same_v<SRC_TYPE, float> ||
                  std::is_same_v<SRC_TYPE, bfloat16_t>,
                  "src type can only be int32_t/uint32_t/int64_t/uint64_t and half/float/bfloat_t");
    static_assert((rnd == ROUND::R) || (rnd == ROUND::A) || (rnd == ROUND::F) || (rnd == ROUND::C) || (rnd == ROUND::Z),
                  "rnd type can only be: ROUND_R, ROUND_A, ROUND_F, ROUND_C,ROUND_Z");

    if constexpr (std::is_same_v<SRC_TYPE, half> || std::is_same_v<SRC_TYPE, bfloat16_t>) {
        if (__isnan(src)) {
            return NAN;
        }
        if (__isinf(src)) {
            return copysignf(INFINITY, src);
        }
        return src.ToFloat();
    } else if constexpr (std::is_same_v<SRC_TYPE, float>) {
        if constexpr (rnd == ROUND::R) {
            return rintf(src);
        } else if constexpr (rnd == ROUND::A) {
            return roundf(src);
        } else if constexpr (rnd == ROUND::F) {
            return floorf(src);
        } else if constexpr (rnd == ROUND::C) {
            return ceilf(src);
        } else if constexpr (rnd == ROUND::Z) {
            return truncf(src);
        } else {
            return src;
        }
    }
    return 0;
}

template <ROUND rnd = ROUND::R, RoundingSaturation sat = RoundingSaturation::RS_DISABLE_VALUE, typename SRC_TYPE>
half __cvt_half(SRC_TYPE src) {
    if (std::is_same<SRC_TYPE, float>::value) {
        return half(src);
    }
    return half(0);
}

template <ROUND rnd = ROUND::R, RoundingSaturation sat = RoundingSaturation::RS_DISABLE_VALUE, typename SRC_TYPE>
bfloat16_t __cvt_bfloat16_t(SRC_TYPE src) {
    if (std::is_same<SRC_TYPE, float>::value) {
        return bfloat16_t(src);
    }
    return bfloat16_t(0);
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
int32_t __cvt_int32_t(SRC_TYPE x) {
    static_assert(
        std::is_same_v<SRC_TYPE,half> || std::is_same_v<SRC_TYPE, float> || std::is_same_v<SRC_TYPE,bfloat16_t>,
        "src type can only be half/float/bfloat_t");

    static_assert((rnd == ROUND::R) || (rnd == ROUND::A) || (rnd == ROUND::F) || (rnd == ROUND::C) || (rnd == ROUND::Z),
                  "rnd type can only be: ROUND_R, ROUND_A, ROUND_F, ROUND_C,ROUND_Z");

    static_assert(rst == RoundingSaturation::RS_ENABLE_VALUE, "sat type can only be: RS_ENABLE");
    if (__isnan(x)) {
        return 0;
    }
    if (__isinf(x)) {
        if (x > SRC_TYPE{0}) {
            return INT32_MAX;
        } else {
            return INT32_MIN;
        }
    }
    if constexpr (std::is_same_v<SRC_TYPE, float>) {
        return static_cast<int32_t>(__cvt_float<rnd>(x));
    } else if constexpr (std::is_same_v<SRC_TYPE, half> || std::is_same_v<SRC_TYPE, bfloat16_t>) {
        float f = __cvt_float<rnd>(x);
        return static_cast<int32_t>(__cvt_float<rnd>(f));
    }
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
uint32_t __cvt_uint32_t(SRC_TYPE x) {
    static_assert(
        std::is_same_v<SRC_TYPE,half> || std::is_same_v<SRC_TYPE, float> || std::is_same_v<SRC_TYPE,bfloat16_t>,
        "src type can only be half/float/bfloat_t");

    static_assert((rnd == ROUND::R) || (rnd == ROUND::A) || (rnd == ROUND::F) || (rnd == ROUND::C) || (rnd == ROUND::Z),
                  "rnd type can only be: ROUND_R, ROUND_A, ROUND_F, ROUND_C,ROUND_Z");

    static_assert(rst == RoundingSaturation::RS_ENABLE_VALUE, "sat type can only be: RS_ENABLE");
    if (__isnan(x) || x < SRC_TYPE{0}) {
        return 0;
    }
    if (__isinf(x) && x > SRC_TYPE{0}) {
        return UINT32_MAX;
    }
    if constexpr (std::is_same_v<SRC_TYPE, float>) {
        return static_cast<uint32_t>(__cvt_float<rnd>(x));
    } else if constexpr (std::is_same_v<SRC_TYPE, half> || std::is_same_v<SRC_TYPE, bfloat16_t>) {
        float f = __cvt_float<rnd>(x);
        return static_cast<uint32_t>(__cvt_float<rnd>(f));
    }
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
int64_t __cvt_int64_t(SRC_TYPE x) {
    static_assert(std::is_same_v<SRC_TYPE, float>, "src type can only be float");
    static_assert((rnd == ROUND::R) || (rnd == ROUND::A) || (rnd == ROUND::F) || (rnd == ROUND::C) || (rnd == ROUND::Z),
                  "rnd type can only be: ROUND_R, ROUND_A, ROUND_F, ROUND_C,ROUND_Z");
    static_assert(rst == RoundingSaturation::RS_ENABLE_VALUE, "sat type can only be: RS_ENABLE");
    if (__isnan(x)) {
        return 0;
    }
    if (__isinf(x)) {
        if (x > SRC_TYPE{0}) {
            return INT64_MAX;
        } else {
            return INT64_MIN;
        }
    }
    if constexpr (std::is_same_v<SRC_TYPE, float>) {
        return static_cast<int64_t>(__cvt_float<rnd>(x));
    }
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
uint64_t __cvt_uint64_t(SRC_TYPE x) {
    static_assert(std::is_same_v<SRC_TYPE, float>, "src type can only be float");
    static_assert((rnd == ROUND::R) || (rnd == ROUND::A) || (rnd == ROUND::F) || (rnd == ROUND::C) || (rnd == ROUND::Z),
                  "rnd type can only be: ROUND_R, ROUND_A, ROUND_F, ROUND_C,ROUND_Z");
    static_assert(rst == RoundingSaturation::RS_ENABLE_VALUE, "sat type can only be: RS_ENABLE");
    if constexpr (std::is_same_v<SRC_TYPE, float>) {
        if (__isnan(x) || x < SRC_TYPE{0}) {
            return 0;
        }
        if (__isinf(x) && x > SRC_TYPE{0}) {
            return UINT64_MAX;
        }
        return static_cast<uint64_t>(__cvt_float<rnd>(x));
    }
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
 float2 __cvt_float2(SRC_TYPE a) {
     float2 tmp;
     tmp.x = __cvt_float<rnd, rst>(a.x);
     tmp.y = __cvt_float<rnd, rst>(a.y);
     return tmp;
 }

 template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
 bfloat16x2_t __cvt_bfloat16x2_t(SRC_TYPE a) {
     bfloat16x2_t tmp;
     tmp.x = __cvt_bfloat16_t<rnd, rst>(a.x);
     tmp.y = __cvt_bfloat16_t<rnd, rst>(a.y);
     return tmp;
 }

 template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
 half2 __cvt_half2(SRC_TYPE a) {

     half2 tmp;
     tmp.x = __cvt_half<rnd, rst>(a.x);
     tmp.y = __cvt_half<rnd, rst>(a.y);
     return tmp;
 }

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
hifloat8x2_t __cvt_hifloat8x2_t(SRC_TYPE x) {
    hifloat8x2_t tmp;
    return tmp;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
float8_e4m3x2_t __cvt_float8_e4m3x2_t(SRC_TYPE x) {
    float8_e4m3x2_t tmp;
    return tmp;
}

template<ROUND rnd, RoundingSaturation rst, typename SRC_TYPE>
float8_e5m2x2_t __cvt_float8_e5m2x2_t(SRC_TYPE x) {
    float8_e5m2x2_t tmp;
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