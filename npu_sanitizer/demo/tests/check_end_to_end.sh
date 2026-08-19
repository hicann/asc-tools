#!/usr/bin/env bash

set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
bin_dir="${demo_dir}/build/npu_compute/bin"
output=$(mktemp)
trap 'rm -f -- "${output}"' EXIT

bash "${demo_dir}/run.sh" >"${output}" 2>&1

grep -F '[npucheck] configured tools=memcheck,synccheck' "${output}"
grep -F '[CALLBACK memcheck:aclrtMalloc]' "${output}"
grep -F '[CALLBACK memcheck:DataCopy]' "${output}"
grep -F '[CALLBACK synccheck:DeviceSync]' "${output}"
grep -F '[CALLBACK synccheck:StreamSyncEnd]' "${output}"
grep -F '[npucheck] child exited status=0' "${output}"
grep -F '[aclsan-demo-app] completed' "${output}"

test -x "${bin_dir}/npucheck"
test -f "${bin_dir}/libnpu_check.so"
test -f "${bin_dir}/libacl_tool_injection.so"
test -f "${bin_dir}/libruntime.so"
test -f "${bin_dir}/libacl_san.so"
test ! -e "${bin_dir}/libaclsan_demo_tool.so"
readelf -d "${bin_dir}/aclsan_demo_app" | grep -F 'libruntime.so'
readelf -d "${bin_dir}/libnpu_check.so" | grep -F 'libacl_san.so'
readelf -d "${bin_dir}/libacl_san.so" | grep -F 'libacl_tool_injection.so'
readelf -d "${bin_dir}/libacl_tool_injection.so" | grep -F 'libruntime.so'
if readelf -d "${bin_dir}/aclsan_demo_app" | grep -E 'libA\.so|libnpu_check\.so'; then
    printf 'legacy runtime dependency leaked into aclsan_demo_app\n' >&2
    exit 1
fi
