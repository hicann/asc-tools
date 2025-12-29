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
precision info module
"""
from msopst.common import op_status


class PrecisionStandard:
    """
    precision standard
    """

    def __init__(self, rtol, atol, max_atol=None, precision_type="percent"):
        """
        init methos
        :param rtol: The relative tolerance parameter
        :param atol: The absolute tolerance parameter
        :param max_atol: The max absolute tolerance parameter
        """
        self.precision_type = precision_type
        self.rtol = rtol
        self.atol = atol
        self.max_atol = max_atol

    @staticmethod
    def parse_json_obj(json_obj):
        """
        parser json obj to PrecisionStandard
        :param json_obj: json obj
        :return: PrecisionStandard
        """
        if json_obj:
            return PrecisionStandard(json_obj['rtol'], json_obj['atol'], json_obj['max_atol'],
                                     json_obj['precision_type'])

        return None

    def to_json_obj(self):
        """
        get json obj
        :return: json obj
        """
        return {
            "precision_type": self.precision_type,
            "rtol": self.rtol,
            "atol": self.atol,
            "max_atol": self.max_atol
        }


class PrecisionCompareResult:
    """
    precision compare result
    """

    def __init__(self, status, err_msg=None):
        self.status = status
        self.err_msg = err_msg

    @staticmethod
    def parse_json_obj(json_obj):
        """
        parser json obj to PrecisionStandard
        :param json_obj: json obj
        :return: PrecisionStandard
        """
        return PrecisionCompareResult(json_obj['status'], json_obj['err_msg'])

    def is_success(self):
        """
        check success
        :return: True or False
        """
        return self.status == op_status.SUCCESS

    def to_json_obj(self):
        """
        get json obj
        :return: json obj
        """
        return {
            "status": self.status,
            "err_msg": self.err_msg
        }


def get_default_standard(dtype):
    """
    get default standard by dtype
    :param dtype: dtype
    :return: PrecisionStandard
    """
    if dtype == "float16":
        return PrecisionStandard(0.001, 0.001, 0.1)
    if dtype == "float32":
        return PrecisionStandard(0.0001, 0.0001, 0.01)
    if dtype in ("int8", "uint8"):
        return PrecisionStandard(0.001, 1, 1, precision_type="absolute")

    return PrecisionStandard(0.001, 0.001, 0.1)
