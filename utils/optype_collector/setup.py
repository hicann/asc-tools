#!/usr/bin/python
# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

import os
import sys

from setuptools import find_packages

if "--inplace" in sys.argv:
    from distutils.core import setup
else:
    from setuptools import setup

VERSIONS = "0.1.0"

os.environ['SOURCE_DATE_EPOCH'] = str(int(os.path.getctime(os.path.realpath(__file__))))

setup(name="optype_collector",
      version=VERSIONS,
      description="optype_collector: collect and detect duplicated CANN OPP OpTypes",
      packages=find_packages(where=".", include=("*",)),
      entry_points={
          "console_scripts": [
              "optype_collector=optype_collector.optype_collector_main:main",
          ],
      },
)
