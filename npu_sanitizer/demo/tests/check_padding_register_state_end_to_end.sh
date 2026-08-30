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
bash "${demo_dir}/examples/basic_func/padding_register_state/run.sh" >"${output}" 2>&1
run_status=$?
set -e
if [[ ${run_status} -ne 0 ]]; then
    printf 'expected padding register state demo exit 0, got %s\n' "${run_status}" >&2
    exit 1
fi

grep -F 'npu_check: handshake=ready tool=memcheck' "${output}"
grep -E '\[raw\] type=AclsanRawTraceRecord .*instrId=392 .*args=\[0x12,' "${output}"
grep -E '\[raw\] type=AclsanRawTraceRecord .*instrId=392 .*args=\[0x34,' "${output}"
grep -F '[param] type=SetPaddingParamField value=0x12' "${output}"
grep -F '[param] type=SetPaddingParamField value=0x34' "${output}"

first_update=$(grep -m 1 -E \
    '\[register\] action=update register=set_padding .*value=0x12' "${output}")
second_update=$(grep -m 1 -E \
    '\[register\] action=update register=set_padding .*value=0x34' "${output}")
first_key=$(sed -E 's/.*launchId=([0-9]+) blockType=([0-9]+) blockId=([0-9]+).*/\1 \2 \3/' <<<"${first_update}")
second_key=$(sed -E 's/.*launchId=([0-9]+) blockType=([0-9]+) blockId=([0-9]+).*/\1 \2 \3/' <<<"${second_update}")
if [[ "${first_key}" != "${second_key}" ]]; then
    printf 'padding updates used different register-state keys: %s != %s\n' "${first_key}" "${second_key}" >&2
    exit 1
fi

grep -F 'padding register state demo pass!' "${output}"
grep -E 'npu_check: SUMMARY .*errors=0([[:space:]]|$)' "${output}"
grep -F 'npu_check: SESSION_END status=complete' "${output}"
grep -F 'npu_check: child_exit=0 handshake=ready session_end=complete' "${output}"
if grep -E 'unsupported raw trace instrId=392' "${output}"; then
    printf 'SET_PADDING was treated as an unsupported callback record\n' >&2
    exit 1
fi
grep -F '[PASSED] basic_func/padding_register_state' "${output}"

mapfile -t probe_objects < <(find "${demo_dir}/build/probe_cache" -mindepth 2 -maxdepth 2 -type f -name probe.o)
if [[ ${#probe_objects[@]} -ne 1 ]]; then
    printf 'expected exactly one cached probe.o, got %d\n' "${#probe_objects[@]}" >&2
    exit 1
fi
cann_home=$(sed -n 's/^ASCEND_HOME_PATH:PATH=//p' "${demo_dir}/build/CMakeCache.txt")
llvm_objdump="${cann_home}/tools/bisheng_compiler/bin/llvm-objdump"
"${llvm_objdump}" --syms "${probe_objects[0]}" | grep -F '__sanitizer_report_set_padding'
