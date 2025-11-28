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
 * \file kernel_mmad_check.h
 * \brief
 */

#ifndef ASCENDC_MMAD_CHECK_H
#define ASCENDC_MMAD_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppMmadCheck : public TikcppBaseCheck {
public:
    TikcppMmadCheck(const std::string& name, MmadApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppMmadCheck() override = default;
    bool CheckMmadParamsRange(const uint32_t num, const std::string &paramName) const;
    bool CheckMmadOverflow(const std::string& errMsg);
    bool CheckAllHighLevel();
public:
    MmadApiParams& param_;
};
}
}
#endif