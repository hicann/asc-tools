/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#include <gtest/gtest.h>
#include "kernel_operator.h"

#ifndef ASCENDC_TEST_SYMBOL_OVERRIDE_H
#define ASCENDC_TEST_SYMBOL_OVERRIDE_H

namespace AscendC {
template <typename T>
void test_symbol_override_add_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    output_local = input0_local + input1_local;
    output_local = output_local * input1_local;
    output_local = output_local - input1_local;

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);
    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_div_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    output_local = input0_local / input1_local;

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);
    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_compare_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    int32_t sel_size = data_size / AscendCUtils::GetBitSize(sizeof(uint8_t));

    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    TBuf<TPosition::VECCALC> tbuf3;
    tpipe.InitBuffer(tbuf3, AlignUp(sel_size * sizeof(uint8_t), ONE_BLK_SIZE));
    LocalTensor<uint8_t> sel_mask = tbuf3.Get<uint8_t>();

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    sel_mask = input0_local < input1_local;

    pipe_barrier(PIPE_V);

    SELMODE sel_mode = SELMODE::VSEL_CMPMASK_SPR;
    Select(output_local, sel_mask, input0_local, input1_local, sel_mode, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);

    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_compare_gt_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    int32_t sel_size = data_size / AscendCUtils::GetBitSize(sizeof(uint8_t));

    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    TBuf<TPosition::VECCALC> tbuf3;
    tpipe.InitBuffer(tbuf3, AlignUp(sel_size * sizeof(uint8_t), ONE_BLK_SIZE));
    LocalTensor<uint8_t> sel_mask = tbuf3.Get<uint8_t>();

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    sel_mask = input0_local > input1_local;

    pipe_barrier(PIPE_V);

    SELMODE sel_mode = SELMODE::VSEL_CMPMASK_SPR;
    Select(output_local, sel_mask, input0_local, input1_local, sel_mode, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);

    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_compare_ge_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    int32_t sel_size = data_size / AscendCUtils::GetBitSize(sizeof(uint8_t));

    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    TBuf<TPosition::VECCALC> tbuf3;
    tpipe.InitBuffer(tbuf3, AlignUp(sel_size * sizeof(uint8_t), ONE_BLK_SIZE));
    LocalTensor<uint8_t> sel_mask = tbuf3.Get<uint8_t>();

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    sel_mask = input0_local >= input1_local;

    pipe_barrier(PIPE_V);

    SELMODE sel_mode = SELMODE::VSEL_CMPMASK_SPR;
    Select(output_local, sel_mask, input0_local, input1_local, sel_mode, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);

    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_compare_le_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    int32_t sel_size = data_size / AscendCUtils::GetBitSize(sizeof(uint8_t));

    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    TBuf<TPosition::VECCALC> tbuf3;
    tpipe.InitBuffer(tbuf3, AlignUp(sel_size * sizeof(uint8_t), ONE_BLK_SIZE));
    LocalTensor<uint8_t> sel_mask = tbuf3.Get<uint8_t>();

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    sel_mask = input0_local <= input1_local;

    pipe_barrier(PIPE_V);

    SELMODE sel_mode = SELMODE::VSEL_CMPMASK_SPR;
    Select(output_local, sel_mask, input0_local, input1_local, sel_mode, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);

    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_compare_eq_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    int32_t sel_size = data_size / AscendCUtils::GetBitSize(sizeof(uint8_t));

    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    TBuf<TPosition::VECCALC> tbuf3;
    tpipe.InitBuffer(tbuf3, AlignUp(sel_size * sizeof(uint8_t), ONE_BLK_SIZE));
    LocalTensor<uint8_t> sel_mask = tbuf3.Get<uint8_t>();

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    sel_mask = input0_local == input1_local;

    pipe_barrier(PIPE_V);

    SELMODE sel_mode = SELMODE::VSEL_CMPMASK_SPR;
    Select(output_local, sel_mask, input0_local, input1_local, sel_mode, data_size);

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);

    pipe_barrier(PIPE_ALL);
}

template <typename T>
void test_symbol_override_or_kernel(__gm__ uint8_t* __restrict__ src0_gm, __gm__ uint8_t* __restrict__ src1_gm,
    __gm__ uint8_t* __restrict__ dst_gm, __gm__ uint32_t data_size)
{
    TPipe tpipe;
    GlobalTensor<T> input0_global;
    GlobalTensor<T> input1_global;
    GlobalTensor<T> output_global;
    input0_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src0_gm), data_size);
    input1_global.SetGlobalBuffer(reinterpret_cast<__gm__ T*>(src1_gm), data_size);
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

    AscendCUtils::SetMask<uint8_t>(256);

    DataCopy(input0_local, input0_global, data_size);
    DataCopy(input1_local, input1_global, data_size);

    set_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);
    AscendCUtils::SetMask<uint8_t>(128);
    wait_flag(PIPE_MTE2, PIPE_V, EVENT_ID0);

    output_local = input0_local | input1_local;
    output_local = output_local & input1_local;

    set_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);
    wait_flag(PIPE_V, PIPE_MTE3, EVENT_ID0);

    DataCopy(output_global, output_local, data_size);
    pipe_barrier(PIPE_ALL);
}

struct SymbolOverrideTestParams {
    int32_t data_size;
    int32_t type_bit_size;
    void (*cal_func)(uint8_t*, uint8_t*, uint8_t*, uint32_t);
};

class SymbolOverrideTestsuite : public testing::Test, public testing::WithParamInterface<SymbolOverrideTestParams> {
protected:
    void SetUp() {}
    void TearDown() {}
};
} // namespace AscendC
#endif // ASCENDC_TEST_SYMBOL_OVERRIDE_H
