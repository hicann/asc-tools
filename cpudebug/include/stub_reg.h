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
 * \file stub_reg.h
 * \brief
 */
#ifndef ASCENDC_STUB_REG_H
#define ASCENDC_STUB_REG_H
#include "intri_fun.h"
#include "intri_fmt.h"

namespace AscendC {
extern const int SYM_LEN_MAX;
void StubReg(IntriTypeT type, const char* stub);
void StubInit(void);
} // namespace AscendC
#endif // ASCENDC_STUB_REG_H
