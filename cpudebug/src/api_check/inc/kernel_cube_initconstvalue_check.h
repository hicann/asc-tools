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
 * \file kernel_cube_initconstvalue_check.h
 * \brief
 */

#ifndef ASCENDC_CUBE_INITCONSTVALUE_CHECK_H
#define ASCENDC_CUBE_INITCONSTVALUE_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {

class TikcppCubeInitConstValueCheck : public TikcppBaseCheck {
public:
    TikcppCubeInitConstValueCheck(const std::string& name, CubeInitConstValueApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppCubeInitConstValueCheck() override = default;

    bool CheckAllLowLevel();
public:
    CubeInitConstValueApiParams& param_;
};
}
}
#endif