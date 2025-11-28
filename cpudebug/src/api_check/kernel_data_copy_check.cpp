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
 * \file kernel_data_copy_check.cpp
 * \brief
 */

#include "kernel_check_params.h"
#include "kernel_data_copy_check.h"

namespace AscendC {
namespace check {
bool TikcppDataCopyCheck::CheckAllHighLevel()
{
    ASCENDC_CHECK(CheckAddrAlign());
    return true;
}

bool TikcppDataCopyCheck::CheckDataCopyAlign(uint64_t addr, uint8_t pos, bool isSrc)
{
    // gm is 1 Byte aligned, so do not need to check aligned
    uint32_t dataType = (isSrc ? param_.srcDtypeBytes : param_.dstDtypeBytes);
    std::map<Hardware, uint32_t> alignSizeMap = {
        { Hardware::GM, 1 },
        { Hardware::L1, 32 },
        { Hardware::UB, 32 },
        { Hardware::L0A, 512 * ((dataType + 1) / sizeof(uint16_t)) },
        { Hardware::L0B, 512 * ((dataType + 1) / sizeof(uint16_t)) },
        { Hardware::L0C, 512 * ((dataType + 1) / sizeof(uint16_t)) },
        { Hardware::BIAS, 64 },
    };

    if (pos != static_cast<uint8_t>(Hardware::GM)) {
        uint32_t alignBytes = alignSizeMap[static_cast<Hardware>(pos)];
        if (isSrc) {
            ASCENDC_CHECK(CheckTensorAddrAlign(addr, pos, alignBytes, "src"));
        } else {
            ASCENDC_CHECK(CheckTensorAddrAlign(addr, pos, alignBytes, "dst"));
        }
    }
    return true;
}
bool TikcppDataCopyCheck::CheckAddrAlign()
{
    bool retSrc = CheckDataCopyAlign(param_.srcAddr, param_.srcPos, true);
    bool retDst = CheckDataCopyAlign(param_.dstAddr, param_.dstPos, false);
    return retSrc && retDst;
}
} // namespace check
} // namespace AscendC
