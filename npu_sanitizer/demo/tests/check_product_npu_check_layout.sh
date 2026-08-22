#!/usr/bin/env bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
sanitizer_dir=$(cd "${demo_dir}/.." && pwd)

grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" npu_check_common)' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../npu_check_cli" npu_check_cli)' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../npu_check" npu_check)' \
    "${demo_dir}/CMakeLists.txt"

if grep -Eq 'CMAKE_CURRENT_SOURCE_DIR}/npu_check(_exec)?"' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo CMake still references a demo-local npu_check component\n' >&2
    exit 1
fi

grep -Fq 'cmake --build "${build_dir}" --target npu_check_cli "${example_target}" --parallel' \
    "${demo_dir}/run.sh"
grep -Fq '"${bin_dir}/npu_check" --tool memcheck -- "${example_executable}"' "${demo_dir}/run.sh"

if grep -Eq -- '--target npucheck|/npucheck" --tool|--tool synccheck' "${demo_dir}/run.sh"; then
    printf 'demo runner still references the legacy launcher or unsupported tool\n' >&2
    exit 1
fi

grep -Fq 'NPU_CHECK_API int acltoolInitialize(void);' "${sanitizer_dir}/npu_check/include/npu_check.h"
grep -Fq 'NPU_CHECK_API int acltoolInitialize(void)' "${sanitizer_dir}/npu_check/src/tool_manager/entry.cpp"
grep -Fq 'acltoolInitialize;' "${sanitizer_dir}/npu_check/cmake/npu_check.map"

if rg -n 'acltoolInitalize' "${sanitizer_dir}/npu_check"; then
    printf 'product npu_check still contains the misspelled injection entry\n' >&2
    exit 1
fi

printf 'product npu_check demo layout check passed\n'
