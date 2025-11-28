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
 * \file kernel_data_copy_pad_check.h
 * \brief
 */

#ifndef ASCENDC_DATA_COPY_PAD_CHECK_H
#define ASCENDC_DATA_COPY_PAD_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppDataCopyPadCheck : public TikcppBaseCheck {
public:
    TikcppDataCopyPadCheck(const std::string& name, DataCopyPadApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppDataCopyPadCheck() override = default;
    bool CheckPadParamters();
    bool CheckAllHighLevel();

private:
    DataCopyPadApiParams& param_;
};
}
}
#endif