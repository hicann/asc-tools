#!/usr/bin/env python3
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

import re
from msopgen.simulator import utils


class RegDumpParser:
    def __init__(self, file_path: str) -> None:
        self._file_path = file_path

    def get_start_pc(self) -> str:
        pattern = r"[Pp][Cc],[0-9, ]+val(?:ue)?[ =:]+(?:0x)?([0-9a-f]+)"
        first_pc = ""
        with open(self._file_path, "r") as fp:
            for line in fp:
                match = re.search(pattern, line.lower())
                if match:
                    first_pc = match.group(1)
                    break
        if not first_pc:
            utils.logger.warn(f"Can't parse first pc from {self._file_path}")
        return first_pc
