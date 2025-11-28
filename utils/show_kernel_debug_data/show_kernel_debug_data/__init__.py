#!/usr/bin/python
# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

import sys
import os
from show_kernel_debug_data.dump_parser import parse_dump_bin


def show_kernel_debug_data(bin_file_path: str, output_path: str = './'):
    if not bin_file_path or not os.path.isfile(bin_file_path) or not os.path.exists(bin_file_path):
        raise RuntimeError(f'({bin_file_path}) file does not exist or permission denied!!!')

    if not output_path or not os.path.isdir(output_path) or not os.path.exists(output_path):
        raise RuntimeError(f'({output_path}) directory does not exist or permission denied!!!')
    
    parse_dump_bin(bin_file_path, output_path)