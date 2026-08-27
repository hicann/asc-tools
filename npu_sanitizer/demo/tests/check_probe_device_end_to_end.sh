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

set +e
bash "${demo_dir}/run.sh" add >"${output}" 2>&1
run_status=$?
set -e
if [[ ${run_status} -ne 2 ]]; then
    printf 'expected npu_check exit status 2 for the intentional add OOB, got %d\n' "${run_status}" >&2
    exit 1
fi

grep -m 1 -E '\[raw\] type=AscsanRawTraceRecord .*instrId=86 .*args=' "${output}"
grep -m 1 -E '\[param\] type=CopyGmToUbufAlignV2ParamField .*burstLen=8256 ' "${output}"
grep -m 1 -E '\[cbdata\] type=AclsanDeviceMemoryAccessData .*bytes=8256 ' "${output}"
grep -E 'npu_check: SUMMARY .*[[:space:]]device_operations=[1-9][0-9]*([[:space:]]|$)' "${output}"
grep -F 'test pass!' "${output}"
grep -F 'npu_check: DIAGNOSTIC' "${output}"
grep -E 'Invalid GM read of size 8256 bytes' "${output}"
grep -E 'npu_check: SUMMARY .*[[:space:]]errors=[1-9][0-9]*([[:space:]]|$)' "${output}"
grep -F 'npu_check: child_exit=0 handshake=ready session_end=complete' "${output}"

mapfile -t probe_objects < <(find "${demo_dir}/build/probe_cache" -mindepth 2 -maxdepth 2 -type f -name probe.o)
if [[ ${#probe_objects[@]} -ne 1 ]]; then
    printf 'expected exactly one cached probe.o, got %d\n' "${#probe_objects[@]}" >&2
    exit 1
fi
cann_root=$(sed -n 's/^NPUCOMPUTE_CANN_ROOT:PATH=//p' "${demo_dir}/build/CMakeCache.txt")
llvm_objdump=$(cd "${cann_root}/.." && pwd)/tools/bisheng_compiler/bin/llvm-objdump
probe_symbols=$("${llvm_objdump}" --syms "${probe_objects[0]}")
for expected_symbol in \
    __sanitizer_report_copy_cbuf_to_ubuf \
    __sanitizer_report_copy_gm_to_ubuf_align_v2 \
    __sanitizer_report_copy_ubuf_to_gm_align_v2 \
    __sanitizer_report_copy_cbuf_to_fbuf; do
    grep -Fq "${expected_symbol}" <<<"${probe_symbols}"
done
if grep -Eq '__sanitizer_report_(set|wait)_flag' <<<"${probe_symbols}"; then
    printf 'memcheck probe.o unexpectedly contains sync probes\n' >&2
    exit 1
fi
