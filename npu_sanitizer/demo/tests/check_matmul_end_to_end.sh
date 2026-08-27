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
trap 'status=$?; if [[ ${status} -ne 0 ]]; then cat "${output}" >&2; fi; rm -f -- "${output}"' EXIT

RunAndCheck()
{
    local example_name=$1
    local expected_records=$2

    bash "${demo_dir}/run.sh" "${example_name}" >"${output}" 2>&1
    grep -F "[probe] readback records=${expected_records}" "${output}"
    grep -F "[probe] records=PASS count=${expected_records}" "${output}"
    grep -E 'npu_check: SUMMARY .*errors=0' "${output}"
    grep -F 'npu_check: child_exit=0 handshake=ready session_end=complete' "${output}"
    grep -F 'test pass!' "${output}"
    if grep -Fq 'unsupported raw trace' "${output}"; then
        printf 'unsupported probe record in %s\n' "${example_name}" >&2
        return 1
    fi
}

RunAndCheck matmul_basic_api 14
RunAndCheck matmul_leakyrelu_basic_api 38
grep -F '[AIV Block 3/4]' "${output}"
grep -F 'instrId=456' "${output}"
grep -F 'instrId=458' "${output}"
