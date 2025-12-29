#!/usr/bin/env python
# coding=utf-8

"""
Function:
This file mainly involves class for IR operator info.
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

from msopgen.interface import utils
from msopgen.interface.op_info_tf import TFOpInfo


class MSTFOpInfo(TFOpInfo):
    """
    CLass representing operator info for Mindspore.
    """

    @staticmethod
    def _mapping_input_output_type(tf_type: str, name: str) -> str:
        return utils.CheckFromConfig().trans_ms_tf_io_dtype(tf_type, name)

    def get_op_path(self: any) -> str:
        """
        get op path
        """
        return self.op_path

    def get_op_type(self: any) -> str:
        """
        get op type
        """
        return self.op_type
