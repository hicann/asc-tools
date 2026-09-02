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
npu_tools_dir=$(cd "${sanitizer_dir}/.." && pwd)

test ! -e "${demo_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory(injection)' "${npu_tools_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory(common)' "${sanitizer_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory(sanitizer_api)' "${sanitizer_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory(npu_check_cli)' "${sanitizer_dir}/CMakeLists.txt"
grep -Fq 'add_subdirectory(npu_check)' "${sanitizer_dir}/CMakeLists.txt"
test ! -e "${sanitizer_dir}/dbi"
test -d "${sanitizer_dir}/sanitizer_api/src/dbi"
test -d "${sanitizer_dir}/sanitizer_api/include/dbi"
if ! grep -Fq '"LINKER:-rpath,$ORIGIN"' "${sanitizer_dir}/sanitizer_api/CMakeLists.txt"; then
    printf 'libacl_san.so does not explicitly preserve its $ORIGIN RUNPATH\n' >&2
    exit 1
fi
if rg -n 'DBI_RUNTIME_SOURCE|src/probes/.*\.cpp|NPU_CHECK_DBI_SOURCE_ROOT' \
    "${sanitizer_dir}/CMakeLists.txt"; then
    printf 'product npu_sanitizer still stages or exports runtime Probe sources\n' >&2
    exit 1
fi

if rg -n 'install\(DIRECTORY.*probes|share/aclsan/dbi|trace_record\.h|trace_buffer_abi\.h' \
    "${sanitizer_dir}/npu_check/CMakeLists.txt"; then
    printf 'product npu_check still installs DBI Probe sources or private ABI headers\n' >&2
    exit 1
fi

internal_arch='dav'"-c310"
if rg -n "${internal_arch}" "${sanitizer_dir}"; then
    printf 'npu_sanitizer still contains the internal architecture name\n' >&2
    exit 1
fi
legacy_probe_prefix='ACLSAN_PROBE'
legacy_probe_resource_dir='probe_resources'
legacy_probe_build_option='ACLSAN_BUILD_DEVICE_PROBE'
if rg -n "${legacy_probe_prefix}_[A-Z_]+|${legacy_probe_resource_dir}/probe\\.o|${legacy_probe_build_option}_RESOURCES" \
    "${demo_dir}" -g '!build/**'; then
    printf 'demo still references legacy Probe build resources\n' >&2
    exit 1
fi

grep -Fq 'NPU_CHECK_API int acltoolInitialize(void);' "${sanitizer_dir}/npu_check/include/npu_check.h"
grep -Fq 'NPU_CHECK_API int acltoolInitialize(void)' "${sanitizer_dir}/npu_check/src/tool_manager/entry.cpp"
grep -Fq 'acltoolInitialize;' "${sanitizer_dir}/npu_check/cmake/npu_check.map"

if rg -n 'acltoolInitalize' "${sanitizer_dir}/npu_check"; then
    printf 'product npu_check still contains the misspelled injection entry\n' >&2
    exit 1
fi

if rg -n 'add_executable\(gen_ctrlbin|install\(TARGETS gen_ctrlbin|TARGET_FILE:gen_ctrlbin|generate_ctrlbin_test\.cmake' \
    "${sanitizer_dir}/npu_check"; then
    printf 'product npu_check still exposes the obsolete gen_ctrlbin executable\n' >&2
    exit 1
fi

if [[ -e "${sanitizer_dir}/sanitizer_api/tests/dbi/generate_ctrlbin_test.cmake" ]]; then
    printf 'obsolete gen_ctrlbin executable test script still exists\n' >&2
    exit 1
fi

printf 'product npu_check demo layout check passed\n'
