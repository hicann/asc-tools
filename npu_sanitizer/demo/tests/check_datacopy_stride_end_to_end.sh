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
output=$(mktemp)
trap 'status=$?; if [[ ${status} -ne 0 ]]; then cat "${output}" >&2; fi; rm -f -- "${output}"' EXIT

set +e
bash "${demo_dir}/run.sh" datacopy_stride >"${output}" 2>&1
run_status=$?
set -e

if [[ ${run_status} -ne 2 ]]; then
    printf 'expected npu_check exit 2, got %s\n' "${run_status}" >&2
    exit 1
fi

grep -F 'npu_check: handshake=ready tool=memcheck' "${output}"
grep -E '\[raw\] type=AclsanRawTraceRecord .*instrId=86 ' "${output}"
grep -E '\[param\] type=CopyGmToUbufAlignV2ParamField .*burstNum=3 burstLen=32 .*burstSrcStride=48' "${output}"
grep -F '[cbdata] layout=block_repeat blockNum=1 blockSize=32 blockStride=0 repeatTimes=3 repeatStride=48' \
    "${output}"
test "$(grep -Fc 'Invalid GM read of size 32 bytes' "${output}")" -eq 4
test "$(grep -Fc '16 bytes after the nearest allocation' "${output}")" -eq 4
test "$(grep -Ec '^=========       #3 DataCopyStrideSingleInput at ' "${output}")" -eq 1
test "$(grep -Ec '^=========       #3 DataCopyStrideDualInput at ' "${output}")" -eq 3
test "$(grep -Ec '^=========     by aicore .* type \(AIC\)' "${output}")" -eq 1
test "$(grep -Ec '^=========     by aicore .* type \(AIV\)' "${output}")" -eq 3
grep -F 'DataCopyStrideSingleInput' "${output}"
grep -F 'DataCopyStrideDualInput' "${output}"
grep -E 'npu_check: SUMMARY .*errors=4' "${output}"
grep -F 'npu_check: SESSION_END status=complete' "${output}"
grep -F 'npu_check: child_exit=0 handshake=ready session_end=complete' "${output}"
grep -F 'DataCopyStrideSingleInput completed' "${output}"
grep -F 'DataCopyStrideDualInput completed' "${output}"
