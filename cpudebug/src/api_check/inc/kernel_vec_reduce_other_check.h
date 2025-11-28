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
 * \file kernel_vec_reduce_other_check.h
 * \brief
 */

#ifndef ASCENDC_VEC_REDUCE_OTHER_CHECK_H
#define ASCENDC_VEC_REDUCE_OTHER_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {

class TikcppVecReduceOtherCheck : public TikcppBaseCheck {
public:
    TikcppVecReduceOtherCheck(const std::string& name, VecReduceApiParams &param) : TikcppBaseCheck(name), param_(param)
    {}
    ~TikcppVecReduceOtherCheck() override = default;
    bool CheckWholeReduceDtypeBytes(const std::string &errMsg);
    bool CheckWholeReduceDstSize();
    bool CheckWholeReduceDstSize(std::vector<uint64_t>& maskArray, const uint64_t unit, const std::string& tensorName);
    bool CheckPairReduceDstSize(std::vector<uint64_t>& maskArray, const std::string& tensorName);
    bool CheckRepeatReduceDstSize();
    bool CheckAllLowLevel(std::vector<uint64_t> maskArray);
    bool CheckAddrAlign();
    bool CheckTensorByteOverflowLow(std::vector<uint64_t>& maskArray, const TensorOverflowParams& params,
        const std::string& tensorName);
public:
    VecReduceApiParams &param_;
};
}  // namespace check
}  // namespace AscendC
#endif