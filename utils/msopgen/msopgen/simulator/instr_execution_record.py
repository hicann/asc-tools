#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Function:
Merge issue and retire inst records
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
from msopgen.simulator.instr_catagory import InstrCatagory
from msopgen.simulator.sim_const import Const


class InstrExecutionRecord:
    RENDER_NAME_DICT = {Const.SCALAR: "A",
                        Const.FLOWCTRL: "B",
                        Const.VECTOR: "C",
                        Const.CUBE: "D",
                        Const.MTE2: "E",
                        Const.MTE1: "F",
                        Const.MTE3: "G",
                        Const.EVENT: "H",
                        Const.FIX_PIPE: "I"}

    def __init__(self, start, end, instr_record):
        self.start = start
        self.end = end
        self.pipe = instr_record.instr_pipe
        self.instr_name = instr_record.instr_name
        self.instr_detail = instr_record.instr_detail
        self.instr_pc = instr_record.pc
        self.binary = instr_record.instr_binary
        self.detail = instr_record.instr_detail

        if hasattr(instr_record, "bank_conflict_rd") and hasattr(instr_record, "bank_conflict_wr"):
            self.bank_conflict_rd = instr_record.bank_conflict_rd
            self.bank_conflict_wr = instr_record.bank_conflict_wr

        if hasattr(instr_record, "params"):
            self.params = instr_record.params

        # change the catagory StatisticPipe, just for statistic
        self.rename_instr_pipe()
        if self.pipe == "VECTOR0" or self.pipe == "VEC":  # V200 model VECTOR==>VECTOR0
            self.pipe = Const.VECTOR
        if self.pipe == "FC":
            self.pipe = Const.FLOWCTRL
        self.render_pipe_name = InstrExecutionRecord.RENDER_NAME_DICT.get(self.pipe, "")

        self.instr_classif()

    def rename_instr_pipe(self):
        if self.instr_name.find("scalar_ld") != -1 or self.instr_name.find("scalar_st") != -1:
            self.pipe = Const.SCALAR
        if self.pipe.find("ISSUE") != -1:
            self.pipe = Const.EVENT

    # Classification
    def instr_classif(self):
        self.instr_catagory = InstrCatagory(self.pipe, self.instr_name)
