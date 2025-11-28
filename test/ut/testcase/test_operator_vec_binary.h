/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef ASCENDC_TEST_OPERATOR_VEC_BINARY_H
#define ASCENDC_TEST_OPERATOR_VEC_BINARY_H

#include <gtest/gtest.h>
#include "kernel_operator.h"

namespace AscendC {
template <typename T>
void main_vec_binary_operator_simple_demo(__gm__ uint8_t* __restrict__ dst_gm, __gm__ uint8_t* __restrict__ src0_gm,
    __gm__ uint8_t* __restrict__ src1_gm, __gm__ int32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
    output_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(dst_gm), data_size);

    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, data_size * sizeof(T));
    LocalTensor<T> input0_local = tbuf.Get<T>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, data_size * sizeof(T));
    LocalTensor<T> input1_local = tbuf1.Get<T>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, data_size * sizeof(T));
    LocalTensor<T> output_local = tbuf2.Get<T>();

    for (int32_t i = 0; i < data_size; ++i) {
        input1_local.SetValue(i, 1);
    }

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    Add(output_local, input0_local, input1_local, data_size);
    Sub(output_local, output_local, input1_local, data_size);
    Mul(output_local, output_local, input1_local, data_size);
    Max(output_local, output_local, input1_local, data_size);
    Min(output_local, output_local, input1_local, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    DataCopy(output_global, output_local, data_size);
    SetAtomicNone();
    pipe_barrier(PIPE_ALL);
}

template <typename T>
void main_vec_binary_operator_div_demo(__gm__ uint8_t* __restrict__ dst_gm, __gm__ uint8_t* __restrict__ src0_gm,
    __gm__ uint8_t* __restrict__ src1_gm, __gm__ int32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
    output_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(dst_gm), data_size);

    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, data_size * sizeof(T));
    LocalTensor<T> input0_local = tbuf.Get<T>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, data_size * sizeof(T));
    LocalTensor<T> input1_local = tbuf1.Get<T>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, data_size * sizeof(T));
    LocalTensor<T> output_local = tbuf2.Get<T>();

    for (int32_t i = 0; i < data_size; ++i) {
        input1_local.SetValue(i, 1);
    }

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    Div(output_local, input0_local, input1_local, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);
    pipe_barrier(PIPE_ALL);
}

template <typename T>
void main_vec_binary_operator_and_or_demo(__gm__ uint8_t* __restrict__ dst_gm, __gm__ uint8_t* __restrict__ src0_gm,
    __gm__ uint8_t* __restrict__ src1_gm, __gm__ int32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
    output_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(dst_gm), data_size);

    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, data_size * sizeof(T));
    LocalTensor<T> input0_local = tbuf.Get<T>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, data_size * sizeof(T));
    LocalTensor<T> input1_local = tbuf1.Get<T>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, data_size * sizeof(T));
    LocalTensor<T> output_local = tbuf2.Get<T>();

    for (int32_t i = 0; i < data_size; ++i) {
        input1_local.SetValue(i, 1);
    }

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    And(output_local, input0_local, input1_local, data_size);
    Or(output_local, output_local, input1_local, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);
    pipe_barrier(PIPE_ALL);
}

template <typename T>
void main_vec_binary_operator_addrelu_demo(__gm__ uint8_t* __restrict__ dst_gm, __gm__ uint8_t* __restrict__ src0_gm,
    __gm__ uint8_t* __restrict__ src1_gm, __gm__ int32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
    output_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(dst_gm), data_size);

    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, data_size * sizeof(T));
    LocalTensor<T> input0_local = tbuf.Get<T>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, data_size * sizeof(T));
    LocalTensor<T> input1_local = tbuf1.Get<T>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, data_size * sizeof(T));
    LocalTensor<T> output_local = tbuf2.Get<T>();

    for (int32_t i = 0; i < data_size; ++i) {
        input1_local.SetValue(i, 1);
    }

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AddRelu(output_local, output_local, input1_local, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    DataCopy(output_global, output_local, data_size);
    SetAtomicNone();
    pipe_barrier(PIPE_ALL);
}

template <typename T>
void main_vec_binary_operator_subrelu_demo(__gm__ uint8_t* __restrict__ dst_gm, __gm__ uint8_t* __restrict__ src0_gm,
    __gm__ uint8_t* __restrict__ src1_gm, __gm__ int32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
    output_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(dst_gm), data_size);

    TBuf<TPosition::VECCALC> tbuf;
    tpipe.InitBuffer(tbuf, data_size * sizeof(T));
    LocalTensor<T> input0_local = tbuf.Get<T>();

    TBuf<TPosition::VECCALC> tbuf1;
    tpipe.InitBuffer(tbuf1, data_size * sizeof(T));
    LocalTensor<T> input1_local = tbuf1.Get<T>();

    TBuf<TPosition::VECCALC> tbuf2;
    tpipe.InitBuffer(tbuf2, data_size * sizeof(T));
    LocalTensor<T> output_local = tbuf2.Get<T>();

    for (int32_t i = 0; i < data_size; ++i) {
        input1_local.SetValue(i, 1);
    }

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    SubRelu(output_local, output_local, input1_local, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    DataCopy(output_global, output_local, data_size);
    SetAtomicNone();
    pipe_barrier(PIPE_ALL);
}

struct BinaryTestParams {
    int32_t data_size;
    int32_t data_bit_size;
    void (*cal_func)(uint8_t*, uint8_t*, uint8_t*, int32_t);
};

class BinarySimpleTestsuite : public testing::Test, public testing::WithParamInterface<BinaryTestParams> {
protected:
    void SetUp() {}
    void TearDown() {}
};
} // namespace AscendC
#endif // ASCENDC_TEST_OPERATOR_VEC_BINARY_H
