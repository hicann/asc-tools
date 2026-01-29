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
 * \file MC62CM12AA_ini.h
 * \brief
 */
#ifndef MC62CM12AA_INI_H
#define MC62CM12AA_INI_H
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
    SMASK_BUFFER = 0,
    UB_SIZE = 253952,
    BT_SIZE = 1024,
    FB0_SIZE = 2048,
    FB1_SIZE = 1024,
    FB2_SIZE = 1024,
    FB3_SIZE = 2048,
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
    const std::string socVersion = "MC62CM12AA";
    const std::unordered_set<std::string> supportInstrinsic {
        "vrec",
        "vadd",
        "vadds",
        "vsub",
        "vdiv",
        "vrsqrt",
        "vmul",
        "vmax",
        "vmin",
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
        "vtranspose",
        "vlrelu",
        "vbitsort",
        "vmrgsort4",
        "vmulconv",
        "vnchwconv",
        "vcmpvs_ne",
        "vcmpvs_eq",
        "vcmpvs_gt",
        "vcmpvs_ge",
        "vcmpvs_lt",
        "vcmpvs_le",
        "vmaxs",
        "vmins",
        "vmaddrelu",
        "vgather",
        "vreduce",
        "vshr",
        "vgatherb",
        "vbrcb",
        "vbi",
        "vreducev2",
        "vbitsort32",
        "vmrgsort4v2",
        "vshl",
        "vcopy",
        "data_move_out2l1",
        "data_move_out2l0a",
        "data_move_out2l0b",
        "data_move_l12l0a",
        "data_move_l12l0b",
        "data_move_l12out",
        "data_move_l12bt",
        "data_move_transpose_l12l0a",
        "data_move_transpose_l12l0b",
        "set_l1",
        "set_l0a",
        "set_l0b",
        "fix_pipe_l0c2l1",
        "fix_pipe_l0c2out",
        "fix_pipe_l12fb",
        "fix_pipe_unit_list",
        "fix_pipe_pre_conv_func_list",
        "fix_pipe_pre_conv_requant",
        "fix_pipe_pre_conv_dequant",
        "fix_pipe_pre_conv_quant",
        "fix_pipe_pre_conv_cast",
        "fix_pipe_pre_act_func_list",
        "fix_pipe_pre_act_normal_relu",
        "fix_pipe_pre_act_scalar_relu",
        "fix_pipe_pre_act_vector_relu",
        "fix_pipe_post_transform_func_list",
        "fix_pipe_post_transform_nz2nd",
        "data_move_out2l1_nd2nz",
        "vaddrelu",
        "data_move_pad",
    };
private:
    SocParams() = default;
    ~SocParams() = default;
};
} // namespace check
} // namespace AscendC
#endif