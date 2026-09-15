# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
"""Generate the fixed control record and embed build artifacts as C++ arrays."""

import pathlib
import struct
import sys

probe, control, header = map(pathlib.Path, sys.argv[1:])
name = b"__npu_compute_before_kernel_end"
binding = struct.pack("<HHH", 397, 0, 0)
names = struct.pack("<I", len(name) + 4) + name + bytes(4)
record = (
    struct.pack("<IHHHH", 12 + len(binding) + len(names), 0, len(binding), 0, 0)
    + binding
    + names
)
control.write_bytes(record)
parts = [
    "// Generated file. Do not edit.\n#pragma once\nnamespace aclpti::profiling {\n"
]
for symbol, data in (
    ("kKernelEndObject", probe.read_bytes()),
    ("kKernelEndControl", record),
):
    parts.append("inline constexpr unsigned char " + symbol + "[] = {\n")
    for offset in range(0, len(data), 16):
        parts.append(
            ",".join(f"0x{byte:02x}" for byte in data[offset : offset + 16]) + ",\n"
        )
    parts.append("};\n")
parts.append("}\n")
header.write_text("".join(parts))
