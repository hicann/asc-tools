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
grep -Fq 'foreach(probe_source mte1 mte2 mte3 fixpipe scalar sync)' "${demo_dir}/CMakeLists.txt"
grep -Fq '"${ACLSAN_DBI_SOURCE_DIRECTORY}/src/probes/${probe_source}.cpp"' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq '"${ACLSAN_DBI_RUNTIME_SOURCE_DIRECTORY}/probes/${probe_source}.cpp"' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq 'foreach(trace_header trace_record.h trace_buffer_abi.h)' "${demo_dir}/CMakeLists.txt"
grep -Fq '"${ACLSAN_DBI_SOURCE_DIRECTORY}/include/${trace_header}"' \
    "${demo_dir}/CMakeLists.txt"
grep -Fq '"${ACLSAN_DBI_RUNTIME_SOURCE_DIRECTORY}/${trace_header}"' \
    "${demo_dir}/CMakeLists.txt"

if grep -Eq 'CMAKE_CURRENT_SOURCE_DIR}/npu_check(_exec)?"' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo CMake still references a demo-local npu_check component\n' >&2
    exit 1
fi

grep -Fq 'cmake --build "${build_dir}" --target npu_check_cli "${example_target}" --parallel' \
    "${demo_dir}/run.sh"
grep -Fq 'default_cann_root="/usr/local/Ascend/cann/x86_64-linux"' "${demo_dir}/run.sh"
if grep -Fq '/home/cty' "${demo_dir}/run.sh" "${demo_dir}/README.md"; then
    printf 'demo runner or README still references /home/cty\n' >&2
    exit 1
fi
grep -Fq 'case "${asc_architecture}" in
    dav-3510)
        dbi_architecture="dav-c310"' "${demo_dir}/run.sh"
grep -Fq 'export NPU_CHECK_DBI_ARCH="${dbi_architecture}"' "${demo_dir}/run.sh"
if grep -Fq 'export NPU_CHECK_DBI_ARCH="${asc_architecture}"' "${demo_dir}/run.sh"; then
    printf 'demo runner passes the ASC architecture directly to the DBI compiler\n' >&2
    exit 1
fi
grep -Fq 'export NPU_CHECK_DBI_TOOLCHAIN_ROOT="${cann_install_dir}"' "${demo_dir}/run.sh"
grep -Fq 'export NPU_CHECK_DBI_SOURCE_ROOT="${build_dir}/dbi_runtime_sources"' "${demo_dir}/run.sh"
rg -U -Fq '"${bin_dir}/npu_check" --tool "${example_tool}" --strict --keep-temp \
    --work-dir "${build_dir}/probe_runtime" \
    --probe-cache-dir "${build_dir}/probe_cache" -- "${example_executable}"' "${demo_dir}/run.sh"

legacy_probe_prefix='ACLSAN_PROBE'
legacy_probe_resource_dir='probe_resources'
legacy_probe_build_option='ACLSAN_BUILD_DEVICE_PROBE'
if rg -n "${legacy_probe_prefix}_[A-Z_]+|${legacy_probe_resource_dir}/probe\\.o|${legacy_probe_build_option}_RESOURCES" \
    "${demo_dir}" -g '!build/**'; then
    printf 'demo still references legacy Probe build resources\n' >&2
    exit 1
fi

if grep -Eq -- '--target npucheck|/npucheck" --tool' "${demo_dir}/run.sh"; then
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
