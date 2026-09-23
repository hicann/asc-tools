#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

"""用途：隔离工具单测与调用者的 CANN、设备及 Python 注入配置。
使用方法：由 pytest 自动加载，仅影响当前测试及其子进程。
"""

import os

import pytest


@pytest.fixture(autouse=True)
def isolated_tool_environment(monkeypatch):
    for name in tuple(os.environ):
        if name.startswith("ASC_PRECISION_") or name in {
            "ASCEND_HOME_PATH",
            "ASCEND_TOOLKIT_HOME",
            "ASC_HOME_PATH",
            "ASCEND_DUMP_PATH",
            "ASCEND_WORK_PATH",
            "PYTHONPATH",
            "PYTHONHOME",
        }:
            monkeypatch.delenv(name, raising=False)
