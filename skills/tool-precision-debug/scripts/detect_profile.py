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

"""用途：探测本机可见的 CANN、SoC、NPU 架构与编程模型线索，并如实保留未知字段。
使用方法（在 Skill 根目录执行）：
    python3 scripts/detect_profile.py --help
"""

import argparse
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from platform_profile import detect_profile  # noqa: E402 - initialize standalone CLI first


def build_parser() -> argparse.ArgumentParser:
    return argparse.ArgumentParser(
        description="Detect the local CANN, SoC, NPU architecture and programming model profile"
    )


def main() -> int:
    build_parser().parse_args()
    print(json.dumps(detect_profile(), ensure_ascii=False, sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
