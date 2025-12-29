#!/usr/bin/env python
# coding=utf-8

"""
Function:
This file mainly involves class for parsing operator info.
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

from msopgen.interface.arg_parser import ArgParser
from msopgen.interface.op_info_ir import IROpInfo
from msopgen.interface.op_info_tf import TFOpInfo
from msopgen.interface.op_info_ir_mindspore import MSIROpInfo
from msopgen.interface.op_info_tf_mindspore import MSTFOpInfo
from msopgen.interface.op_info_ir_json import JsonIROpInfo
from msopgen.interface.op_info_ir_json_mindspore import JsonMSIROpInfo
from msopgen.interface.const_manager import ConstManager
from msopgen.interface import utils


class OpInfoParser:
    """
    CLass for parsing operator info
    """

    def __init__(self: any, argument: ArgParser) -> None:
        self.op_info = self._create_op_info(argument)
        self.op_info.parse()

    @staticmethod
    def get_gen_flag() -> str:
        """
        get gen flag
        """
        return ""

    @staticmethod
    def _create_op_info(argument: ArgParser) -> any:
        if argument.input_path.endswith(ConstManager.INPUT_FILE_EXCEL):
            utils.print_warn_log("Excel cannot be used as inputs in future "
                                 "versions. It is recommended that json "
                                 "files be used as inputs.")
            if argument.gen_flag and argument.framework in ConstManager.FMK_MS:
                return MSIROpInfo(argument)
            return IROpInfo(argument)
        if argument.input_path.endswith(ConstManager.INPUT_FILE_JSON):
            if argument.gen_flag and argument.framework in ConstManager.FMK_MS:
                return JsonMSIROpInfo(argument)
            return JsonIROpInfo(argument)
        if argument.gen_flag and argument.framework in ConstManager.FMK_MS:
            return MSTFOpInfo(argument)
        return TFOpInfo(argument)

    def get_op_info(self: any) -> any:
        """
        get op info
        """
        return self.op_info
