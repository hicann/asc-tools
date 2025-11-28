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
 * \file kernel_vectorized.h
 * \brief
 */
#ifndef ASCENDC_VECTORIZED_H
#define ASCENDC_VECTORIZED_H
#include "kernel_fp16.h"
#include "kernel_bf16.h"

namespace Vectorized {

template <typename T>
struct VectorizedType1 {
    T x;
};

template <typename T>
struct VectorizedType2 : VectorizedType1<T> {
    T y;
};

template <typename T>
struct VectorizedType3 : VectorizedType2<T> {
    T z;
};

template <typename T>
struct VectorizedType4 : VectorizedType3<T> {
    T w;
};
}
using uint4 = Vectorized::VectorizedType4<uint32_t>;
using uint3 = Vectorized::VectorizedType3<uint32_t>;
using uint2 = Vectorized::VectorizedType2<uint32_t>;
using uint1 = Vectorized::VectorizedType1<uint32_t>;

using int4 = Vectorized::VectorizedType4<int32_t>;
using int3 = Vectorized::VectorizedType3<int32_t>;
using int2 = Vectorized::VectorizedType2<int32_t>;
using int1 = Vectorized::VectorizedType1<int32_t>;

using float4 = Vectorized::VectorizedType4<float>;
using float3 = Vectorized::VectorizedType3<float>;
using float2 = Vectorized::VectorizedType2<float>;
using float1 = Vectorized::VectorizedType1<float>;

using long4 = Vectorized::VectorizedType4<int64_t>;
using long3 = Vectorized::VectorizedType3<int64_t>;
using long2 = Vectorized::VectorizedType2<int64_t>;
using long1 = Vectorized::VectorizedType1<int64_t>;

using ulong4 = Vectorized::VectorizedType4<uint64_t>;
using ulong3 = Vectorized::VectorizedType3<uint64_t>;
using ulong2 = Vectorized::VectorizedType2<uint64_t>;
using ulong1 = Vectorized::VectorizedType1<uint64_t>;

using half2 = Vectorized::VectorizedType2<half>;
using half1 = Vectorized::VectorizedType1<half>;

using bhalf2 = Vectorized::VectorizedType2<bfloat16_t>;
#endif  // ASCENDC_VECTORIZED_H