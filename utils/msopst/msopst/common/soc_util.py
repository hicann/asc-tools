#!/usr/bin/env python
# -*- coding:utf-8 -*-
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
soc utils
"""
from typing import List, Tuple, Union


def check_support_soc(supported_soc: Union[str, Tuple[str], List[str]], soc_version: str) -> bool:
    """
    check if soc_version is supported

    Parameters
    ----------
    supported_soc: Union[str, Tuple[str], List[str]]
        the supported soc versions
    soc_version: str
        soc_version which need to check if support

    Returns
    -------
    True or False
    """
    if isinstance(supported_soc, str):
        if supported_soc.lower() == "all":
            return True
        if supported_soc == soc_version:
            return True
        if all(soc in ("Ascend910", "Ascend910A") for soc in (supported_soc, soc_version)):
            return True
    if isinstance(supported_soc, (tuple, list)):
        if soc_version in supported_soc:
            return True
        if "all" in supported_soc:
            return True
        if soc_version in ("Ascend910", "Ascend910A") and \
            any(soc in ("Ascend910", "Ascend910A") for soc in supported_soc):
            return True
    return False