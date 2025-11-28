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
 * \file kernel_vec_compare_scalar_check.h
 * \brief
 */

#ifndef ASCENDC_VEC_COMPARE_SCALAR_CHECK_H
#define ASCENDC_VEC_COMPARE_SCALAR_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppVecCompareScalarCheck : public TikcppBaseCheck {
public:
    TikcppVecCompareScalarCheck(const std::string& name, VecBinaryScalarApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppVecCompareScalarCheck() override = default;
    bool CheckAllLowLevel(std::vector<uint64_t> maskArray);
    bool CheckAllHighLevel();
    bool CheckAddrAlign();
    bool CommonCheck();
public:
    VecBinaryScalarApiParams& param_;
};
}
}
#endif