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
bash "${demo_dir}/run.sh" add_datacopy_stride_oob >"${output}" 2>&1
run_status=$?
set -e

if [[ ${run_status} -ne 2 ]]; then
    printf 'expected npu_check exit 2, got %s\n' "${run_status}" >&2
    exit 1
fi

grep -F 'npu_check: handshake=ready tool=memcheck' "${output}"
grep -E '\[raw\] type=AscsanRawTraceRecord .*instrId=86 ' "${output}"
grep -E '\[param\] type=CopyGmToUbufAlignV2ParamField .*burstNum=3 burstLen=32 .*burstSrcStride=48' "${output}"
grep -F '[cbdata] layout=block_repeat blockNum=3 blockSize=32 blockStride=48 repeatTimes=1 repeatStride=0' \
    "${output}"
grep -F 'Invalid GM read of size 32 bytes' "${output}"
grep -F '16 bytes after the nearest allocation' "${output}"
grep -E 'npu_check: SUMMARY .*errors=1' "${output}"
grep -F 'npu_check: SESSION_END status=complete' "${output}"
grep -F 'npu_check: child_exit=0 handshake=ready session_end=complete' "${output}"
grep -F 'non-contiguous DataCopy kernel completed' "${output}"
