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
 * \file kernel_struct_proposal.h
 * \brief
 */
#ifndef ASCENDC_MODULE_STRUCT_PROPOSAL_H
#define ASCENDC_MODULE_STRUCT_PROPOSAL_H
#include "utils/kernel_utils_constants.h"

namespace AscendC {
struct MrgSort4Info {
    __aicore__ MrgSort4Info() {}
    
    __aicore__ MrgSort4Info(const uint16_t elementLengthsIn[MRG_SORT_ELEMENT_LEN], const bool ifExhaustedSuspensionIn,
        const uint16_t validBitIn, const uint16_t repeatTimesIn)
        : ifExhaustedSuspension(ifExhaustedSuspensionIn),
          validBit(validBitIn),
          repeatTimes(repeatTimesIn)
    {
        for (int32_t i = 0; i < MRG_SORT_ELEMENT_LEN; ++i) {
            elementLengths[i] = elementLengthsIn[i];
        }
    }

    uint16_t elementLengths[MRG_SORT_ELEMENT_LEN] = { 0 };
    bool ifExhaustedSuspension = false;
    uint16_t validBit = 0;
    uint8_t repeatTimes = 1;
};
} // namespace AscendC
#endif // ASCENDC_MODULE_STRUCT_PROPOSAL_H
