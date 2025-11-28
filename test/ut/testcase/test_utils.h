/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#ifndef ASCENDC_TEST_UTILS_H
#define ASCENDC_TEST_UTILS_H

namespace AscendC {
#define LOCAL_TENSOR_REGISTER(tensor_name, type, que_pos, init_addr, data_size) \
    AscendC::LocalTensor<type> tensor_name;                                     \
    AscendC::TBuffAddr tbuf_##tensor_name;                                      \
    tbuf_##tensor_name.logicPos = (uint8_t)(AscendC::TPosition::que_pos);       \
    tensor_name.SetAddr(tbuf_##tensor_name);                                    \
    tensor_name.InitBuffer(init_addr, data_size);

#define ALIGN_ADDR(addr) (((addr) + 31) / 32 * 32)
}  // namespace AscendC
#endif  // TIK2_TEST_UTILS_H
