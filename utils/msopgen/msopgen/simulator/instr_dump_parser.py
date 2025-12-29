#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Function:
instr dump parser
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
from msopgen.simulator.sim_dump_parser import SimDumpParser
from msopgen.simulator.instr_dump_record import InstrDumpRecord
from msopgen.simulator.sim_const import Const


class InstrDumpParser(SimDumpParser):
    def update_instr_rule(self: any) -> None:
        self.update_instr_dump_rule()

    def get_instr_list(self: any) -> list:
        unify_lines = []
        with open(self._file_path, "r") as file:
            lines = file.readlines()
            for line in lines:
                line = line.strip()
                if not line or (self.ignore_list and InstrDumpParser.judge_ignore(line, self.ignore_list)):
                    continue
                for key, value in self.replace_map.items():
                    line = line.replace(key, value, 1)
                instr_dump = InstrDumpRecord()
                rec = instr_dump.parse(line)
                if rec.instr_pipe == Const.VECTOR_EMPTY_INSTR:
                    continue
                if not rec.tick or not rec.pc:
                    continue
                unify_lines.append(rec)
            sorted_tick_list = sorted(unify_lines, key=InstrDumpParser.get_tick_key)
        return sorted_tick_list