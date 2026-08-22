/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include <iostream>
#include <string>

#include "MSBit.h"

extern "C" {
void MSBitStart(const char* output, uint16_t length);
}

void MSBitAtInit()
{
    Bind(InstrType::COPY_GM_TO_UBUF, "__sanitizer_report_copy_gm_to_ubuf", {0, 1, 2});
    Bind(InstrType::COPY_GM_TO_UBUF_ALIGN_B8, "__sanitizer_report_copy_gm_to_ubuf_align_b8", {0, 1, 2, 3});
    Bind(InstrType::COPY_GM_TO_UBUF_ALIGN_B16, "__sanitizer_report_copy_gm_to_ubuf_align_b16", {0, 1, 2, 3});
    Bind(InstrType::COPY_GM_TO_UBUF_ALIGN_B32, "__sanitizer_report_copy_gm_to_ubuf_align_b32", {0, 1, 2, 3});
    Bind(InstrType::COPY_GM_TO_UBUF_ALIGN_V2_B8, "__sanitizer_report_copy_gm_to_ubuf_align_v2_b8", {0, 1, 2, 3});
    Bind(InstrType::COPY_GM_TO_UBUF_ALIGN_V2_B16, "__sanitizer_report_copy_gm_to_ubuf_align_v2_b16", {0, 1, 2, 3});
    Bind(InstrType::COPY_GM_TO_UBUF_ALIGN_V2_B32, "__sanitizer_report_copy_gm_to_ubuf_align_v2_b32", {0, 1, 2, 3});
    Bind(InstrType::COPY_UBUF_TO_GM_ALIGN_V2, "__sanitizer_report_copy_ubuf_to_gm_align_v2", {0, 1, 2, 3});
    Bind(InstrType::SET_FLAG, "__sanitizer_report_set_flag", {0, 1, 2});
    Bind(InstrType::SET_FLAGI, "__sanitizer_report_set_flagi", {0, 1, 2});
    Bind(InstrType::WAIT_FLAG, "__sanitizer_report_wait_flag", {0, 1, 2});
    Bind(InstrType::WAIT_FLAGI, "__sanitizer_report_wait_flagi", {0, 1, 2});
    Bind(InstrType::GET_BUF, "__sanitizer_report_get_buf", {0, 1, 2});
    Bind(InstrType::GET_BUFI, "__sanitizer_report_get_bufi", {0, 1, 2});
    Bind(InstrType::RLS_BUF, "__sanitizer_report_rls_buf", {0, 1, 2});
    Bind(InstrType::RLS_BUFI, "__sanitizer_report_rls_bufi", {0, 1, 2});
    Bind(InstrType::SET_FLAG_V, "__sanitizer_report_set_flag_v", {0, 1});
    Bind(InstrType::SET_FLAGI_V, "__sanitizer_report_set_flagi_v", {0, 1});
    Bind(InstrType::WAIT_FLAG_V, "__sanitizer_report_wait_flag_v", {0, 1});
    Bind(InstrType::WAIT_FLAGI_V, "__sanitizer_report_wait_flagi_v", {0, 1});
    Bind(InstrType::GET_BUF_V, "__sanitizer_report_get_buf_v", {0, 1, 2});
    Bind(InstrType::GET_BUFI_V, "__sanitizer_report_get_bufi_v", {0, 1, 2});
    Bind(InstrType::RLS_BUF_V, "__sanitizer_report_rls_buf_v", {0, 1, 2});
    Bind(InstrType::RLS_BUFI_V, "__sanitizer_report_rls_bufi_v", {0, 1, 2});
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output.ctrl.bin>\n";
        return 2;
    }
    const std::string outputPath = argv[1];
    MSBitStart(outputPath.c_str(), static_cast<uint16_t>(outputPath.size()));
    std::cout << "[CTRL] output=" << outputPath << " bindings=24\n";
    return 0;
}
