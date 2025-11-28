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
 * \file kernel_vec_reduce_check.h
 * \brief
 */

#ifndef ASCENDC_VEC_REDUCE_CHECK_H
#define ASCENDC_VEC_REDUCE_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {

class TikcppVecReduceCheck : public TikcppBaseCheck {
public:
    TikcppVecReduceCheck(const std::string& name, VecReduceApiParams &param) : TikcppBaseCheck(name), param_(param)
    {}
    ~TikcppVecReduceCheck() override = default;
    uint32_t AlignStartPos(const uint32_t startPos, const uint32_t byteLen) const;
    bool CheckAllDtypeBytes(const std::string &errMsg);
    void ReduceBodyCal(const std::vector<uint32_t> &paramsArray, uint32_t &outputCount, uint32_t &nextStartPos) const;
    bool CheckCheckWorkSize(const std::string &errMsg, const uint64_t needElements, const uint32_t byteLen);
    bool CheckWorkTensorOffset(const std::string& errMsg);
    bool CheckWorkTensorSizeEqual(const std::string& errMsg);
    bool CheckDstTensorSizeRange(const std::string& errMsg);
    bool CheckAllLowLevel(std::vector<uint64_t> maskArray);
    bool CheckAllHighLevel();
    bool CheckAllHighLevelMode2();
    bool CheckAddrAlign();
    bool CommonCheck();

public:
    VecReduceApiParams &param_;
};
}  // namespace check
}  // namespace AscendC
#endif