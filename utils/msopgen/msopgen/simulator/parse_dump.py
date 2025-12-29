#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

from .instr_execution_record import InstrExecutionRecord
from msopgen.simulator.sim_const import DumpType
from msopgen.simulator import utils


class ParseDump:
    """Parse dumps of a core
    parse simulator dump contains:xxx_icache_log.dump、xxx_instr_log.dump、xxx_instr_popped_log.dump and so on.
    Only open class of dump_parser module
    """

    def __init__(self, parse_info) -> None:
        self.dump_files = parse_info.dump_files
        self.core_id = parse_info.core_id
        self.instr_list = []
        self.cache_list = []

    @classmethod
    def _merge_instr_info(cls: any, dump_info: dict) -> list:
        merged_list = []
        instr_pop = {}
        for instr in dump_info.get(DumpType.InstrPopDump, []):
            value = instr_pop.get(instr.pc, [])
            value.append(instr)
            instr_pop[instr.pc] = value
        instr_pop_sorted = {key: sorted(
            value, key=lambda x: x.tick) for key, value in instr_pop.items()}
        instr_dict = {}
        for instr in dump_info.get(DumpType.InstrDump, []):
            value = instr_dict.get(instr.pc, [])
            value.append(instr)
            instr_dict[instr.pc] = value
        instr_sorted = {key: sorted(value, key=lambda x: x.tick)
                        for key, value in instr_dict.items()}
        for key, value in instr_sorted.items():
            while value and instr_pop_sorted.get(key):
                if len(instr_pop_sorted[key]) < 1:
                    continue
                exe_rec = InstrExecutionRecord(int(instr_pop_sorted[key][-1].tick),
                                               int(value[-1].tick),
                                               value[-1])
                value.pop()
                instr_pop_sorted[key].pop()
                merged_list.append(exe_rec)

            while value:
                exe_rec = InstrExecutionRecord(int(value[-1].tick),
                                               int(value[-1].tick),
                                               value[-1])
                value.pop()
                merged_list.append(exe_rec)
        if not merged_list:
            utils.logger.error("Issue and retire instr does not match")
        return merged_list

    def parse_dump_files(self):
        dump_info = {}
        for type_, path in self.dump_files:
            dump_info[type_] = self.parse_dump_file(
                path, type_, self.core_id)
        self.instr_list = self._merge_instr_info(dump_info)
        self.cache_list = dump_info.get(DumpType.IcacheDump, [])

    def parse_dump_file(self, dump_file_path: str, dump_type: DumpType, core_id: str) -> list:
        import importlib
        module_path = f"msopgen.simulator.{dump_type.value}_dump_parser"
        parser_module = importlib.import_module(module_path, package="op_gen")
        class_name = f"{dump_type.name}Parser"
        dump_parser_class = getattr(parser_module, class_name)
        dump_parser = dump_parser_class(dump_file_path, core_id)
        dump_parser.update_instr_rule()
        return dump_parser.get_instr_list()
