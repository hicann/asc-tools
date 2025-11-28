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
 * \file kernel_check_util.h
 * \brief
 */

#ifndef ASCENDC_CHECK_UTIL_H
#define ASCENDC_CHECK_UTIL_H
#if ASCENDC_CPU_DEBUG
#include <string>
#include "kernel_utils.h"
#include "kernel_check_vec_padding_util.h"
#include "utils/kernel_check_vec_util.h"
#include "utils/kernel_check_vec_select_util.h"
#include "utils/kernel_check_vec_reduce_util.h"
#include "utils/kernel_check_vec_data_filling_util.h"
#include "utils/kernel_check_vec_binary_util.h"
#include "utils/kernel_check_data_copy_util.h"
#include "utils/kernel_check_cube_util.h"

#endif
#endif