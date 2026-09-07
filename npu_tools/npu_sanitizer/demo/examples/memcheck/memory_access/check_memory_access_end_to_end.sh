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

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
demo_dir=$(cd "${script_dir}/../../.." && pwd)
example_dir="${script_dir}"
current_output=""
build_prepared=false
all_targets_built=false

cases=(
    gm_to_ub_read_valid
    gm_to_ub_read_oob
    gm_to_ub_b16_valid
    gm_to_ub_b16_oob
    gm_to_ub_b32_valid
    gm_to_ub_b32_oob
    gm_to_ub_block_repeat_valid
    gm_to_ub_block_repeat_oob
    ub_to_gm_write_valid
    ub_to_gm_write_oob
    local_ub_to_l1_valid
    gm_to_ub_outer_loop_valid
    gm_to_ub_outer_loop_oob
    ub_to_gm_outer_loop_valid
    ub_to_gm_outer_loop_oob
    cube_gm_to_l1_outer_loop_valid
    cube_gm_to_l1_outer_loop_oob
    cube_gm_to_l1_id73_outer_loop_valid
    cube_gm_to_l1_id73_outer_loop_oob
    cube_gm_to_l1_id75_outer_loop_valid
    cube_gm_to_l1_id75_outer_loop_oob
    cube_gm_to_l1_id76_outer_loop_valid
    cube_gm_to_l1_id76_outer_loop_oob
    cube_gm_to_l1_id73_mode0_valid
    cube_gm_to_l1_id73_mode0_oob
    cube_gm_to_l1_id73_mode1_valid
    cube_gm_to_l1_id73_mode1_oob
    cube_gm_to_l1_id73_mode2_valid
    cube_gm_to_l1_id73_mode2_oob
    cube_gm_to_l1_id73_mode3_valid
    cube_gm_to_l1_id73_mode3_oob
    cube_gm_to_l1_id73_mode4_valid
    cube_gm_to_l1_id73_mode4_oob
    cube_gm_to_l1_id73_mode5_valid
    cube_gm_to_l1_id73_mode5_oob
    cube_gm_to_l1_id73_mode6_valid
    cube_gm_to_l1_id73_mode6_oob
    cube_gm_to_l1_id73_mode7_valid
    cube_gm_to_l1_id73_mode7_oob
    cube_gm_to_l1_id73_mode8_valid
    cube_gm_to_l1_id73_mode8_oob
    cube_gm_to_l1_id74_valid
    cube_gm_to_l1_id74_oob
    cube_gm_to_l1_id75_valid
    cube_gm_to_l1_id75_oob
    cube_gm_to_l1_id76_valid
    cube_gm_to_l1_id76_oob
    multi_nd2nz_valid
    multi_nd2nz_oob
    multi_nd2nz_b8_valid
    multi_nd2nz_b8_oob
    multi_nd2nz_b32_valid
    multi_nd2nz_b32_oob
    multi_dn2nz_valid
    multi_dn2nz_oob
    multi_dn2nz_b8_valid
    multi_dn2nz_b8_oob
    multi_dn2nz_b32_valid
    multi_dn2nz_b32_oob
    multi_dn2nz_matrix2_valid
    multi_dn2nz_matrix2_oob
    multi_dn2nz_b8_matrix2_valid
    multi_dn2nz_b8_matrix2_oob
    multi_dn2nz_b32_matrix2_valid
    multi_dn2nz_b32_matrix2_oob
    fixpipe_valid
    fixpipe_oob
    fixpipe_s32_valid
    fixpipe_s32_oob
    fixpipe_nz2nd_valid
    fixpipe_nz2nd_oob
    fixpipe_nz2dn_valid
    fixpipe_nz2dn_oob
    fixpipe_s32_nz2nd_valid
    fixpipe_s32_nz2nd_oob
    fixpipe_s32_nz2dn_valid
    fixpipe_s32_nz2dn_oob
    fixpipe_channel_split_n8_valid
    fixpipe_channel_split_n8_oob
    fixpipe_channel_split_n16_valid
    fixpipe_channel_split_n16_oob
    fixpipe_c0pad_exact_valid
    fixpipe_c0pad_exact_oob
    fixpipe_c0pad_roundup_valid
    fixpipe_c0pad_roundup_oob
    nddma_b8_valid
    nddma_b8_oob
    nddma_b16_valid
    nddma_b16_oob
    nddma_b32_valid
    nddma_b32_oob
    nddma_rank1_b8_valid
    nddma_rank1_b8_oob
    nddma_rank2_singleton_b16_valid
    nddma_rank2_singleton_b16_oob
    nddma_rank3_singleton_b32_valid
    nddma_rank3_singleton_b32_oob
    nddma_rank4_singleton_b8_valid
    nddma_rank4_singleton_b8_oob
    nddma_missing_active_stride
    nddma_padding_nearest_b8_valid
    nddma_padding_nearest_b8_oob
    nddma_padding_nearest_b16_valid
    nddma_padding_nearest_b16_oob
    nddma_padding_nearest_b32_valid
    nddma_padding_nearest_b32_oob
    nddma_padding_constant_nonzero_b8_valid
    nddma_padding_constant_nonzero_b8_oob
    nddma_padding_uniform_constant_b16_valid
    nddma_padding_uniform_constant_b16_oob
    nddma_padding_max_loop0_padding_b8_valid
    nddma_padding_max_loop0_padding_b8_oob
    nddma_p0_broadcast_b8_valid
    nddma_p0_broadcast_b8_oob
    nddma_p0_b64_4d_padding_valid
    nddma_p0_b64_4d_padding_oob
    nddma_p0_b64_5d_padding_valid
    nddma_p0_b64_5d_padding_oob
    nddma_p0_b64_5d_singleton_valid
    nddma_p0_b64_5d_singleton_oob
    nddma_p0_multi_block_b8_valid
    nddma_p0_multi_block_b8_oob
    load2dv2_mode0_valid
    load2dv2_mode0_oob
    load2dv2_negative_stride_valid
    load2dv2_negative_stride_oob
    api_datacopypad_gm2ub_normal_valid
    api_datacopypad_gm2ub_normal_oob
    api_datacopypad_gm2ub_compact_valid
    api_datacopypad_gm2ub_compact_oob
    api_datacopypad_ub2gm_normal_valid
    api_datacopypad_ub2gm_normal_oob
    api_datacopypad_ub2gm_compact_valid
    api_datacopypad_ub2gm_compact_oob
    api_datacopy_nd2nz_valid
    api_datacopy_nd2nz_oob
    api_datacopy_nz2nd_valid
    api_datacopy_nz2nd_oob
    api_load_data_2d_valid
    api_load_data_2d_oob
    api_load_data_2dv2_valid
    api_load_data_2dv2_oob
    api_datacopypad_gm2l1_normal_valid
    api_datacopypad_gm2l1_normal_oob
    api_datacopypad_gm2l1_compact_valid
    api_datacopypad_gm2l1_compact_oob
    api_nddma_b64_4d_valid
    api_nddma_b64_4d_oob
    api_nddma_b64_5d_valid
    api_nddma_b64_5d_oob
)

readonly fixpipe_quant_cases=(
    q0_f32 q0_s32 q1 q2 q3 q4 q5 q8 q9 q10 q11 q12 q13 q14 q15 q16
    q21 q22 q23 q24 q25 q26 q31 q32 q33 q34 q35 q36 channel_merge_tail b4_channel_merge_two_group
    b8_f32_nz2nd b8_s32_nz2dn b4_f32_nz2nd b4_s32_nz2dn
    relu_pre relu_scalar relu_vector unit_keep unit_update clip_relu_pre
)
for fixpipe_quant_case in "${fixpipe_quant_cases[@]}"; do
    cases+=("fixpipe_quant_${fixpipe_quant_case}_valid" "fixpipe_quant_${fixpipe_quant_case}_oob")
done
readonly cases

readonly smoke_cases=(
    gm_to_ub_read_oob
    ub_to_gm_write_oob
    cube_gm_to_l1_id73_mode8_oob
    cube_gm_to_l1_id74_oob
    multi_dn2nz_b8_matrix2_oob
    fixpipe_channel_split_n16_oob
    fixpipe_quant_b4_channel_merge_two_group_oob
    fixpipe_quant_relu_vector_valid
    nddma_b32_oob
    nddma_missing_active_stride
    nddma_padding_constant_nonzero_b8_oob
    nddma_p0_b64_5d_padding_oob
    load2dv2_mode0_oob
    load2dv2_negative_stride_valid
    api_datacopy_nd2nz_oob
    api_datacopy_nz2nd_oob
    api_load_data_2dv2_oob
    api_nddma_b64_5d_oob
)

Cleanup()
{
    if [[ -n "${current_output}" ]]; then
        rm -f -- "${current_output}"
        current_output=""
    fi
}
trap Cleanup EXIT

Usage()
{
    printf 'usage: %s <case|--list|smoke|all>\n' "${BASH_SOURCE[0]}" >&2
}

ListCases()
{
    printf '%s\n' "${cases[@]}"
}

IsKnownCase()
{
    local requested_case=$1
    local case_name
    for case_name in "${cases[@]}"; do
        if [[ "${case_name}" == "${requested_case}" ]]; then
            return 0
        fi
    done
    return 1
}

RequirePattern()
{
    local pattern=$1
    local description=$2
    local output=$3
    if ! grep -Eq "${pattern}" "${output}"; then
        printf 'missing %s\n' "${description}" >&2
        return 1
    fi
}

RejectPattern()
{
    local pattern=$1
    local description=$2
    local output=$3
    if grep -Eq "${pattern}" "${output}"; then
        printf 'unexpected %s\n' "${description}" >&2
        return 1
    fi
}

ExpectedAccessName()
{
    local case_name=$1
    case "${case_name}" in
        ub_to_gm_write_* | ub_to_gm_outer_loop_* | fixpipe_* | api_datacopypad_ub2gm_* | api_datacopy_nz2nd_*)
            printf 'write\n'
            ;;
        *)
            printf 'read\n'
            ;;
    esac
}


CheckTraceProcessingComplete()
{
    local output=$1
    RequirePattern \
        '^tool=memcheck .*dropped_device_operations=0([[:space:]]|$)' \
        'zero dropped device operations' "${output}" || return 1
    RequirePattern \
        '^\[CLI\] outcome=forwarded has_errors=[01] truncated=0 child_exit=0 exit=(0|2)$' \
        'complete forwarded CLI result' "${output}" || return 1
    RequirePattern \
        '^status=complete aclsan_unsubscribe=0 dropped_messages=0 analysis_complete=true report_truncated=false$' \
        'complete device trace session' "${output}" || return 1
    RequirePattern \
        '^callbacks=[0-9]+ malformed_callbacks=0 framework_errors=0 dropped_messages=0$' \
        'error-free callback processing' "${output}" || return 1
    RejectPattern 'outcome=infra_failed' 'infrastructure failure' "${output}" || return 1
}

CheckCommonOutput()
{
    local case_name=$1
    local output=$2
    local access_name
    access_name=$(ExpectedAccessName "${case_name}")

    CheckTraceProcessingComplete "${output}" || return 1
    if [[ "${case_name}" == *_valid ]]; then
        RequirePattern '^tool=memcheck .*device_operations=[1-9][0-9]* .*errors=0([[:space:]]|$)' \
            'nonzero device operations and zero-error summary' "${output}" || return 1
        RejectPattern 'Invalid GM (read|write)' 'GM out-of-bounds diagnostic in a valid case' "${output}" || return 1
    else
        RequirePattern "Invalid GM ${access_name} of size [1-9][0-9]* bytes" \
            "GM ${access_name} out-of-bounds diagnostic" "${output}" || return 1
        RequirePattern '^tool=memcheck .*errors=[1-9][0-9]*([[:space:]]|$)' \
            'nonzero-error summary' "${output}" || return 1
    fi
}


CheckCaseOutput()
{
    local case_name=$1
    local output=$2

    if [[ "${case_name}" == nddma_missing_active_stride ]]; then
        CheckTraceProcessingComplete "${output}" || return 1
        RequirePattern '^tool=memcheck .*device_operations=0 .*errors=0([[:space:]]|$)' \
            'zero-operation and zero-error summary for skipped NDDMA' "${output}" || return 1
        RejectPattern 'Invalid GM (read|write)' \
            'GM out-of-bounds diagnostic for skipped NDDMA' "${output}" || return 1
        return 0
    fi
    if [[ "${case_name}" == local_ub_to_l1_valid ]]; then
        CheckTraceProcessingComplete "${output}" || return 1
        RequirePattern '^tool=memcheck .*errors=0([[:space:]]|$)' \
            'zero-error summary for local-memory transfer' "${output}" || return 1
        RejectPattern 'Invalid GM (read|write)' \
            'GM out-of-bounds diagnostic for local-memory transfer' "${output}" || return 1
        return 0
    fi
    CheckCommonOutput "${case_name}" "${output}" || return 1
}

PrepareBuild()
{
    local build_all=$1
    if [[ "${build_prepared}" == true ]]; then
        return 0
    fi
    if [[ "${NPU_CHECK_E2E_REUSE_EXISTING_BUILD:-0}" != "1" ]]; then
        rm -rf -- "${example_dir}/build"
    fi
    if ! command -v npu-check >/dev/null 2>&1; then
      printf 'missing npu-check; run %s/build.sh first\n' "${demo_dir}" >&2
      return 1
    fi
    cmake -S "${example_dir}" -B "${example_dir}/build" -DCMAKE_ASC_ARCHITECTURES=dav-3510
    build_prepared=true
    if [[ "${build_all}" == true ]]; then
        cmake --build "${example_dir}/build" --parallel
        all_targets_built=true
    fi
}

IsRetryableSetDeviceFailure()
{
    local run_status=$1
    local output=$2
    [[ ${run_status} -eq 1 ]] || return 1
    grep -Eq 'aclrtSetDevice\(deviceId\) failed: 507033([[:space:]]|$)' "${output}" || return 1
    grep -Eq '^tool=memcheck allocations=0 frees=0 device_operations=0 synchronizations=0 ' "${output}" || return 1
    grep -Eq '^callbacks=0 malformed_callbacks=0 framework_errors=0 dropped_messages=0([[:space:]]|$)' "${output}" || return 1
    grep -Eq '^\[CLI\] outcome=app_failed has_errors=0 truncated=0 child_exit=1 exit=1([[:space:]]|$)' "${output}"
}

RunCase()
{
    local case_name=$1
    local executable_case=${case_name}
    local executable
    local run_status
    local attempt=1
    local max_attempts=${NPU_CHECK_E2E_SET_DEVICE_ATTEMPTS:-3}
    local case_timeout=${NPU_CHECK_E2E_CASE_TIMEOUT_SECONDS:-300}
    if ! [[ "${max_attempts}" =~ ^[1-9][0-9]*$ && "${case_timeout}" =~ ^[1-9][0-9]*$ ]]; then
        printf 'invalid retry or timeout configuration\n' >&2
        return 1
    fi
    if [[ "${case_name}" == fixpipe_quant_*_valid ]]; then
        executable_case=${case_name%_valid}
    elif [[ "${case_name}" == fixpipe_quant_*_oob ]]; then
        executable_case=${case_name%_oob}
    fi
    executable="${example_dir}/build/aclsan_demo_memory_access_${executable_case}"

    PrepareBuild false
    if [[ "${all_targets_built}" == false ]]; then
        cmake --build "${example_dir}/build" --target "aclsan_demo_memory_access_${executable_case}" --parallel
    fi
    current_output=$(mktemp)
    mkdir -p "${example_dir}/build/memory_access_logs"
    while true; do
        set +e
        if [[ "${case_name}" == fixpipe_quant_*_oob ]]; then
            NPU_CHECK_MEMORY_CASE_OOB=1 timeout "${case_timeout}" \
                npu-check --tool memcheck -- "${executable}" >"${current_output}" 2>&1
        else
            timeout "${case_timeout}" \
                npu-check --tool memcheck -- "${executable}" >"${current_output}" 2>&1
        fi
        run_status=$?
        set -e
        if ! IsRetryableSetDeviceFailure "${run_status}" "${current_output}" || [[ ${attempt} -ge ${max_attempts} ]]; then
            break
        fi
        cp -- "${current_output}" \
            "${example_dir}/build/memory_access_logs/${case_name}.attempt-${attempt}-set-device-507033.log"
        printf '%s: retrying after aclrtSetDevice 507033 (attempt %d/%d)\n' \
            "${case_name}" "${attempt}" "${max_attempts}" >&2
        attempt=$((attempt + 1))
    done

    cp -- "${current_output}" "${example_dir}/build/memory_access_logs/${case_name}.log"

    if [[ "${case_name}" == *_oob ]]; then
        if [[ ${run_status} -ne 0 && ${run_status} -ne 2 ]]; then
            printf '%s: expected diagnostic exit 0 or 2, got %d\n' "${case_name}" "${run_status}" >&2
            cat "${current_output}" >&2
            return 1
        fi
    elif [[ ${run_status} -ne 0 ]]; then
        printf '%s: expected forwarded child exit 0, got %d\n' "${case_name}" "${run_status}" >&2
        cat "${current_output}" >&2
        return 1
    fi
    if ! CheckCaseOutput "${case_name}" "${current_output}"; then
        printf '%s: output validation failed\n' "${case_name}" >&2
        cat "${current_output}" >&2
        return 1
    fi

    rm -f -- "${current_output}"
    current_output=""
    printf '%s: PASS\n' "${case_name}"
}

if [[ $# -ne 1 ]]; then
    Usage
    exit 2
fi

case "$1" in
    --list)
        ListCases
        ;;
    all)
        PrepareBuild true
        start_case=${NPU_CHECK_MEMORY_ACCESS_START_CASE:-}
        start_reached=false
        if [[ -z "${start_case}" ]]; then
            start_reached=true
        elif ! IsKnownCase "${start_case}"; then
            printf 'unsupported start case: %s\n' "${start_case}" >&2
            exit 2
        fi
        for case_name in "${cases[@]}"; do
            if [[ "${case_name}" == "${start_case}" ]]; then
                start_reached=true
            fi
            if [[ "${start_reached}" == false ]]; then
                continue
            fi
            RunCase "${case_name}"
        done
        ;;
    smoke)
        for case_name in "${smoke_cases[@]}"; do
            RunCase "${case_name}"
        done
        ;;
    *)
        if ! IsKnownCase "$1"; then
            printf 'unsupported memory access case: %s\n' "$1" >&2
            Usage
            exit 2
        fi
        RunCase "$1"
        ;;
esac
