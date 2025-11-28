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
 * \file ascend610_ini.h
 * \brief
 */
#ifndef ASCEND610_INI_H
#define ASCEND610_INI_H
#include <unordered_set>
#include <string>

namespace AscendC {
namespace check {
enum class PlatFormParams {
    CUBE_M_SIZE = 16,
    CUBE_N_SIZE = 16,
    CUBE_K_SIZE = 16,
    L0A_SIZE = 65536,
    L0B_SIZE = 65536,
    L0C_SIZE = 262144,
    L1_SIZE = 1048576,
    SMASK_BUFFER = 256,
    UB_SIZE = 262144,
    BT_SIZE = 0,
    FB0_SIZE = 0,
    FB1_SIZE = 0,
    FB2_SIZE = 0,
    FB3_SIZE = 0,
    UB_BLOCK_SIZE = 32,
    UB_BANK_SIZE = 4096,
    UB_BANK_NUM = 64,
    ONE_BLK_SIZE = 32,
    ONE_REP_BYTE_SIZE = 256,
    BLK_NUM_PER_REP = 8,
    BLOCK_LEN = 8,
};

class SocParams {
public:
    static SocParams& Instance()
    {
        static SocParams instance;
        return instance;
    }
    const std::string socVersion = "Ascend610";
    const std::unordered_set<std::string> supportInstrinsic {
        "vrec",
        "vadd",
        "vadds",
        "vsub",
        "vdiv",
        "vrsqrt",
        "vmul",
        "vmax",
        "vmaxs",
        "vmin",
        "vmins",
        "vln",
        "vexp",
        "vmuls",
        "vabs",
        "vcmax",
        "vcgmax",
        "vcmin",
        "vcgmin",
        "vcadd",
        "vcgadd",
        "vcpadd",
        "vconv",
        "mmad",
        "vor",
        "vand",
        "vaxpy",
        "vnot",
        "vsqrt",
        "vrelu",
        "vmla",
        "vmadd",
        "vmaddrelu",
        "vsel",
        "vcmp",
        "vcmpv_ne",
        "vcmpv_eq",
        "vcmpv_gt",
        "vcmpv_ge",
        "vcmpv_lt",
        "vcmpv_le",
        "sqrt",
        "abs",
        "bcnt0",
        "bcnt1",
        "clz",
        "max",
        "min",
        "vcmpvs_ne",
        "vcmpvs_eq",
        "vcmpvs_gt",
        "vcmpvs_ge",
        "vcmpvs_lt",
        "vcmpvs_le",
        "depthwise_conv",
        "vtranspose",
        "broadcast_ub_to_cc",
        "v4dtrans",
        "vgather",
        "vpadding",
        "vscatter",
        "vmergech",
        "vrpac",
        "vaadd",
        "viou",
        "vbitsort",
        "vextract",
        "vconcat",
        "vmrgsort4",
        "vreduce",
        "vadddeqrelu",
        "vmulconv",
        "scatter_vector_mov",
        "scatter_vabs",
        "scatter_vexp",
        "scatter_vrelu",
        "scatter_vrec",
        "scatter_vln",
        "scatter_vrsqrt",
        "scatter_vsqrt",
        "scatter_vadds",
        "scatter_vmuls",
        "scatter_vaxpy",
        "scatter_vmaxs",
        "scatter_vmins",
        "scatter_vmulconv",
        "scatter_vsel",
        "scatter_vconv",
        "scatter_vcmp",
        "scatter_vadd",
        "scatter_vsub",
        "scatter_vmul",
        "scatter_vmax",
        "scatter_vmin",
        "scatter_vdiv",
        "scatter_vmadd",
        "scatter_vmaddrelu",
        "scatter_vmla",
        "vbi",
        "vci",
        "vnchwconv",
        "vlrelu",
        "vaddrelu",
        "vsubrelu",
        "vaddreluconv",
        "vsubreluconv",
        "vcbd",
        "vshl",
        "vshr",
        "vdp",
        "scatter_vnchwconv",
        "rpn_cor",
        "rpn_cor_diag",
        "data_move_out2l1",
        "data_move_out2l0a",
        "data_move_out2l0b",
        "data_move_l12l0a",
        "data_move_l12l0b",
        "data_move_l0c2ub",
        "data_move_l12ub",
        "data_move_ub2l1",
        "data_move_ub2out",
        "data_move_out2ub",
        "data_move_ub2ub",
    };
private:
    SocParams() = default;
    ~SocParams() = default;
};

} // namespace check
} // namespace AscendC
#endif