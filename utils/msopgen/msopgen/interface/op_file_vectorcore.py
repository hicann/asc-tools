#!/usr/bin/env python
# coding=utf-8
"""
Function:
This file mainly involves class for generating vector core operator files.
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
from msopgen.interface.op_file_aicore import OpFileAiCore


class OpFileVectorCore(OpFileAiCore):
    """
    CLass for generate vector core op files
    """
    def _generate_project(self: any) -> None:
        self._new_operator()

    def _new_operator(self: any) -> None:
        self.generate_impl()
        self.generate_info_cfg()
