#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
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
import os
from msopgen.simulator.reg_dump_parser import RegDumpParser
from msopgen.simulator.instr_pop_dump_parser import InstrPopDumpParser
from msopgen.simulator import utils
from msopgen.simulator.sim_const import Const


class FirstPCParser:
    def __init__(self, core_prefix: str, dump_dir: str, core_id: str) -> None:
        self.core_prefix = core_prefix
        self.dump_dir = dump_dir
        self.core_id = core_id

    def parse_reg(self):
        reg_dump_path = os.path.join(
            self.dump_dir, f"{self.core_prefix}{Const.REG_DUMP}.dump")
        if not os.path.isfile(reg_dump_path):
            return ""
        reg_parser = RegDumpParser(reg_dump_path)
        return reg_parser.get_start_pc()

    def parse_instr_pop(self):
        instr_pop_dump_path = os.path.join(self.dump_dir, f"{self.core_prefix}{Const.INSTR_POP_DUMP}.dump")
        if not utils.CheckPath.check_file(instr_pop_dump_path):
            raise utils.Dump2TraceException(
                f"{Const.INSTR_POP_DUMP} not found in {self.dump_dir}")
        instr_pop_dump_parser = InstrPopDumpParser(
            instr_pop_dump_path, self.core_id)
        return instr_pop_dump_parser.get_pc_start_addr()

    def get_first_pc(self):
        first_pc = self.parse_reg()
        if not first_pc:
            first_pc = self.parse_instr_pop()
        utils.logger.info(f"{self.core_prefix} first pc: {first_pc}")
        return first_pc
