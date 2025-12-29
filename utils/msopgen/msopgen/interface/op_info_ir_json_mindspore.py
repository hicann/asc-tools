#!/usr/bin/env python
# coding=utf-8

"""
Function:
This file mainly involves class for IR JSON for mindspore operator info.
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
from msopgen.interface.op_info_ir_json import JsonIROpInfo
from msopgen.interface.const_manager import ConstManager


class JsonMSIROpInfo(JsonIROpInfo):
    """
    CLass for IR OP Info from Json for Mindspore.
    """

    @staticmethod
    def _mapping_input_output_type(ir_type: str, ir_name: str) -> any:
        file_type = ConstManager.INPUT_FILE_JSON
        return utils.CheckFromConfig().trans_ms_io_dtype(ir_type, ir_name,
                                                         file_type)

    @staticmethod
    def _init_op_format(input_output_map: dict, prefix: str, input_output_name: str,
                        type_list: list) -> any:
        ir_type_list, _ = type_list
        op_format = ",".join("ND" for _ in ir_type_list)
        op_format = op_format.split(",")
        return op_format

    def get_op_path(self: any) -> str:
        """
        get op path
        """
        return self.op_path

    def get_gen_flag(self: any) -> str:
        """
        get gen flag
        """
        return self.gen_flag
