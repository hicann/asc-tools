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
bin_dir="${demo_dir}/build/npu_compute/bin"
output=$(mktemp)
trap 'status=$?; if [[ ${status} -ne 0 ]]; then cat "${output}" >&2; fi; rm -f -- "${output}"' EXIT

if grep -Fq 'NPU_COMPUTE_BUILD_INTEGRATION_STUBS' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo still depends on removed NPU_COMPUTE_BUILD_INTEGRATION_STUBS option\n' >&2
    exit 1
fi
grep -Fq 'set(NPU_COMPUTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)' "${demo_dir}/CMakeLists.txt"
grep -Fq -- '--target npu_check_cli "${example_target}"' "${demo_dir}/run.sh"

set +e
bash "${demo_dir}/run.sh" add >"${output}" 2>&1
run_status=$?
set -e
if [[ ${run_status} -ne 2 ]]; then
    printf 'expected npu_check exit status 2 for the intentional add OOB, got %d\n' "${run_status}" >&2
    exit 1
fi

grep -F 'npu_check: handshake=ready tool=memcheck' "${output}"
grep -F 'npu_check: DIAGNOSTIC' "${output}"
grep -E 'Invalid GM read of size 8256 bytes' "${output}"
grep -E 'npu_check: SUMMARY .*[[:space:]]errors=[1-9][0-9]*([[:space:]]|$)' "${output}"
grep -F 'npu_check: SESSION_END status=complete' "${output}"
grep -F 'npu_check: child_exit=0 handshake=ready session_end=complete' "${output}"
grep -F 'test pass!' "${output}"

test -x "${bin_dir}/npu_check"
test ! -e "${bin_dir}/npucheck"
test -f "${bin_dir}/libnpu_check.so"
test -f "${bin_dir}/libacl_tool_injection.so"
test -f "${bin_dir}/libacl_san.so"
test ! -e "${bin_dir}/libaclsan_demo_tool.so"
bash "${demo_dir}/../sanitizer_api/tests/check_acl_san_exports.sh" "${bin_dir}/libacl_san.so"
nm -D --defined-only "${bin_dir}/libnpu_check.so" | grep -F ' acltoolInitialize@@NPU_CHECK_1.0'
if nm -D --undefined-only "${bin_dir}/libnpu_check.so" |
    grep -Eq 'aclsan(SymbolizeDevicePc|GetPatchSiteInfo)'; then
    printf 'legacy SourceResolver symbol dependency remains\n' >&2
    exit 1
fi
if nm -D --defined-only "${bin_dir}/libnpu_check.so" | grep -Fq 'acltoolInitalize'; then
    printf 'misspelled injection entry is still exported\n' >&2
    exit 1
fi
readelf -d "${bin_dir}/aclsan_demo_add" | grep -F 'libacl_rt.so'
readelf -d "${bin_dir}/libnpu_check.so" | grep -F 'libacl_san.so'
readelf -d "${bin_dir}/libacl_san.so" | grep -F 'libacl_tool_injection.so'
readelf -d "${bin_dir}/libacl_tool_injection.so" | grep -F 'libacl_rt.so'
cann_root=$(sed -n 's/^NPUCOMPUTE_CANN_ROOT:PATH=//p' "${demo_dir}/build/CMakeCache.txt")
test -n "${cann_root}"
readelf -d "${cann_root}/lib64/libacl_rt.so" | grep -F 'libruntime.so'
if readelf -d "${bin_dir}/aclsan_demo_add" | grep -E 'libA\.so|libnpu_check\.so'; then
    printf 'legacy runtime dependency leaked into aclsan_demo_add\n' >&2
    exit 1
fi

test ! -e "${demo_dir}/npu_check"
test ! -e "${demo_dir}/npu_check_exec"
