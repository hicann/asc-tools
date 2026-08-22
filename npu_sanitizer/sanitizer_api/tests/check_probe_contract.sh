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

grep -Fq 'g_sanitizerOutput' "${api_dir}/probe/probe.asc"
grep -Fq '__sanitizer_report_copy_gm_to_ubuf_align_v2_b16' "${api_dir}/probe/probe.asc"
grep -Fq '__sanitizer_report_copy_ubuf_to_gm_align_v2' "${api_dir}/probe/probe.asc"
grep -Fq '__sanitizer_report_set_flag' "${api_dir}/probe/probe.asc"
grep -Fq '__sanitizer_report_wait_flag' "${api_dir}/probe/probe.asc"
grep -Fq 'GetCceInstructionPipeline(instructionId)' "${api_dir}/probe/probe_parser.cpp"
grep -Fq 'ACLSAN_DEVICE_PIPE_MTE2' "${api_dir}/src/cce_instr_types.cpp"
grep -Fq 'ACLSAN_DEVICE_PIPE_MTE3' "${api_dir}/src/cce_instr_types.cpp"
grep -Fq 'ACLSAN_DEVICE_PIPE_SCALAR' "${api_dir}/src/cce_instr_types.cpp"
grep -Fq 'InstrType::COPY_UBUF_TO_GM_ALIGN_V2' "${api_dir}/probe/gen_ctrlbin.cpp"
grep -Fq '_Z43__sanitizer_report_copy_ubuf_to_gm_align_v2PU3AS1hljPU3AS1vPU3AS6vmm.vector' \
    "${api_dir}/probe/symbol_ordering.txt"
grep -Fq 'DispatchProbeRecords(parseResult)' "${api_dir}/src/aclsan_hook_aclrt.cpp"
if grep -Rq 'MakeMockRawTraceRecords' "${api_dir}/src" "${api_dir}/include"; then
    printf 'mock raw trace producer is still present\n' >&2
    exit 1
fi
grep -Fq 'aclsan_probe_resources' "${api_dir}/CMakeLists.txt"
grep -Fq 'ACLSAN_PROBE_OBJECT' "${api_dir}/../demo/run.sh"

if rg -q '\bAclsanPatchPipeline\b|\bACLSAN_PATCH_PIPELINE_' \
    "${api_dir}/include" "${api_dir}/probe" "${api_dir}/src" "${api_dir}/tests" "${api_dir}/../npu_check" \
    -g '*.{h,cpp,asc}' -g '!check_probe_contract.sh'; then
    printf 'legacy patch pipeline name is still present\n' >&2
    exit 1
fi

printf 'probe contract=PASS\n'
