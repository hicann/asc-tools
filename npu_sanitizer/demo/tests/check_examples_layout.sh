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
output=$(mktemp)
trap 'rm -f -- "${output}"' EXIT

test -f "${demo_dir}/examples/add/add.asc"
test -f "${demo_dir}/examples/datacopy_stride/datacopy_stride.asc"
test -f "${demo_dir}/examples/datacopy_stride/CMakeLists.txt"
test -x "${demo_dir}/tests/check_datacopy_stride_end_to_end.sh"
test ! -e "${demo_dir}/examples/test"
test -x "${demo_dir}/examples/matmul_basic_api/run.sh"
test -x "${demo_dir}/examples/matmul_leakyrelu_basic_api/run.sh"
grep -Fq 'matmul_basic_api)' "${demo_dir}/run.sh"
grep -Fq 'matmul_leakyrelu_basic_api)' "${demo_dir}/run.sh"
grep -Fq 'aclsan_demo_matmul_basic_api' "${demo_dir}/run.sh"
grep -Fq 'aclsan_demo_matmul_leakyrelu_basic_api' "${demo_dir}/run.sh"
grep -Fq 'datacopy_stride)' "${demo_dir}/run.sh"
grep -Fq 'aclsan_demo_datacopy_stride' "${demo_dir}/run.sh"
grep -Fq 'scripts/verify_result.py' "${demo_dir}/run.sh"
for example in matmul_basic_api matmul_leakyrelu_basic_api; do
    grep -Fq "add_executable(aclsan_demo_${example}" "${demo_dir}/examples/${example}/CMakeLists.txt"
    grep -Fq 'target_link_libraries' "${demo_dir}/examples/${example}/CMakeLists.txt"
    grep -Fq 'lib64' "${demo_dir}/examples/${example}/CMakeLists.txt"
    grep -Fq -- "--target aclsan_demo_${example}" "${demo_dir}/examples/${example}/run.sh"
    grep -Fq 'scripts/gen_data.py' "${demo_dir}/examples/${example}/run.sh"
    if grep -Eq 'source|set_env\.sh|NPUCOMPUTE_CANN_ROOT|default_cann_root' \
        "${demo_dir}/examples/${example}/run.sh"; then
        printf '%s/run.sh still configures the CANN environment\n' "${example}" >&2
        exit 1
    fi
    if [[ $(wc -l <"${demo_dir}/examples/${example}/run.sh") -gt 16 ]]; then
        printf '%s/run.sh is not minimal\n' "${example}" >&2
        exit 1
    fi
    bash -n "${demo_dir}/examples/${example}/run.sh"
done
if grep -Fq 'examples/test' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo CMake still references the removed examples/test directory\n' >&2
    exit 1
fi
grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/add" examples_add)' "${demo_dir}/CMakeLists.txt"
grep -Fq \
    'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/datacopy_stride" examples_datacopy_stride)' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq 'add_executable(aclsan_demo_datacopy_stride' \
    "${demo_dir}/examples/datacopy_stride/CMakeLists.txt"
datacopy_stride_source="${demo_dir}/examples/datacopy_stride/datacopy_stride.asc"
grep -Fq '#define ACL_CHECK' "${datacopy_stride_source}"
grep -Fq 'ACL_CHECK(aclInit(nullptr));' "${datacopy_stride_source}"
if grep -Eq 'RuntimeResources|InitializeRuntimeResources|ReleaseRuntimeResources' "${datacopy_stride_source}"; then
    printf 'datacopy_stride still hides ACL Runtime calls behind resource helpers\n' >&2
    exit 1
fi
grep -Fq 'example_name=${1:-add}' "${demo_dir}/run.sh"
grep -Fq 'case "${example_name}" in' "${demo_dir}/run.sh"
grep -Fq 'add)' "${demo_dir}/run.sh"
if grep -Eq 'examples/test|aclsan_demo_test|test\)' "${demo_dir}/run.sh"; then
    printf 'run.sh still exposes the removed test example\n' >&2
    exit 1
fi
grep -Fq 'set +u' "${demo_dir}/run.sh"
grep -Fq 'set -u' "${demo_dir}/run.sh"
bash -n "${demo_dir}/run.sh"

if bash "${demo_dir}/run.sh" invalid >"${output}" 2>&1; then
    printf 'run.sh accepted an unsupported example\n' >&2
    exit 1
fi
grep -Fq 'unsupported example: invalid' "${output}"
