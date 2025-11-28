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
 * \file kernel_vec_scatter_check.h
 * \brief
 */

#ifndef ASCENDC_VEC_SCATTER_CHECK_H
#define ASCENDC_VEC_SCATTER_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppVecScatterCheck : public TikcppBaseCheck {
public:
    TikcppVecScatterCheck(const std::string& name, VecScatterApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppVecScatterCheck() override = default;
    bool CheckAllHighLevel();
    bool CheckAllLowLevel(std::vector<uint64_t> maskArray);
    bool CheckAddrAlign();
    bool CheckTensorSize(std::vector<uint64_t> maskArray);
    bool CommonCheck();
private:
    VecScatterApiParams& param_;
};
}
}
#endif