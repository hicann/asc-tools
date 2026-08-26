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

api_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
pipeline_header="${api_dir}/include/aclsan/aclsan_cbdata_device.h"

grep -Fq 'typedef enum AclsanDevicePipeline {' "${pipeline_header}"
grep -Fq 'ACLSAN_DEVICE_PIPE_SCALAR = 1' "${pipeline_header}"
if [[ -e "${api_dir}/include/internal/aclsan_cbdata_device.h" ]]; then
    printf 'internal/aclsan_cbdata_device.h is still present\n' >&2
    exit 1
fi
grep -Fq '#include "aclsan/aclsan_cbdata_device.h"' \
    "${api_dir}/include/aclsan/aclsan_callback.h"
grep -Fq '#include "aclsan/aclsan_callback.h"' \
    "${api_dir}/../npu_check/src/checker/memcheck.h"

legacy_probe_resources=(
    probe
)
for resource in "${legacy_probe_resources[@]}"; do
    if [[ -e "${api_dir}/${resource}" ]]; then
        printf 'legacy Probe resource is still present: %s\n' "${resource}" >&2
        exit 1
    fi
done

dbi_probe_resources=(
    src/probes/mte1.cpp
    src/probes/mte2.cpp
    src/probes/mte3.cpp
    src/probes/fixpipe.cpp
    src/probes/sync.cpp
    include/trace_record.h
)
for resource in "${dbi_probe_resources[@]}"; do
    if [[ ! -f "${api_dir}/../dbi/${resource}" ]]; then
        printf 'required DBI probe resource is missing: %s\n' "${resource}" >&2
        exit 1
    fi
done

grep -Fq 'ACLSAN_DEVICE_PIPE_MTE2' "${api_dir}/src/cce_instr_types.cpp"
grep -Fq 'ACLSAN_DEVICE_PIPE_MTE3' "${api_dir}/src/cce_instr_types.cpp"
grep -Fq 'ACLSAN_DEVICE_PIPE_SCALAR' "${api_dir}/src/cce_instr_types.cpp"
if grep -Rq 'MakeMockRawTraceRecords' "${api_dir}/src" "${api_dir}/include"; then
    printf 'mock raw trace producer is still present\n' >&2
    exit 1
fi
if rg -q 'ACLSAN_PROBE_OBJECT|ACLSAN_BUILD_DEVICE_PROBE_RESOURCES|aclsan_probe_resources' \
    "${api_dir}/CMakeLists.txt" "${api_dir}/src"; then
    printf 'legacy Probe build reference is still present in sanitizer_api\n' >&2
    exit 1
fi

if rg -q '\bAclsanPatchPipeline\b|\bACLSAN_PATCH_PIPELINE_' \
    "${api_dir}/include" "${api_dir}/src" "${api_dir}/tests" "${api_dir}/../npu_check" \
    -g '*.{h,cpp,asc}' -g '!check_probe_contract.sh'; then
    printf 'legacy patch pipeline name is still present\n' >&2
    exit 1
fi

printf 'probe contract=PASS\n'
