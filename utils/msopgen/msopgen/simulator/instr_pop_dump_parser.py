#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Function:
instr pop dump parser
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
import re
from msopgen.simulator.sim_dump_parser import SimDumpParser
from msopgen.simulator.instr_pop_dump_record import InstrPopDumpRecord
from msopgen.simulator import utils


class InstrPopDumpParser(SimDumpParser):
    def update_instr_rule(self: any) -> None:
        self.update_instr_dump_rule()

    def get_instr_list(self: any) -> list:
        unify_lines = self._get_instr_list()
        # Sort by Tick
        sorted_tick_list = sorted(unify_lines, key=self.get_tick_key)
        return sorted_tick_list

    def get_pc_start_addr(self) -> str:
        self.update_instr_dump_rule()
        instr_list = self.get_instr_list()
        pc_pattern = r"^(0x)?[0-9a-f]+$"
        if not instr_list:
            err = "Parsing instr pop dump error: empty parsing output."
            raise utils.Dump2TraceException(err)
        if not instr_list[0].pc:
            err = "Parsing instr pop dump error: no pc attr in InstrPopDumpRecord object."
            raise utils.Dump2TraceException(err)
        if not re.match(pc_pattern, instr_list[0].pc):
            err = f"Parsing instr pop dump error: no pc address in raw info: '{instr_list[0].raw_record}'"
            raise utils.Dump2TraceException(err)
        return instr_list[0].pc

    def _get_instr_list(self) -> list:
        unify_lines = []
        with open(self._file_path, "r") as file:
            lines = file.readlines()
            for line in lines:
                line = line.strip()
                if not line or (self.ignore_list and InstrPopDumpParser.judge_ignore(line, self.ignore_list)):
                    continue
                for key, value in self.replace_map.items():
                    line = line.replace(key, value, 1)
                instr = InstrPopDumpRecord()
                rec = instr.parse(line)
                if not rec.tick or not rec.pc:
                    continue
                if not rec.is_partial_issue:
                    unify_lines.append(rec)
                else:
                    InstrPopDumpRecord.unresolve_instr_list.append(rec)
        return unify_lines
