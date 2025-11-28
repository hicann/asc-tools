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
 * \file kernel_cpu_check.h
 * \brief
 */

#ifndef ASCENDC_CHECK_H
#define ASCENDC_CHECK_H
#include "kernel_base_check.h"
#include "kernel_mmad_check.h"
#include "kernel_vec_binary_check.h"
#include "kernel_vec_binary_scalar_check.h"
#include "kernel_vec_dup_check.h"
#include "kernel_vec_broadcast_check.h"
#include "kernel_vec_gatherb_check.h"
#include "kernel_vec_gather_check.h"
#include "kernel_vec_transpose_check.h"
#include "kernel_vec_proposal_check.h"
#include "kernel_vec_sort_check.h"
#include "kernel_vec_reduce_check.h"
#include "kernel_vec_reduce_other_check.h"
#include "kernel_vec_reduce_other_whl_check.h"
#include "kernel_vec_broadcast_to_mm_check.h"
#include "kernel_copy_check.h"
#include "kernel_data_copy_check.h"
#include "kernel_data_copy_slice_check.h"
#include "kernel_loaddata_check.h"
#include "kernel_vec_gather_mask_check.h"
#include "kernel_vec_compare_scalar_check.h"
#include "kernel_data_copy_pad_check.h"
#include "kernel_vec_bilinearinterpolation_check.h"
#include "kernel_vec_createvecindex_check.h"
#include "kernel_cube_initconstvalue_check.h"
#include "kernel_vector_padding_check.h"
#include "kernel_vec_scatter_check.h"
#include "kernel_vec_select_check.h"
#include "kernel_vec_compare_register_check.h"
#endif