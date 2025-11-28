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
 * \file kernel_vec_proposal_check.h
 * \brief
 */

#ifndef ASCENDC_VEC_PROPOSAL_CHECK_H
#define ASCENDC_VEC_PROPOSAL_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppVecProposalCheck : public TikcppBaseCheck {
public:
    TikcppVecProposalCheck(const std::string& name, VecProposalApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppVecProposalCheck() override = default;

    bool CheckAllHighLevel();
    bool CheckAddrAlign(const std::string& src0Name);
    bool NeedRepeatTimes() const;
    bool Vbs16Check() const;
    bool Vbs32Check() const;
    bool Vms4Check() const;
    bool Vms4v2Check() const;
    bool VconcatCheck() const;
    bool VextractCheck() const;
    bool ConcatCheck() const;
    bool ExtractCheck() const;

    uint8_t CountBit(uint16_t validBit) const;
    bool CheckValidBit(uint16_t validBit) const;
    uint64_t CalSortElemPerRep(uint16_t elementLengths[4], uint8_t count) const;
public:
    VecProposalApiParams& param_;
};
}
}
#endif