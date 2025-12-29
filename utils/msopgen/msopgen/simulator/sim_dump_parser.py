#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Function:
Abstract class using for simulator dump log parser
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
from abc import ABCMeta
from abc import abstractmethod


class SimDumpParser(metaclass=ABCMeta):
    """
    simulator dump file parser
    """
    def __init__(self: any, file_path: str, core_id: str) -> None:
        self._file_path = file_path
        self._core_id = core_id
        self.ignore_list = []
        self.replace_map = {}

    @staticmethod
    def get_tick_key(obj: object) -> int:
        return obj.tick

    @staticmethod
    def judge_ignore(line, ignore_list):
        for ign in ignore_list:
            if ign in line:
                return True
        return False

    @abstractmethod
    def update_instr_rule(self: any) -> None:
        pass

    @abstractmethod
    def get_instr_list(self: any) -> list:
        pass

    def update_instr_dump_rule(self: any):
        self.ignore_list.append("core:")
        self.ignore_list.append("++++++++++++++++++++++")
        self.replace_map["[info] "] = ""
        self.replace_map[")SCALAR"] = ") SCALAR"
        self.replace_map[")"] = ")@" + self._core_id.upper()
