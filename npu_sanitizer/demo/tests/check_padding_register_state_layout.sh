#!/usr/bin/env bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
example_dir="${demo_dir}/examples/padding_register_state"

test -f "${example_dir}/padding_register_state.asc"
test -f "${example_dir}/CMakeLists.txt"
test -x "${demo_dir}/tests/check_padding_register_state_end_to_end.sh"

grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/padding_register_state"' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq 'foreach(probe_source mte1 mte2 mte3 fixpipe register sync)' "${demo_dir}/CMakeLists.txt"
grep -Fq 'add_executable(aclsan_demo_padding_register_state padding_register_state.asc)' \
    "${example_dir}/CMakeLists.txt"
grep -Fq 'uint64_t reserved, uint64_t firstValue, uint64_t secondValue' \
    "${example_dir}/padding_register_state.asc"
grep -Fq 'asc_set_l13d_padding(firstValue);' "${example_dir}/padding_register_state.asc"
grep -Fq 'asc_set_l13d_padding(secondValue);' "${example_dir}/padding_register_state.asc"
grep -Fq 'PaddingRegisterStateKernel<<<1, 0, stream>>>(0, FIRST_PADDING_INPUT, SECOND_PADDING_INPUT);' \
    "${example_dir}/padding_register_state.asc"
grep -Fq 'padding_register_state)' "${demo_dir}/run.sh"
grep -Fq 'example_target="aclsan_demo_padding_register_state"' "${demo_dir}/run.sh"
grep -Fq 'padding_register_state' "${demo_dir}/README.md"
grep -Fq 'padding_register_state' "${demo_dir}/README_en.md"

bash -n "${demo_dir}/run.sh"
bash -n "${demo_dir}/tests/check_padding_register_state_end_to_end.sh"
