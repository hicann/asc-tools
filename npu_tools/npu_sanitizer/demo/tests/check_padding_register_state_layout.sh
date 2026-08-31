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
example_dir="${demo_dir}/examples/basic_func/padding_register_state"

test -f "${example_dir}/padding_register_state.asc"
test -f "${example_dir}/CMakeLists.txt"
test -x "${demo_dir}/tests/check_padding_register_state_end_to_end.sh"

if grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_func/padding_register_state"' \
    "${demo_dir}/CMakeLists.txt"; then
    printf 'padding_register_state is still registered in the shared demo build\n' >&2
    exit 1
fi
grep -Fq 'foreach(probe_source mte1 mte2 mte3 fixpipe scalar sync)' "${demo_dir}/CMakeLists.txt"
grep -Fq 'cmake_minimum_required(VERSION 3.16)' "${example_dir}/CMakeLists.txt"
grep -Fq 'find_package(ASC REQUIRED)' "${example_dir}/CMakeLists.txt"
grep -Fq 'find_library(ACL_RT_LIBRARY' "${example_dir}/CMakeLists.txt"
grep -Fq 'add_executable(demo' \
    "${example_dir}/CMakeLists.txt"
grep -Fq 'target_link_libraries(demo PRIVATE' "${example_dir}/CMakeLists.txt"
grep -Fq '${ACL_RT_LIBRARY}' "${example_dir}/CMakeLists.txt"
if rg -q 'ACLSAN_DEMO_ACL_RT_DIRECTORY|RUNTIME_OUTPUT_DIRECTORY|BUILD_RPATH' \
    "${example_dir}/CMakeLists.txt"; then
    printf 'padding_register_state still overrides its output or build RPATH\n' >&2
    exit 1
fi
if rg -q 'source .*common\.sh|aclsan_(prepare|capture|expect|require|verify|complete)' \
    "${example_dir}/run.sh"; then
    printf 'padding_register_state runner still depends on common helper functions\n' >&2
    exit 1
fi
grep -Fq 'cd "$(dirname "$0")"' "${example_dir}/run.sh"
grep -Fq 'output="build/npu_check.log"' "${example_dir}/run.sh"
grep -Fq 'exec > >(tee -a "${output}") 2>&1' "${example_dir}/run.sh"
grep -Fq 'cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510' "${example_dir}/run.sh"
grep -Fq '"${npu_check}" --tool memcheck -- build/demo' \
    "${example_dir}/run.sh"
grep -Fq 'uint64_t reserved, uint64_t firstValue, uint64_t secondValue' \
    "${example_dir}/padding_register_state.asc"
grep -Fq 'asc_set_l13d_padding(firstValue);' "${example_dir}/padding_register_state.asc"
grep -Fq 'asc_set_l13d_padding(secondValue);' "${example_dir}/padding_register_state.asc"
grep -Fq 'PaddingRegisterStateKernel<<<1, 0, stream>>>(0, FIRST_PADDING_INPUT, SECOND_PADDING_INPUT);' \
    "${example_dir}/padding_register_state.asc"
grep -Fq 'padding_register_state' "${demo_dir}/README.md"
grep -Fq 'padding_register_state' "${demo_dir}/README_en.md"

bash -n "${example_dir}/run.sh"
bash -n "${demo_dir}/tests/check_padding_register_state_end_to_end.sh"
