#!/usr/bin/env python
# -*- coding:utf-8 -*-
# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2025 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

"""
precision compare util
"""
import numpy as np
from msopst.common import precision_info
from msopst.common import op_status
from msopst.common.precision_info import PrecisionStandard
from msopst.common.precision_info import PrecisionCompareResult


# 'pylint: disable=too-many-statements
def _get_np_dtype(d_type):
    res = np.float16
    if d_type.strip() == "float16":
        res = np.float16
    elif d_type.strip() == "float32":
        res = np.float32
    elif d_type.strip() == "float64" or d_type.strip() == "double":
        res = np.float64
    elif d_type.strip() == "int8":
        res = np.int8
    elif d_type.strip() == "int16":
        res = np.int16
    elif d_type.strip() == "int32":
        res = np.int32
    elif d_type.strip() == "uint8":
        res = np.uint8
    elif d_type.strip() == "uint16":
        res = np.uint16
    elif d_type.strip() == "uint32":
        res = np.uint32
    elif d_type.strip() == "bool":
        res = bool
    return res


def compare_precision(actual_data_file: str, expect_data_file: str,
                      precision_standard: PrecisionStandard) -> PrecisionCompareResult:
    """
    compare precision
    :param actual_data_file: actual data file or numpy array
    :param expect_data_file: expect data file or numpy array
    :param precision_standard: precision standard
    :return: PrecisionCompareResult
    """
    if not isinstance(actual_data_file, str):
        actual_data = actual_data_file.reshape([-1, ])
    else:
        actual_data_dt_str = actual_data_file[-14:]
        actual_data_dt_str = actual_data_dt_str[actual_data_dt_str.index("_") + 1:-4]
        np_dtype = _get_np_dtype(actual_data_dt_str)
        actual_data = np.fromfile(actual_data_file, np_dtype)

    if not isinstance(expect_data_file, str):
        expect_data = expect_data_file.reshape([-1, ])
        np_dtype = expect_data.dtype
    else:
        expect_data_dt_str = expect_data_file[-14:]
        expect_data_dt_str = expect_data_dt_str[expect_data_dt_str.index("_") + 1:-4]
        np_dtype = _get_np_dtype(expect_data_dt_str)
        expect_data = np.fromfile(expect_data_file, np_dtype)

    compare_size = min(len(actual_data), len(expect_data))
    if not precision_standard:
        precision_standard = precision_info.get_default_standard(np_dtype)

    actual_data = actual_data[:compare_size]
    expect_data = expect_data[:compare_size]

    def _compare_tensor():
        a_sub_b = actual_data - expect_data
        abs_a_sub_b = np.abs(a_sub_b)
        min_abs_a_b = np.abs(expect_data)
        atol_value = min_abs_a_b * precision_standard.atol
        max_cnt = 0
        if precision_standard.max_atol:
            max_atol_value = min_abs_a_b * precision_standard.max_atol
            max_cmp = np.less_equal(abs_a_sub_b, max_atol_value)
            max_cmp = max_cmp.astype(np.int8)
            max_cmp = 1 - max_cmp
            max_cnt = np.sum(max_cmp)

        less_cmp = np.less_equal(abs_a_sub_b, atol_value)
        less_cmp = less_cmp.astype(np.int8)
        less_cmp = 1 - less_cmp
        sum_cnt = np.sum(less_cmp)

        return sum_cnt, max_cnt

    def _compare_bool_tensor():
        xor_ab = np.logical_xor(actual_data, expect_data)
        xor_ab_int = xor_ab.astype(np.int8)
        sum_cnt = np.sum(xor_ab_int)
        return sum_cnt, 0

    if np_dtype == bool:
        rtol_cnt, max_atol_cnt = _compare_bool_tensor()
    else:
        rtol_cnt, max_atol_cnt = _compare_tensor()
    print("Error count: {}, Max atol error count: {}, Threshold count(rtol * data_size):{}"
          .format(rtol_cnt, max_atol_cnt, precision_standard.rtol * expect_data.size))

    status = op_status.SUCCESS
    err_msg = ""
    if rtol_cnt > precision_standard.rtol * expect_data.size:
        status = op_status.FAILED
        err_msg = "Error count (expect - actual > atol * expect): %s, rtol is: %s, total size: %s. " % (
            str(rtol_cnt), str(precision_standard.rtol), str(compare_size))

    if max_atol_cnt > 0:
        status = op_status.FAILED
        err_msg += "Max atol error count(expect - actual > max_atol * expect): %d, max_atol is: %s " % (
            max_atol_cnt, str(precision_standard.max_atol))

    return PrecisionCompareResult(status, err_msg)
