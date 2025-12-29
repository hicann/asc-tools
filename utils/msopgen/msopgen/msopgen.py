#!/usr/bin/env python
# coding=utf-8
"""
Function:
This file mainly involves main function of op generation module.
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

import sys
from msopgen.interface.arg_parser import ArgParser
from msopgen.interface.op_file_generator import OpFileGenerator
from msopgen.interface.op_file_compile import OpFileCompile
from msopgen.interface import utils
from msopgen.interface.const_manager import ConstManager
from msopgen.simulator import Simulator, Dump2TraceException


def _do_gen_cmd(argument: ArgParser) -> None:
    op_file_generator = OpFileGenerator(argument)
    op_file_generator.generate()


def _do_compile_cmd(argument: ArgParser) -> None:
    op_project_compile = OpFileCompile(argument)
    op_project_compile.compile()


def _do_sim_cmd(argument: ArgParser) -> None:
    Simulator.run(argument)


def _msopgen_task():
    # 1.parse input argument and check arguments valid
    argument = ArgParser()
    # 2.generate file, according to gen and mi
    if argument.gen_flag:
        _do_gen_cmd(argument)
    elif argument.compile_flag:
        _do_compile_cmd(argument)
    else:
        _do_sim_cmd(argument)


def main():
    """
    main function
    """
    # 1.parse input argument and check arguments valid
    try:
        _msopgen_task()
    except (utils.MsOpGenException, Dump2TraceException) as ex:
        sys.exit(ex.error_info)
    finally:
        pass
    utils.print_info_log("Generation completed.")
    sys.exit(ConstManager.MS_OP_GEN_NONE_ERROR)


if __name__ == "__main__":
    main()
