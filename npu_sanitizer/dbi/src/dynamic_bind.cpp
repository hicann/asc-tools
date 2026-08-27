/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "MSBit.h"
#include "ctrlbin_generator.h"

extern "C" void MSBitStart(const char* output, uint16_t length);

namespace {

struct BindingSpec {
    InstrType instrType;
    uint16_t apiId;
    const char* stubName;
    std::vector<uint16_t> paraMask;
};

thread_local const std::vector<aclsan::ProbeGroup>* g_selectedGroups = nullptr;

aclsan::ProbeGroup BindingGroup(const BindingSpec& binding)
{
    using aclsan::ProbeGroup;
    const uint16_t id = binding.apiId;
    if ((id >= 72 && id <= 82) || (id >= 84 && id <= 89) || id == 149 || id == 150 || id == 399) {
        return ProbeGroup::Mte2;
    }
    if (id == 83 || id == 173) {
        return ProbeGroup::Mte3;
    }
    if ((id >= 137 && id <= 148) || (id >= 153 && id <= 158) || (id >= 422 && id <= 426)) {
        return ProbeGroup::Mte1;
    }
    if (id == 91 || id == 92 || (id >= 167 && id <= 171)) {
        return ProbeGroup::Fixpipe;
    }
    return ProbeGroup::Sync;
}

bool IsSelected(const BindingSpec& binding)
{
    return g_selectedGroups == nullptr ||
           std::find(g_selectedGroups->begin(), g_selectedGroups->end(), BindingGroup(binding)) !=
               g_selectedGroups->end();
}

const std::vector<BindingSpec>& AllBindings()
{
    static const std::vector<BindingSpec> bindings = {
        // MTE2. Binding order is stable for ctrl.bin compatibility; probes follow instr.md order.
        // LOAD_OUT_TO_L1_2DV2.
        {InstrType::LOAD_GM_TO_CBUF_2DV2, 72, "__sanitizer_report_load_gm_to_cbuf_2dv2", {0, 1, 2, 3}},
        // MOV_OUT_TO_L1_V2.
        {InstrType::COPY_GM_TO_CBUF_V2, 73, "__sanitizer_report_copy_gm_to_cbuf_v2", {0, 1, 2, 3}},
        // MOV_OUT_TO_L1_ALIGN_V2.b8/b16/b32.
        {InstrType::COPY_GM_TO_CBUF_ALIGN_V2_B8, 74, "__sanitizer_report_copy_gm_to_cbuf_align_v2_b8", {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_CBUF_ALIGN_V2_B16, 75, "__sanitizer_report_copy_gm_to_cbuf_align_v2_b16", {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_CBUF_ALIGN_V2_B32, 76, "__sanitizer_report_copy_gm_to_cbuf_align_v2_b32", {0, 1, 2, 3}},
        // MOV_OUT_TO_L1_MULTI_ND2NZ.b8/b16/b32.
        {InstrType::COPY_GM_TO_CBUF_MULTI_ND2NZ_D_B8,
         77,
         "__sanitizer_report_copy_gm_to_cbuf_multi_nd2nz_d_b8",
         {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_CBUF_MULTI_ND2NZ_D_B16,
         78,
         "__sanitizer_report_copy_gm_to_cbuf_multi_nd2nz_d_b16",
         {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_CBUF_MULTI_ND2NZ_D_B32,
         79,
         "__sanitizer_report_copy_gm_to_cbuf_multi_nd2nz_d_b32",
         {0, 1, 2, 3}},
        // MOV_OUT_TO_L1_MULTI_DN2NZ.b8/b16/b32.
        {InstrType::COPY_GM_TO_CBUF_MULTI_DN2NZ_D_B8,
         80,
         "__sanitizer_report_copy_gm_to_cbuf_multi_dn2nz_d_b8",
         {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_CBUF_MULTI_DN2NZ_D_B16,
         81,
         "__sanitizer_report_copy_gm_to_cbuf_multi_dn2nz_d_b16",
         {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_CBUF_MULTI_DN2NZ_D_B32,
         82,
         "__sanitizer_report_copy_gm_to_cbuf_multi_dn2nz_d_b32",
         {0, 1, 2, 3}},
        // MOV_OUT_TO_UB_ALIGN_V2.b8/b16/b32.
        {InstrType::COPY_GM_TO_UBUF_ALIGN_V2_B8, 84, "__sanitizer_report_copy_gm_to_ubuf_align_v2_b8", {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_UBUF_ALIGN_V2_B16, 85, "__sanitizer_report_copy_gm_to_ubuf_align_v2_b16", {0, 1, 2, 3}},
        {InstrType::COPY_GM_TO_UBUF_ALIGN_V2_B32, 86, "__sanitizer_report_copy_gm_to_ubuf_align_v2_b32", {0, 1, 2, 3}},
        // ND_DMA_OUT_TO_UB.b8/b16/b32.
        {InstrType::NDDMA_OUT_TO_UB_B8, 87, "__sanitizer_report_nd_copy_gm_to_ubuf_b8", {0, 1, 2, 3}},
        {InstrType::NDDMA_OUT_TO_UB_B16, 88, "__sanitizer_report_nd_copy_gm_to_ubuf_b16", {0, 1, 2, 3}},
        {InstrType::NDDMA_OUT_TO_UB_B32, 89, "__sanitizer_report_nd_copy_gm_to_ubuf_b32", {0, 1, 2, 3}},
        // SET_L1_2D.b16/b32.
        {InstrType::SET_L1_2D_B16, 149, "__sanitizer_report_set_l1_2d_b16", {0, 1}},
        {InstrType::SET_L1_2D_B32, 150, "__sanitizer_report_set_l1_2d_b32", {0, 1}},
        // SET_MTE2_NZ_PARA. The low 16 bits provide matrixNum for subsequent MULTI copies in the same block.
        {InstrType::SET_MTE2_NZ_PARA, 399, "__sanitizer_report_set_mte2_nz_para", {0}},

        // MTE3: MOV_UB_TO_OUT_ALIGN_V2.
        {InstrType::COPY_UBUF_TO_GM_ALIGN_V2, 83, "__sanitizer_report_copy_ubuf_to_gm_align_v2", {0, 1, 2, 3}},
        // MOV_UB_TO_L1.
        {InstrType::COPY_UBUF_TO_CBUF, 173, "__sanitizer_report_copy_ubuf_to_cbuf", {0, 1, 2}},

        // MTE1: LOAD_L1_TO_L0B_2D_TRANSPOSE.b8/b16/b32/b4.
        {InstrType::LOAD_CBUF_TO_CB_TRANSPOSE_B8,
         137,
         "__sanitizer_report_load_cbuf_to_cb_2d_transpose_b8",
         {0, 1, 2, 3}},
        {InstrType::LOAD_CBUF_TO_CB_TRANSPOSE_B16,
         138,
         "__sanitizer_report_load_cbuf_to_cb_2d_transpose_b16",
         {0, 1, 2, 3}},
        {InstrType::LOAD_CBUF_TO_CB_TRANSPOSE_B32,
         139,
         "__sanitizer_report_load_cbuf_to_cb_2d_transpose_b32",
         {0, 1, 2, 3}},
        {InstrType::LOAD_CBUF_TO_CB_TRANSPOSE_B4,
         140,
         "__sanitizer_report_load_cbuf_to_cb_2d_transpose_b4",
         {0, 1, 2, 3}},
        // LOAD_L1_TO_L0A_2DV2.b4/b16/b8/b32.
        {InstrType::LOAD_CBUF_TO_CA_B4, 141, "__sanitizer_report_load_cbuf_to_ca_2dv2_b4", {0, 1, 2, 3, 4}},
        {InstrType::LOAD_CBUF_TO_CA_B16, 142, "__sanitizer_report_load_cbuf_to_ca_2dv2_b16", {0, 1, 2, 3, 4}},
        {InstrType::LOAD_CBUF_TO_CA_B8, 143, "__sanitizer_report_load_cbuf_to_ca_2dv2_b8", {0, 1, 2, 3, 4}},
        {InstrType::LOAD_CBUF_TO_CA_B32, 144, "__sanitizer_report_load_cbuf_to_ca_2dv2_b32", {0, 1, 2, 3, 4}},
        // LOAD_L1_TO_L0B_2DV2.b16/b4/b8/b32.
        {InstrType::LOAD_CBUF_TO_CB_B16, 145, "__sanitizer_report_load_cbuf_to_cb_2dv2_b16", {0, 1, 2, 3, 4}},
        {InstrType::LOAD_CBUF_TO_CB_B4, 146, "__sanitizer_report_load_cbuf_to_cb_2dv2_b4", {0, 1, 2, 3, 4}},
        {InstrType::LOAD_CBUF_TO_CB_B8, 147, "__sanitizer_report_load_cbuf_to_cb_2dv2_b8", {0, 1, 2, 3, 4}},
        {InstrType::LOAD_CBUF_TO_CB_B32, 148, "__sanitizer_report_load_cbuf_to_cb_2dv2_b32", {0, 1, 2, 3, 4}},
        // LOAD_L1_TO_L0A_3DV2.b16/b8 and LOAD_L1_TO_L0B_3DV2.b16/b8/b32.
        {InstrType::IMG2COLV2_CBUF_TO_CA_B16, 153, "__sanitizer_report_img2colv2_cbuf_to_ca_b16", {0, 1, 2, 3}},
        {InstrType::IMG2COLV2_CBUF_TO_CA_B8, 154, "__sanitizer_report_img2colv2_cbuf_to_ca_b8", {0, 1, 2, 3}},
        {InstrType::IMG2COLV2_CBUF_TO_CB_B16, 155, "__sanitizer_report_img2colv2_cbuf_to_cb_b16", {0, 1, 2, 3}},
        {InstrType::IMG2COLV2_CBUF_TO_CB_B8, 156, "__sanitizer_report_img2colv2_cbuf_to_cb_b8", {0, 1, 2, 3}},
        {InstrType::IMG2COLV2_CBUF_TO_CB_B32, 157, "__sanitizer_report_img2colv2_cbuf_to_cb_b32", {0, 1, 2, 3}},
        // MOV_L1_TO_UB.
        {InstrType::COPY_CBUF_TO_UBUF, 158, "__sanitizer_report_copy_cbuf_to_ubuf", {0, 1, 2}},
        // LOAD_L1_TO_L0A_3DV2.b32.
        {InstrType::IMG2COLV2_CBUF_TO_CA_B32, 422, "__sanitizer_report_img2colv2_cbuf_to_ca_b32", {0, 1, 2, 3}},
        // MOV_L1_TO_BT.f32/s32/f16/bf16.
        {InstrType::COPY_CBUF_TO_BT_F32, 423, "__sanitizer_report_copy_cbuf_to_bt_f32", {0, 1, 2}},
        {InstrType::COPY_CBUF_TO_BT_S32, 424, "__sanitizer_report_copy_cbuf_to_bt_s32", {0, 1, 2}},
        {InstrType::COPY_CBUF_TO_BT_F16, 425, "__sanitizer_report_copy_cbuf_to_bt_f16", {0, 1, 2}},
        {InstrType::COPY_CBUF_TO_BT_B16, 426, "__sanitizer_report_copy_cbuf_to_bt_b16", {0, 1, 2}},

        // FIX: FIX_L0C_TO_OUT.f32/s32.
        {InstrType::COPY_MATRIX_CC_TO_GM_F32_A5, 91, "__sanitizer_report_copy_matrix_cc_to_gm_f32_a5", {0, 1, 2, 3}},
        {InstrType::COPY_MATRIX_CC_TO_GM_S32_A5, 92, "__sanitizer_report_copy_matrix_cc_to_gm_s32_a5", {0, 1, 2, 3}},
        // MOV_L1_TO_FB.
        {InstrType::COPY_CBUF_TO_FBUF, 167, "__sanitizer_report_copy_cbuf_to_fbuf", {0, 1, 2}},
        // FIX_L0C_TO_L1.f32/s32.
        {InstrType::COPY_MATRIX_CC_TO_CBUF_F32, 168, "__sanitizer_report_copy_matrix_cc_to_cbuf_f32", {0, 1, 2, 3}},
        {InstrType::COPY_MATRIX_CC_TO_CBUF_S32, 169, "__sanitizer_report_copy_matrix_cc_to_cbuf_s32", {0, 1, 2, 3}},
        // FIX_L0C_TO_UB.f32/s32.
        {InstrType::COPY_MATRIX_CC_TO_UB_F32, 170, "__sanitizer_report_copy_matrix_cc_to_ubuf_f32", {0, 1, 2, 3}},
        {InstrType::COPY_MATRIX_CC_TO_UB_S32, 171, "__sanitizer_report_copy_matrix_cc_to_ubuf_s32", {0, 1, 2, 3}},

        // SYNCCHECK: SET/WAIT FLAG, API IDs 440-443, 445-446, 456-459, and 469-474.
        {InstrType::SET_FLAG, 440, "__sanitizer_report_set_flag", {0, 1, 2}},
        {InstrType::SET_FLAGI, 441, "__sanitizer_report_set_flagi", {0, 1, 2}},
        {InstrType::WAIT_FLAG, 442, "__sanitizer_report_wait_flag", {0, 1, 2}},
        {InstrType::WAIT_FLAGI, 443, "__sanitizer_report_wait_flagi", {0, 1, 2}},
        {InstrType::WAIT_FLAG_DEV, 445, "__sanitizer_report_wait_flag_dev_pipe", {0, 1}},
        {InstrType::WAIT_FLAG_DEVI, 446, "__sanitizer_report_wait_flag_devi_pipe", {0, 1}},
        {InstrType::SET_FLAG_V, 456, "__sanitizer_report_set_flag_v", {0, 1}},
        {InstrType::SET_FLAGI_V, 457, "__sanitizer_report_set_flagi_v", {0, 1}},
        {InstrType::WAIT_FLAG_V, 458, "__sanitizer_report_wait_flag_v", {0, 1}},
        {InstrType::WAIT_FLAGI_V, 459, "__sanitizer_report_wait_flagi_v", {0, 1}},
        {InstrType::WAIT_FLAG_DEV_V, 469, "__sanitizer_report_wait_flag_dev_pipe_v", {0}},
        {InstrType::WAIT_FLAG_DEVI_V, 470, "__sanitizer_report_wait_flag_devi_pipe_v", {0}},
        {InstrType::HSET_FLAG, 471, "__sanitizer_report_hset_flag", {0, 1, 2, 3, 4}},
        {InstrType::HSET_FLAGI, 472, "__sanitizer_report_hset_flagi", {0, 1, 2, 3, 4}},
        {InstrType::HWAIT_FLAG, 473, "__sanitizer_report_hwait_flag", {0, 1, 2, 3, 4}},
        {InstrType::HWAIT_FLAGI, 474, "__sanitizer_report_hwait_flagi", {0, 1, 2, 3, 4}},

        // SYNCCHECK: GET/RLS BUF, API IDs 448-451 and 460-463.
        {InstrType::GET_BUF, 448, "__sanitizer_report_get_buf", {0, 1, 2}},
        {InstrType::GET_BUFI, 449, "__sanitizer_report_get_bufi", {0, 1, 2}},
        {InstrType::RLS_BUF, 450, "__sanitizer_report_rls_buf", {0, 1, 2}},
        {InstrType::RLS_BUFI, 451, "__sanitizer_report_rls_bufi", {0, 1, 2}},
        {InstrType::GET_BUF_V, 460, "__sanitizer_report_get_buf_v", {0, 1}},
        {InstrType::GET_BUFI_V, 461, "__sanitizer_report_get_bufi_v", {0, 1}},
        {InstrType::RLS_BUF_V, 462, "__sanitizer_report_rls_buf_v", {0, 1}},
        {InstrType::RLS_BUFI_V, 463, "__sanitizer_report_rls_bufi_v", {0, 1}},
    };
    return bindings;
}

bool ValidateBindings(const std::vector<aclsan::ProbeGroup>& groups, std::string& diagnostic)
{
    std::set<uint16_t> apiIds;
    std::set<std::string> symbols;
    for (const auto& binding : AllBindings()) {
        if (std::find(groups.begin(), groups.end(), BindingGroup(binding)) == groups.end()) {
            continue;
        }
        const uint16_t enumId = static_cast<uint16_t>(binding.instrType);
        if (enumId != binding.apiId) {
            diagnostic = "binding ID mismatch for " + std::string(binding.stubName);
            return false;
        }
        if (!apiIds.insert(binding.apiId).second) {
            diagnostic = "duplicate instruction ID " + std::to_string(binding.apiId);
            return false;
        }
        if (!symbols.insert(binding.stubName).second) {
            diagnostic = "duplicate injected symbol " + std::string(binding.stubName);
            return false;
        }
    }
    return true;
}

#ifndef ASCSAN_CTRLBIN_NO_MAIN
std::string MaskToString(const std::vector<uint16_t>& mask)
{
    std::string out = "{";
    for (size_t i = 0; i < mask.size(); ++i) {
        if (i != 0) {
            out += ",";
        }
        out += std::to_string(mask[i]);
    }
    out += "}";
    return out;
}
#endif

bool IsNonEmptyFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    return input.good() && input.tellg() > 0;
}

} // namespace

extern "C" void MSBitAtInit()
{
    for (const auto& binding : AllBindings()) {
        if (IsSelected(binding)) {
            Bind(binding.instrType, binding.stubName, binding.paraMask);
        }
    }
}

#ifndef ASCSAN_CTRLBIN_NO_MAIN
int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output.ctrl.bin>\n";
        return 1;
    }
    const std::string outputPath = argv[1];
    std::string diagnostic;
    const auto groups = aclsan::AllProbeGroups();
    if (!aclsan::GenerateCtrlBin(outputPath, groups, diagnostic)) {
        std::cerr << "[gen-ctrlbin] " << diagnostic << '\n';
        return 2;
    }

    for (const auto& binding : AllBindings()) {
        std::cout << "[gen-ctrlbin] id=" << binding.apiId << " mask=" << MaskToString(binding.paraMask)
                  << " symbol=" << binding.stubName << '\n';
    }
    std::cout << "[gen-ctrlbin] wrote " << aclsan::BindingSymbols(groups).size() << " bindings to " << outputPath
              << '\n';
    return 0;
}
#endif

namespace aclsan {

std::vector<ProbeGroup> AllProbeGroups()
{
    return {ProbeGroup::Mte1, ProbeGroup::Mte2, ProbeGroup::Mte3, ProbeGroup::Fixpipe, ProbeGroup::Sync};
}

std::vector<std::string> BindingSymbols(const std::vector<ProbeGroup>& groups)
{
    std::vector<std::string> symbols;
    for (const auto& binding : AllBindings()) {
        if (std::find(groups.begin(), groups.end(), BindingGroup(binding)) != groups.end()) {
            symbols.emplace_back(binding.stubName);
        }
    }
    return symbols;
}

bool GenerateCtrlBin(const std::string& outputPath, const std::vector<ProbeGroup>& groups, std::string& diagnostic)
{
    std::vector<ProbeGroup> selected(groups.begin(), groups.end());
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
    if (selected.empty()) {
        diagnostic = "probe set is empty";
        return false;
    }
    if (!ValidateBindings(selected, diagnostic)) {
        return false;
    }
    if (outputPath.size() > UINT16_MAX) {
        diagnostic = "output path is too long";
        return false;
    }
    g_selectedGroups = &selected;
    MSBitStart(outputPath.c_str(), static_cast<uint16_t>(outputPath.size()));
    g_selectedGroups = nullptr;
    if (!IsNonEmptyFile(outputPath)) {
        diagnostic = "failed to generate a non-empty file: " + outputPath;
        return false;
    }
    diagnostic.clear();
    return true;
}

} // namespace aclsan
