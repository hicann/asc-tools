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
 * \file kernel_data_copy_slice_check.h
 * \brief
 */

#ifndef ASCENDC_DATA_COPY_SLICE_CHECK_H
#define ASCENDC_DATA_COPY_SLICE_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppDataCopySliceCheck : public TikcppBaseCheck {
public:
    TikcppDataCopySliceCheck(const std::string& name, DataCopySliceApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppDataCopySliceCheck() override = default;
    bool CheckSliceInfoParamters(const std::string &errMsg);
    bool CheckDataCopyIntrsParamters(const std::string &errMsg);
    uint32_t DataCopyGetTotalInstrsNum(const SliceInfo sliceInfo[], const uint32_t shapeIn[]);
    bool CheckDataCopyInstrsNum(const std::string &errMsg);

    bool CheckAllHighLevel();
    bool CheckAddrAlign();
public:
    static constexpr uint32_t dataCopySliceAlignSize = 32;
    uint32_t alignBytes_{ dataCopySliceAlignSize };
    DataCopySliceApiParams& param_;
};
}
}
#endif