#!/usr/bin/env python
# coding=utf-8
"""
Function:
This file mainly involves class for generating operator files.
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
from msopgen.interface.op_file_aicore import OpFileAiCore
from msopgen.interface.op_file_vectorcore import OpFileVectorCore
from msopgen.interface.op_file_aicpu import OpFileAiCpu
from msopgen.interface.op_file_mindspore_aicore import OpFileMindSporeAiCore
from msopgen.interface.op_file_mindspore_aicpu import OpFileMindSporeAICPU
from msopgen.interface import utils
from msopgen.interface.const_manager import ConstManager


class OpFileGenerator:
    """
    CLass for generating operator files
    """

    def __init__(self: any, argument: ArgParser) -> None:
        self.op_file = self._create_op_file(argument)

    @staticmethod
    def _create_op_file(argument: ArgParser) -> any:
        if argument.framework in ConstManager.FMK_MS:
            if argument.core_type == ConstManager.AICORE:
                utils.print_info_log(
                    "Start to generate MindSpore AI core operator files.")
                return OpFileMindSporeAiCore(argument)

            if argument.core_type == ConstManager.AICPU:
                utils.print_info_log(
                    "Start to generate MindSpore AI CPU operator files.")
                return OpFileMindSporeAICPU(argument)

        if argument.core_type == ConstManager.AICORE:
            utils.print_info_log(
                "Start to generate AI Core operator files.")
            return OpFileAiCore(argument)
        if argument.core_type == ConstManager.VECTORCORE:
            utils.print_info_log(
                "Start to generate Vector Core operator files.")
            return OpFileVectorCore(argument)
        utils.print_info_log("Start to generate AI CPU operator files.")
        return OpFileAiCpu(argument)

    def generate(self: any) -> None:
        """
        generate op files
        """
        self.op_file.generate()

    def get_op_file(self: any) -> any:
        """
        get op files
        """
        return self.op_file
