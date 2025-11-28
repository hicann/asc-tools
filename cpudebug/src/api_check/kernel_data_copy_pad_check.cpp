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
 * \file kernel_data_copy_pad_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_data_copy_pad_check.h"

namespace AscendC {
namespace check {
bool TikcppDataCopyPadCheck::CheckPadParamters()
{
    static constexpr uint32_t dataCopyPadPaddingSizeLimit = 32;     // pad element * sizeof(dtype) <= 32
    uint64_t paddingLimit = dataCopyPadPaddingSizeLimit / param_.srcDtypeBytes;
    if (param_.leftPadding > paddingLimit) {
        CHECK_LOG_ERROR("Failed to check leftPadding value in DataCopyPad, its valid range is 0 ~ %lu, current value "
            "is %u.", paddingLimit, param_.leftPadding);
        return false;
    }
    if (param_.rightPadding > paddingLimit) {
        CHECK_LOG_ERROR("Failed to check rightPadding value in DataCopyPad, its valid range is 0 ~ %lu, current value "
            "is %u.", paddingLimit, param_.rightPadding);
        return false;
    }

    if (param_.isPad && param_.srcDtypeBytes == B64_BYTE_SIZE && param_.paddingValue != 0) {
        CHECK_LOG_ERROR("Failed to check paddingValue value in DataCopyPad when dtype is B64, it must be 0, current "
            "value is %s.", std::to_string(param_.paddingValue).c_str());
        return false;
    }
    return true;
}

bool TikcppDataCopyPadCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CheckPadParamters());
    return true;
}
} // namespace check
} // namespace AscendC
