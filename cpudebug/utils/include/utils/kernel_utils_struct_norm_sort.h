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
 * \file kernel_utils_struct_norm_sort.h
 * \brief
 */
#ifndef ASCENDC_MODULE_UTILS_STRUCT_NORM_SORT_H
#define ASCENDC_MODULE_UTILS_STRUCT_NORM_SORT_H

namespace AscendC {
template <typename T> class LocalTensor;
template <typename T> class GlobalTensor;

#if defined(__NPU_ARCH__) && ((__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3113))

// redefine these to support other platform.
enum class pre_quant_t {
    No_Quant = 0,
    REQS8_Vector = 2,
    REQS8_Scalar,
    DEQF16_Vector,
    DEQF16_Scalar,
    DEQS16_Vector,
    DEQS16_Scalar,
    DEQS32_Vector,
    DEQS32_Scalar,
    REQS4_Vector,
    REQS4_Scalar,
};

typedef ReluMode_t pre_relu_t;

#if !defined(__CCE_KT_TEST__)
enum class eltwise_op_t {
    No_Eltwise = 0,
    Add,
    Sub,
    Mul,
    Max
};

enum class ClipReluMode_t {
    NoClipRelu = 0,
    ScalarClipRelu
 };
#endif

enum class lsb_mask_t {
    Disable = 0,
    Mask_1_LSB,
    Mask_2_LSBs,
    Mask_3_LSBs,
    Mask_4_LSBs,
};

enum class instr_id_t {
    ID_0 = 0,
    ID_1 = 1,
    ID_2 = 2,
    ID_3 = 3,
};

#endif

enum class SELMODE : uint8_t {
    VSEL_CMPMASK_SPR = 0,
    VSEL_TENSOR_SCALAR_MODE,
    VSEL_TENSOR_TENSOR_MODE,
};

enum class DeqScale : uint8_t {
    DEQ_NONE = 0,
    DEQ,
    VDEQ,
    DEQ8,
    VDEQ8,
    DEQ16,
    VDEQ16,
};

enum class ReduceOrder : uint8_t {
    ORDER_VALUE_INDEX = 0,
    ORDER_INDEX_VALUE,
    ORDER_ONLY_VALUE,
    ORDER_ONLY_INDEX,
};

template <typename T> __aicore__ inline uint64_t GetScalarBitcodeValue(T scalarValue)
{
    union ScalarBitcode {
        __aicore__ ScalarBitcode() {}
        T input;
        uint64_t output;
    } data;

    data.input = scalarValue;
    return data.output;
}

template <typename T, typename U> __aicore__ inline U GetScalarBitcodeValue(T scalarValue)
{
    union ScalarBitcode {
        __aicore__ ScalarBitcode() {}
        T input;
        U output;
    } data;

    data.input = scalarValue;
    return static_cast<U>(data.output);
}

} // namespace AscendC
#endif // ASCENDC_MODULE_UTILS_STRUCT_NORM_SORT_H