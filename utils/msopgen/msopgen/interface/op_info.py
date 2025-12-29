#!/usr/bin/env python
# coding=utf-8

"""
Function:
This file mainly involves class for operator info.
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
import collections


class OpInfo:
    """
    OpInfo store the op informat for generate the op files,
    parsed_input_info and parsed_output_info is dicts,eg:
    {name:
    {
    ir_type_list:[],
    param_type:""required,
    format_list:[]
    }
    }
    """

    def __init__(self: any) -> None:
        self.op_type = ""
        self.fix_op_type = ""
        self.parsed_input_info = collections.OrderedDict()
        self.parsed_output_info = collections.OrderedDict()
        self.parsed_attr_info = []

    def get_op_type(self: any) -> str:
        """
        get op type
        """
        return self.op_type

    def get_fix_op_type(self: any) -> str:
        """
        get fix op type
        """
        return self.fix_op_type
