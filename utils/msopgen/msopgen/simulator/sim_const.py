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

"""
Function:
simulator const parameters
"""
from enum import Enum


class DumpType(Enum):
    """
    simulator dump file type
    """
    InstrDump = "instr"
    InstrPopDump = "instr_pop"
    IcacheDump = "icache"


class Const:
    """
    simulator constant
    """
    SET_FLAG = "set_flag"
    WAIT_FLAG = "wait_flag"
    BARRIER = "barrier"
    MTE2_INSTR = "mov_out_to_ub"
    ICACHE_MISS_DUMP_2RD = "dcache_log"
    ICACHE_MISS_DUMP_1ST = "icache_log"
    INSTR_DUMP = "instr_log"
    INSTR_POP_DUMP = "instr_popped_log"
    REG_DUMP = "reg_log"
    VECTOR_EMPTY_INSTR = "VECTOR Repeat0"
    TICK = "tick"
    CTRL_INSTR = "FLOWCTRL"
    BANK_CONFLICT_RD = "bankConflictRD"
    BACK_CONFLICT_WR = "bankConflictWR"
    REGISTER_XD = "xdValue"
    PIP_NAME = "name"
    PC_ADDR = "addr"
    START_TIME = "start"
    END_TIME = "end"
    DURATION = "duration"
    DETAIL_INFO = "detail"
    ICACHEMISS = "ICmiss"
    REQ_ID = "req_id"
    RAW_STR = "raw_str"
    REG_PC_FLAG = "spr_pc"

    # instr type
    MTE1 = "MTE1"
    MTE2 = "MTE2"
    MTE3 = "MTE3"
    SCALAR = "SCALAR"
    VECTOR = "VECTOR"
    FLOWCTRL = "FLOWCTRL"
    EVENT = "EVENT"
    CUBE = "CUBE"
    FC = "FC"
    FIX_PIPE = "FIXP"

    # sim profile
    INSTR_NAME = "instr name"
    CALL_COUNT = "call count"
    CYCLE = "cycles"
    PARAM = "params"
    START_END = "start end"
    INSTR_DETAIL = "instr detail"
    LINE = "line"
