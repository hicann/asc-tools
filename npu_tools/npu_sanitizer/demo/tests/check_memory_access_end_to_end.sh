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
example_dir="${demo_dir}/examples/memcheck/memory_access"
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

RequirePatternCount()
{
    local pattern=$1
    local expected_count=$2
    local description=$3
    local output=$4
    local actual_count
    actual_count=$(grep -Ec "${pattern}" "${output}" || true)
    if [[ ${actual_count} -ne ${expected_count} ]]; then
        printf 'expected %s count %d, got %d\n' "${description}" "${expected_count}" "${actual_count}" >&2
        return 1
    fi
}

ExpectedInstructionId()
{
    local case_name=$1
    case "${case_name}" in
        gm_to_ub_read_* | gm_to_ub_block_repeat_* | gm_to_ub_outer_loop_* | local_ub_to_l1_valid | \
            api_datacopypad_gm2ub_* | api_datacopy_nd2nz_*)
            printf '84\n'
            ;;
        gm_to_ub_b16_*)
            printf '85\n'
            ;;
        gm_to_ub_b32_*)
            printf '86\n'
            ;;
        ub_to_gm_write_* | ub_to_gm_outer_loop_* | api_datacopypad_ub2gm_* | api_datacopy_nz2nd_*)
            printf '83\n'
            ;;
        cube_gm_to_l1_outer_loop_*)
            printf '74\n'
            ;;
        cube_gm_to_l1_id73_*)
            printf '73\n'
            ;;
        cube_gm_to_l1_id74_* | api_datacopypad_gm2l1_*)
            printf '74\n'
            ;;
        cube_gm_to_l1_id75_*)
            printf '75\n'
            ;;
        cube_gm_to_l1_id76_*)
            printf '76\n'
            ;;
        multi_nd2nz_b8_*)
            printf '77\n'
            ;;
        multi_nd2nz_b32_*)
            printf '79\n'
            ;;
        multi_nd2nz_*)
            printf '78\n'
            ;;
        multi_dn2nz_b8_*)
            printf '80\n'
            ;;
        multi_dn2nz_b32_*)
            printf '82\n'
            ;;
        multi_dn2nz_*)
            printf '81\n'
            ;;
        fixpipe_s32_*)
            printf '92\n'
            ;;
        fixpipe_quant_q2[1-2]_*)
            printf '92\n'
            ;;
        fixpipe_quant_q2[5-6]_*)
            printf '91\n'
            ;;
        fixpipe_quant_q0_s32_* | fixpipe_quant_q8_* | fixpipe_quant_q9_* | \
            fixpipe_quant_q10_* | fixpipe_quant_q11_* | fixpipe_quant_q3[5-6]_* | \
            fixpipe_quant_b8_s32_nz2dn_* | fixpipe_quant_b4_s32_nz2dn_*)
            printf '92\n'
            ;;
        fixpipe_*)
            printf '91\n'
            ;;
        nddma_b8_* | nddma_rank1_b8_* | nddma_rank4_singleton_b8_* | nddma_missing_active_stride | \
            nddma_p0_broadcast_b8_* | nddma_p0_multi_block_b8_* | \
            nddma_padding_*_b8_*)
            printf '87\n'
            ;;
        nddma_b16_* | nddma_rank2_singleton_b16_* | nddma_padding_*_b16_*)
            printf '88\n'
            ;;
        nddma_b32_* | nddma_rank3_singleton_b32_* | nddma_padding_*_b32_* | nddma_p0_b64_* | api_nddma_b64_*)
            printf '89\n'
            ;;
        load2dv2_* | api_load_data_*)
            printf '72\n'
            ;;
    esac
}

ExpectedLayoutKind()
{
    local case_name=$1
    case "${case_name}" in
        fixpipe_quant_b8_*_nz2nd_* | fixpipe_quant_b8_*_nz2dn_*)
            printf '3\n' # ACLSAN_MEM_LAYOUT_BLOCK_REPEAT
            ;;
        gm_to_ub_outer_loop_* | ub_to_gm_outer_loop_* | cube_gm_to_l1_outer_loop_* | \
            cube_gm_to_l1_id7[3-6]_outer_loop_*)
            printf '4\n' # ACLSAN_MEM_LAYOUT_ND_AFFINE
            ;;
        gm_to_ub_read_* | gm_to_ub_b16_* | gm_to_ub_b32_* | ub_to_gm_write_* | local_ub_to_l1_valid | cube_gm_to_l1_id73_* | fixpipe_valid | fixpipe_oob | fixpipe_s32_valid | fixpipe_s32_oob | fixpipe_channel_split_* | fixpipe_c0pad_* | fixpipe_quant_* | api_load_data_2d_*)
            printf '2\n' # ACLSAN_MEM_LAYOUT_RANGE
            ;;
        gm_to_ub_block_repeat_* | cube_gm_to_l1_id7[4-6]_* | multi_dn2nz_* | load2dv2_* | fixpipe_nz2* | fixpipe_s32_nz2* | api_datacopy* | api_load_data_* | api_datacopypad_gm2l1_*)
            printf '3\n' # ACLSAN_MEM_LAYOUT_BLOCK_REPEAT
            ;;
        multi_nd2nz_* | nddma_* | api_nddma_*)
            printf '4\n' # ACLSAN_MEM_LAYOUT_ND_AFFINE
            ;;
    esac
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

CheckOrderedPatterns()
{
    local first_pattern=$1
    local second_pattern=$2
    local description=$3
    local output=$4
    if ! awk -v first_pattern="${first_pattern}" -v second_pattern="${second_pattern}" '
        $0 ~ first_pattern { seen_first = 1 }
        seen_first && $0 ~ second_pattern { found = 1; exit }
        END { exit found ? 0 : 1 }
    ' "${output}"; then
        printf 'missing ordered %s\n' "${description}" >&2
        return 1
    fi
}

CheckCbdataAddressDelta()
{
    local output=$1
    local expected_delta=$2
    local addresses=()
    mapfile -t addresses < <(
        grep -E '\[cbdata\] type=AclsanDeviceMemoryAccessData ' "${output}" |
            sed -E 's/.* address=(0x[0-9a-fA-F]+).*/\1/'
    )
    if [[ ${#addresses[@]} -lt 2 || $((addresses[1] - addresses[0])) -ne ${expected_delta} ]]; then
        printf 'unexpected first two cbdata address delta, expected %d\n' "${expected_delta}" >&2
        return 1
    fi
}

CheckFirstCbdataAddressMatchesFixpipeDst()
{
    local output=$1
    local dst_address
    local cbdata_address
    dst_address=$(sed -n -E \
        's/.*\[param\] type=FixL0cToOutParamField .*dstAddr=(0x[0-9a-fA-F]+).*/\1/p' \
        "${output}" | head -1)
    cbdata_address=$(grep -E \
        '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessMode=2 .*pipeline=10([[:space:]]|$)' \
        "${output}" | sed -n -E \
        's/.* address=(0x[0-9a-fA-F]+).*/\1/p' | head -1)
    if [[ -z "${dst_address}" || -z "${cbdata_address}" ||
        $((dst_address)) -ne $((cbdata_address)) ]]; then
        printf 'first cbdata address does not match Fixpipe dstAddr\n' >&2
        return 1
    fi
}

CheckB4ConversionCbdataAddresses()
{
    local output=$1
    local dst_address
    local addresses=()
    dst_address=$(sed -n -E \
        's/.*\[param\] type=FixL0cToOutParamField .*dstAddr=(0x[0-9a-fA-F]+).*/\1/p' \
        "${output}" | head -1)
    mapfile -t addresses < <(
        grep -E \
            '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessMode=2 .*pipeline=10([[:space:]]|$)' \
            "${output}" |
            sed -E 's/.* address=(0x[0-9a-fA-F]+).*/\1/'
    )
    if [[ -z "${dst_address}" || ${#addresses[@]} -ne 32 ]]; then
        printf 'unexpected B4 conversion cbdata address count\n' >&2
        return 1
    fi

    local index
    local expected_address
    for ((index = 0; index < 32; ++index)); do
        expected_address=$((dst_address + (index % 2) * 256 + (index / 2) * 16))
        if [[ $((addresses[index])) -ne ${expected_address} ]]; then
            printf 'unexpected B4 conversion cbdata address at index %d\n' "${index}" >&2
            return 1
        fi
    done
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
    RejectPattern 'status=incomplete|outcome=infra_failed|malformed_callbacks=[1-9]|framework_errors=[1-9]' \
        'incomplete device trace processing' "${output}" || return 1
}

CheckCommonOutput()
{
    local case_name=$1
    local output=$2
    local instruction_id
    local layout_kind
    local access_name
    instruction_id=$(ExpectedInstructionId "${case_name}")
    layout_kind=$(ExpectedLayoutKind "${case_name}")
    access_name=$(ExpectedAccessName "${case_name}")

    RequirePattern "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        "raw instruction ID ${instruction_id}" "${output}" || return 1
    RequirePattern "\\[cbdata\\] type=AclsanDeviceMemoryAccessData .*layoutKind=${layout_kind}([[:space:]]|$)" \
        "memory cbdata layout kind ${layout_kind}" "${output}" || return 1
    CheckTraceProcessingComplete "${output}" || return 1
    if [[ "${case_name}" == *_valid ]]; then
        RequirePattern '^tool=memcheck .*errors=0([[:space:]]|$)' 'zero-error summary' "${output}" || return 1
        RejectPattern 'Invalid GM (read|write)' 'GM out-of-bounds diagnostic in a valid case' "${output}" || return 1
    else
        RequirePattern "Invalid GM ${access_name} of size [1-9][0-9]* bytes" \
            "GM ${access_name} out-of-bounds diagnostic" "${output}" || return 1
        RequirePattern '^tool=memcheck .*errors=[1-9][0-9]*([[:space:]]|$)' \
            'nonzero-error summary' "${output}" || return 1
    fi
}

CheckLoad2dOutput()
{
    local case_name=$1
    local output=$2
    local source_stride='-?[0-9]+'

    if [[ "${case_name}" == load2dv2_negative_stride_* ]]; then
        source_stride='-[1-9][0-9]*'
    fi

    RequirePattern '\[raw\].*instrId=124([[:space:]]|$)' 'ID 124 MTE2 source state' "${output}" || return 1
    RequirePattern "\\[param\\] type=Mte2SourceParamField srcStride=${source_stride}([[:space:]]|$)" \
        'decoded MTE2 source stride' "${output}" || return 1
    RequirePattern \
        '\[param\] type=LoadGmToCbuf2DV2ParamField .*srcAddr=0x[0-9a-fA-F]+ .*mStartPosition=[0-9]+ kStartPosition=[0-9]+ .*mStep=[1-9][0-9]* kStep=[1-9][0-9]* .*decompMode=0([[:space:]]|$)' \
        'decoded LOAD_OUT_TO_L1_2DV2 address fields' "${output}" || return 1
    RequirePattern \
        '\[cbdata\] layout=block_repeat blockNum=1 blockSize=[1-9][0-9]* blockStride=0 ' \
        'LOAD_OUT_TO_L1_2DV2 block-repeat layout fields' "${output}" || return 1
    CheckOrderedPatterns '\[raw\].*instrId=124([[:space:]]|$)' '\[raw\].*instrId=72([[:space:]]|$)' \
        'ID 124 state before ID 72 access' "${output}" || return 1
    CheckOrderedPatterns '\[raw\].*instrId=72([[:space:]]|$)' \
        '\[cbdata\] type=AclsanDeviceMemoryAccessData' 'ID 72 access before cbdata' "${output}" || return 1
}

CheckNdDmaOutput()
{
    local case_name=$1
    local output=$2
    local instruction_id
    instruction_id=$(ExpectedInstructionId "${case_name}")

    RequirePattern '\[raw\].*instrId=131([[:space:]]|$)' 'ID 131 ND-DMA padding state' "${output}" || return 1
    if [[ "${case_name}" == nddma_rank* ]]; then
        local expected_profile
        local expected_dimensions
        local expected_loop_sizes
        local expected_source_strides
        case "${case_name}" in
            nddma_rank1_b8_*)
                expected_profile=1; expected_dimensions=1
                expected_loop_sizes='4,1,1,1,1'; expected_source_strides=(1 0 0 0 0)
                ;;
            nddma_rank2_singleton_b16_*)
                expected_profile=2; expected_dimensions=2
                expected_loop_sizes='1,4,1,1,1'; expected_source_strides=(1 4 0 0 0)
                ;;
            nddma_rank3_singleton_b32_*)
                expected_profile=3; expected_dimensions=3
                expected_loop_sizes='2,1,3,1,1'; expected_source_strides=(1 4 16 0 0)
                ;;
            nddma_rank4_singleton_b8_*)
                expected_profile=4; expected_dimensions=4
                expected_loop_sizes='2,1,3,2,1'; expected_source_strides=(1 4 16 64 0)
                ;;
        esac
        RequirePattern \
            "NDDMA_PROFILE=${expected_profile} NDDMA_DIMENSIONS=${expected_dimensions} NDDMA_LOOP_SIZES=\\[${expected_loop_sizes}\\]" \
            'selected NDDMA rank and singleton profile' "${output}" || return 1
        RequirePattern \
            "\\[param\\] type=NdDmaParamField instrId=${instruction_id} .*loopSizes=\\[${expected_loop_sizes}\\] " \
            'decoded NDDMA loop sizes' "${output}" || return 1
        RequirePattern \
            '\[param\] type=NdDmaPadCountParamField left=\[0,0,0,0\] right=\[0,0,0,0\]([[:space:]]|$)' \
            'zero NDDMA padding for rank profile' "${output}" || return 1
    else
        RequirePattern \
            '\[param\] type=NdDmaPadCountParamField (left=\[[^]]*[1-9][0-9]*[^]]*\]|.*right=\[[^]]*[1-9][0-9]*[^]]*\])' \
            'nonzero ND-DMA padding count field' "${output}" || return 1
    fi
    local previous_id=131
    local stride_id
    local stride_index=0
    local expected_stride
    for stride_id in 132 133 134 135 136; do
        if [[ "${case_name}" == nddma_rank* ]]; then
            expected_stride=${expected_source_strides[stride_index]}
        else
            case "${stride_index}" in
                0) expected_stride=1 ;;
                1) expected_stride=4 ;;
                2) expected_stride=16 ;;
                3) expected_stride=64 ;;
                4) expected_stride=256 ;;
            esac
        fi
        RequirePattern "\\[raw\\].*instrId=${stride_id}([[:space:]]|$)" \
            "ID ${stride_id} ND-DMA stride state" "${output}" || return 1
        RequirePattern \
            "\\[param\\] type=NdDmaLoopStrideParamField loopIndex=${stride_index} srcStride=${expected_stride}([[:space:]]|$)" \
            "ID ${stride_id} decoded ND-DMA source stride" "${output}" || return 1
        CheckOrderedPatterns "\\[raw\\].*instrId=${previous_id}([[:space:]]|$)" \
            "\\[raw\\].*instrId=${stride_id}([[:space:]]|$)" \
            "ID ${previous_id} before ID ${stride_id}" "${output}" || return 1
        previous_id=${stride_id}
        stride_index=$((stride_index + 1))
    done
    CheckOrderedPatterns '\[raw\].*instrId=131([[:space:]]|$)' \
        "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        "ID 131 state before ID ${instruction_id} access" "${output}" || return 1
    CheckOrderedPatterns '\[raw\].*instrId=136([[:space:]]|$)' \
        "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        "ID 136 state before ID ${instruction_id} access" "${output}" || return 1
    CheckOrderedPatterns "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        '\[cbdata\] type=AclsanDeviceMemoryAccessData' \
        "ID ${instruction_id} access before cbdata" "${output}" || return 1
}

CheckNdDmaPaddingOutput()
{
    local case_name=$1
    local output=$2
    local instruction_id
    local data_bits
    local source_bytes
    local allocated_bytes
    local padding_mode=1
    local mode_name=constant
    local config_name=per_dimension
    local constant_value=0
    local loop0_left=2
    local loop0_right=1
    local id131_left='1,0,0,0'
    local id131_right='2,0,0,0'
    local loop_sizes='4,3,1,1,1'

    instruction_id=$(ExpectedInstructionId "${case_name}")
    case "${case_name}" in
        *_b8_*) data_bits=8; source_bytes=20; allocated_bytes=19 ;;
        *_b16_*) data_bits=16; source_bytes=40; allocated_bytes=38 ;;
        *_b32_*) data_bits=32; source_bytes=80; allocated_bytes=76 ;;
    esac
    if [[ "${case_name}" == *nearest* ]]; then
        padding_mode=0
        mode_name=nearest
    elif [[ "${case_name}" == *constant_nonzero* ]]; then
        constant_value=7
    elif [[ "${case_name}" == *uniform_constant* ]]; then
        config_name=uniform
        loop0_left=3
        loop0_right=4
        id131_left='3,0,0,0'
        id131_right='4,0,0,0'
    elif [[ "${case_name}" == *max_loop0_padding* ]]; then
        source_bytes=4
        allocated_bytes=3
        loop0_left=255
        loop0_right=255
        id131_left='0,0,0,0'
        id131_right='0,0,0,0'
        loop_sizes='4,1,1,1,1'
    fi
    if [[ "${case_name}" == *_valid ]]; then
        allocated_bytes=${source_bytes}
    fi

    RequirePattern \
        "NDDMA_PADDING_BITS=${data_bits} NDDMA_PADDING_MODE=${mode_name} NDDMA_PADDING_CONFIG=${config_name} NDDMA_PADDING_CONSTANT_VALUE=${constant_value} .*NDDMA_EXPECTED_GM_ACCESS_END_BYTES=${source_bytes} NDDMA_ALLOCATED_GM_BYTES=${allocated_bytes} " \
        'selected supported NDDMA padding API profile' "${output}" || return 1
    RequirePattern \
        "\\[param\\] type=NdDmaPadCountParamField left=\\[${id131_left}\\] right=\\[${id131_right}\\]([[:space:]]|$)" \
        'decoded NDDMA loop1 padding state' "${output}" || return 1
    RequirePattern \
        "\\[param\\] type=NdDmaParamField instrId=${instruction_id} dataBits=${data_bits} .*loopSizes=\\[${loop_sizes}\\] loop0LeftPaddingCount=${loop0_left} loop0RightPaddingCount=${loop0_right} paddingMode=${padding_mode} " \
        'decoded supported NDDMA padding mode and loop0 counts' "${output}" || return 1

    local previous_id=131
    local stride_id
    local stride_index=0
    local expected_strides=(1 8 0 0 0)
    for stride_id in 132 133 134 135 136; do
        RequirePattern "\\[raw\\].*instrId=${stride_id}([[:space:]]|$)" \
            "ID ${stride_id} NDDMA padding stride state" "${output}" || return 1
        RequirePattern \
            "\\[param\\] type=NdDmaLoopStrideParamField loopIndex=${stride_index} srcStride=${expected_strides[stride_index]}([[:space:]]|$)" \
            "ID ${stride_id} NDDMA padding source stride" "${output}" || return 1
        CheckOrderedPatterns "\\[raw\\].*instrId=${previous_id}([[:space:]]|$)" \
            "\\[raw\\].*instrId=${stride_id}([[:space:]]|$)" \
            "ID ${previous_id} before ID ${stride_id}" "${output}" || return 1
        previous_id=${stride_id}
        stride_index=$((stride_index + 1))
    done
    CheckOrderedPatterns '\[raw\].*instrId=136([[:space:]]|$)' \
        "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        "ID 136 before NDDMA padding ID ${instruction_id}" "${output}" || return 1
    CheckOrderedPatterns "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        '\[cbdata\] type=AclsanDeviceMemoryAccessData' \
        "NDDMA padding ID ${instruction_id} before cbdata" "${output}" || return 1
}

CheckNdDmaP0Output()
{
    local case_name=$1
    local output=$2
    local profile_name
    local expected_records
    local expected_blocks
    local expected_end_bytes
    local expected_allocated_bytes
    local instruction_id
    local data_bits
    local loop_sizes
    local id131_left='0,0,0,0'
    local id131_right='0,0,0,0'
    local expected_strides=()

    case "${case_name}" in
        nddma_p0_broadcast_b8_*)
            profile_name=broadcast_b8
            expected_records=1; expected_blocks=1; expected_end_bytes=16
            instruction_id=87; data_bits=8; loop_sizes='16,3,1,1,1'
            expected_strides=(1 0 0 0 0)
            ;;
        nddma_p0_b64_4d_padding_*)
            profile_name=b64_4d_padding
            expected_records=1; expected_blocks=1; expected_end_bytes=688
            instruction_id=89; data_bits=32; loop_sizes='2,2,2,2,2'
            id131_left='1,0,0,0'; id131_right='1,0,0,0'
            expected_strides=(1 2 8 32 128)
            ;;
        nddma_p0_b64_5d_padding_*)
            profile_name=b64_5d_padding
            expected_records=2; expected_blocks=1; expected_end_bytes=2736
            instruction_id=89; data_bits=32; loop_sizes='2,2,2,2,2'
            id131_left='1,0,0,0'; id131_right='1,0,0,0'
            expected_strides=(2 8 32 128 512)
            ;;
        nddma_p0_b64_5d_singleton_*)
            profile_name=b64_5d_singleton
            expected_records=1; expected_blocks=1; expected_end_bytes=688
            instruction_id=89; data_bits=32; loop_sizes='2,2,2,2,2'
            expected_strides=(1 2 8 32 128)
            ;;
        nddma_p0_multi_block_b8_*)
            profile_name=multi_block_b8
            expected_records=2; expected_blocks=2; expected_end_bytes=52
            instruction_id=87; data_bits=8; loop_sizes='4,2,1,1,1'
            expected_strides=(1 0 0 0 0)
            ;;
    esac

    expected_allocated_bytes=${expected_end_bytes}
    local expected_result=normal
    if [[ "${case_name}" == *_oob ]]; then
        expected_allocated_bytes=$((expected_end_bytes - 1))
        expected_result=oob
    fi
    RequirePattern \
        "NDDMA_P0_PROFILE=${profile_name} NDDMA_P0_EXPECTED_RECORDS=${expected_records} NDDMA_P0_BLOCKS=${expected_blocks} NDDMA_P0_EXPECTED_GM_ACCESS_END_BYTES=${expected_end_bytes} NDDMA_P0_ALLOCATED_GM_BYTES=${expected_allocated_bytes} NDDMA_P0_EXPECTED_RESULT=${expected_result}([[:space:]]|$)" \
        'selected NDDMA P0 public API profile' "${output}" || return 1

    local state_id
    for state_id in 131 132 133 134 135 136; do
        RequirePatternCount "\\[raw\\].*instrId=${state_id}([[:space:]]|$)" "${expected_records}" \
            "NDDMA P0 ID ${state_id} records" "${output}" || return 1
    done
    RequirePatternCount "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" "${expected_records}" \
        "NDDMA P0 ID ${instruction_id} records" "${output}" || return 1
    RequirePatternCount \
        "\\[cbdata\\] type=AclsanDeviceMemoryAccessData .*dataBits=${data_bits} .*layoutKind=4([[:space:]]|$)" \
        "${expected_records}" 'NDDMA P0 cbdata records' "${output}" || return 1

    if [[ "${profile_name}" == multi_block_b8 ]]; then
        RequirePatternCount \
            '\[param\] type=NdDmaPadCountParamField left=\[1,0,0,0\] right=\[2,0,0,0\]([[:space:]]|$)' \
            1 'block 0 NDDMA padding state' "${output}" || return 1
        RequirePatternCount \
            '\[param\] type=NdDmaPadCountParamField left=\[3,0,0,0\] right=\[4,0,0,0\]([[:space:]]|$)' \
            1 'block 1 NDDMA padding state' "${output}" || return 1
        RequirePatternCount \
            '\[param\] type=NdDmaLoopStrideParamField loopIndex=0 srcStride=1([[:space:]]|$)' \
            2 'per-block NDDMA loop 0 stride' "${output}" || return 1
        RequirePatternCount \
            '\[param\] type=NdDmaLoopStrideParamField loopIndex=1 srcStride=8([[:space:]]|$)' \
            1 'block 0 NDDMA loop 1 stride' "${output}" || return 1
        RequirePatternCount \
            '\[param\] type=NdDmaLoopStrideParamField loopIndex=1 srcStride=16([[:space:]]|$)' \
            1 'block 1 NDDMA loop 1 stride' "${output}" || return 1
        local zero_stride_index
        for zero_stride_index in 2 3 4; do
            RequirePatternCount \
                "\\[param\\] type=NdDmaLoopStrideParamField loopIndex=${zero_stride_index} srcStride=0([[:space:]]|$)" \
                2 "per-block NDDMA loop ${zero_stride_index} zero stride" "${output}" || return 1
        done
        RequirePatternCount \
            "\\[param\\] type=NdDmaParamField instrId=87 dataBits=8 .*loopSizes=\\[${loop_sizes}\\] .*paddingMode=1 " \
            2 'two block-specific NDDMA access fields' "${output}" || return 1
        RequirePatternCount '\[raw\].*blockId=0 .*instrId=87([[:space:]]|$)' 1 \
            'block 0 raw NDDMA access' "${output}" || return 1
        RequirePatternCount '\[raw\].*blockId=1 .*instrId=87([[:space:]]|$)' 1 \
            'block 1 raw NDDMA access' "${output}" || return 1
        RequirePatternCount '\[cbdata\].*blockId=0 .*pipeline=4([[:space:]]|$)' 1 \
            'block 0 NDDMA cbdata' "${output}" || return 1
        RequirePatternCount '\[cbdata\].*blockId=1 .*pipeline=4([[:space:]]|$)' 1 \
            'block 1 NDDMA cbdata' "${output}" || return 1
    else
        RequirePatternCount \
            "\\[param\\] type=NdDmaPadCountParamField left=\\[${id131_left}\\] right=\\[${id131_right}\\]([[:space:]]|$)" \
            "${expected_records}" 'NDDMA P0 padding state' "${output}" || return 1
        local loop_index
        for loop_index in 0 1 2 3 4; do
            RequirePatternCount \
                "\\[param\\] type=NdDmaLoopStrideParamField loopIndex=${loop_index} srcStride=${expected_strides[loop_index]}([[:space:]]|$)" \
                "${expected_records}" "NDDMA P0 loop ${loop_index} stride" "${output}" || return 1
        done
        RequirePatternCount \
            "\\[param\\] type=NdDmaParamField instrId=${instruction_id} dataBits=${data_bits} .*loopSizes=\\[${loop_sizes}\\] .*paddingMode=1 " \
            "${expected_records}" 'NDDMA P0 access fields' "${output}" || return 1
    fi

    RequirePattern "^tool=memcheck .*device_operations=${expected_records} " \
        'NDDMA P0 device operation count' "${output}" || return 1
    if [[ "${profile_name}" == b64_5d_padding ]]; then
        local source_addresses=()
        mapfile -t source_addresses < <(
            grep -E '\[param\] type=NdDmaParamField instrId=89 ' "${output}" |
                sed -E 's/.* srcAddr=(0x[0-9a-fA-F]+).*/\1/'
        )
        if [[ ${#source_addresses[@]} -ne 2 ||
            $((source_addresses[1] - source_addresses[0])) -ne 4 ]]; then
            printf 'unexpected padded b64 5D split source addresses\n' >&2
            return 1
        fi
    fi
}

CheckNdDmaMissingStrideOutput()
{
    local output=$1
    RequirePattern 'NDDMA_MISSING_ACTIVE_STRIDE=loop0 NDDMA_REQUIRED_SET_INSTR_ID=132' \
        'missing active NDDMA stride intent' "${output}" || return 1
    RequirePattern '\[raw\].*instrId=87([[:space:]]|$)' 'raw ID 87 NDDMA access' "${output}" || return 1
    RejectPattern '\[raw\].*instrId=132([[:space:]]|$)' 'unexpected loop0 stride SET' "${output}" || return 1
    RequirePattern \
        'cannot resolve GM memory access status=MISSING_REGISTER_STATE instrId=87 .*requiredSetInstrId=132([[:space:]]|$)' \
        'actionable missing NDDMA loop0 stride diagnostic' "${output}" || return 1
    RejectPattern 'unsupported raw trace.*instrId=87' \
        'misclassified recognized NDDMA trace' "${output}" || return 1
    RejectPattern '\[cbdata\] type=AclsanDeviceMemoryAccessData' \
        'memory cbdata generated without an active stride' "${output}" || return 1
    RequirePattern '^tool=memcheck .*device_operations=0 .*dropped_device_operations=0([[:space:]]|$)' \
        'no fabricated device operation for missing stride' "${output}" || return 1
    CheckTraceProcessingComplete "${output}" || return 1
}

CheckMultiOutput()
{
    local case_name=$1
    local output=$2
    local instruction_id
    instruction_id=$(ExpectedInstructionId "${case_name}")

    RequirePattern '\[raw\].*instrId=399([[:space:]]|$)' 'ID 399 MTE2 NZ state' "${output}" || return 1
    CheckOrderedPatterns '\[raw\].*instrId=399([[:space:]]|$)' \
        "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
        "ID 399 state before ID ${instruction_id} access" "${output}" || return 1

    if [[ "${case_name}" == multi_dn2nz_* ]]; then
        local expected_matrix_num=1
        local expected_data_bits=16
        local expected_block_size=8
        local expected_repeat_times=16
        local expected_repeat_stride=32
        local expected_footprint=488
        local expected_allocation=488
        if [[ "${case_name}" == *_matrix2_* ]]; then
            expected_matrix_num=2
            expected_repeat_times=32
            expected_footprint=1000
            expected_allocation=1000
            if [[ "${case_name}" == *_oob ]]; then
                expected_allocation=968
            fi
        fi
        if [[ "${case_name}" == multi_dn2nz_b8_* ]]; then
            expected_data_bits=8
            expected_block_size=4
            expected_repeat_stride=16
            expected_footprint=244
            expected_allocation=244
            if [[ "${case_name}" == *_matrix2_* ]]; then
                expected_footprint=500
                expected_allocation=500
                if [[ "${case_name}" == *_oob ]]; then
                    expected_allocation=468
                fi
            elif [[ "${case_name}" == *_oob ]]; then
                expected_allocation=212
            fi
        elif [[ "${case_name}" == multi_dn2nz_b32_* ]]; then
            expected_data_bits=32
            expected_block_size=16
            expected_repeat_stride=64
            expected_footprint=976
            expected_allocation=976
            if [[ "${case_name}" == *_matrix2_* ]]; then
                expected_footprint=2000
                expected_allocation=2000
                if [[ "${case_name}" == *_oob ]]; then
                    expected_allocation=1968
                fi
            elif [[ "${case_name}" == *_oob ]]; then
                expected_allocation=944
            fi
        elif [[ "${case_name}" != *_matrix2_* && "${case_name}" == *_oob ]]; then
            expected_allocation=456
        fi
        RequirePattern \
            "\\[param\\] type=Mte2NzParamField matrixNum=${expected_matrix_num}([[:space:]]|$)" \
            'decoded DN2NZ matrix count' "${output}" || return 1
        RequirePattern \
            "MULTI_DN_MATRIX_NUM=${expected_matrix_num} MULTI_DN_EXPECTED_GM_FOOTPRINT_BYTES=${expected_footprint} MULTI_DN_ALLOCATED_GM_BYTES=${expected_allocation}([[:space:]]|$)" \
            'DN2NZ sample footprint contract' "${output}" || return 1
        RequirePattern \
            "\\[cbdata\\] type=AclsanDeviceMemoryAccessData .*dataBits=${expected_data_bits} .*layoutKind=3([[:space:]]|$)" \
            'DN2NZ cbdata type and layout' "${output}" || return 1
        RequirePattern \
            "\\[cbdata\\] layout=block_repeat blockNum=1 blockSize=${expected_block_size} blockStride=0 repeatTimes=${expected_repeat_times} repeatStride=${expected_repeat_stride}([[:space:]]|$)" \
            'exact DN2NZ matrix GM layout' "${output}" || return 1
    fi
}

CheckCubeDmaOutput()
{
    local case_name=$1
    local output=$2

    if [[ "${case_name}" == cube_gm_to_l1_id73_* ]]; then
        local padding_mode
        local expected_bytes
        local expected_stride
        [[ "${case_name}" =~ _mode([0-8])_ ]]
        padding_mode=${BASH_REMATCH[1]}
        case "${padding_mode}" in
            0 | 6 | 7 | 8)
                expected_bytes=96
                expected_stride=1
                ;;
            1) expected_bytes=3; expected_stride=0 ;;
            2) expected_bytes=6; expected_stride=0 ;;
            3) expected_bytes=12; expected_stride=0 ;;
            4) expected_bytes=24; expected_stride=0 ;;
            5) expected_bytes=48; expected_stride=0 ;;
        esac
        RequirePattern \
            "\\[param\\] type=CopyGmToCbufV2ParamField .*burstNum=3 burstLen=1 padFunctionMode=${padding_mode} .*srcStride=${expected_stride}([[:space:]]|$)" \
            'ID 73 padding-mode fields' "${output}" || return 1
        RequirePattern "\\[cbdata\\] layout=range bytes=${expected_bytes}([[:space:]]|$)" \
            'ID 73 exact GM source footprint' "${output}" || return 1
        return 0
    fi
    RequirePattern \
        '\[param\] type=CopyGmToCbufAlignV2ParamField .*burstNum=3 burstLen=32 .*burstSrcStride=64 ' \
        'ID 74-76 aligned GM source fields' "${output}" || return 1
    RequirePattern \
        '\[cbdata\] layout=block_repeat .*blockSize=32 .*repeatTimes=3 repeatStride=64([[:space:]]|$)' \
        'ID 74-76 exact GM source layout' "${output}" || return 1
}

CheckDmaOuterLoopOutput()
{
    local case_name=$1
    local output=$2
    local direction
    local size_id
    local loop1_id
    local loop2_id
    local access_id
    local loop1_src_stride
    local loop1_dst_stride
    local loop2_src_stride
    local loop2_dst_stride
    local data_bits

    case "${case_name}" in
        gm_to_ub_outer_loop_*)
            direction=1; size_id=128; loop1_id=129; loop2_id=130; access_id=84
            loop1_src_stride=64; loop1_dst_stride=32
            loop2_src_stride=192; loop2_dst_stride=64
            ;;
        ub_to_gm_outer_loop_*)
            direction=0; size_id=125; loop1_id=126; loop2_id=127; access_id=83
            loop1_src_stride=32; loop1_dst_stride=64
            loop2_src_stride=64; loop2_dst_stride=192
            ;;
        cube_gm_to_l1_outer_loop_* | cube_gm_to_l1_id7[3-6]_outer_loop_*)
            direction=2; size_id=394; loop1_id=395; loop2_id=396; access_id=74
            loop1_src_stride=64; loop1_dst_stride=32
            loop2_src_stride=192; loop2_dst_stride=64
            if [[ "${case_name}" =~ _id(7[3-6])_ ]]; then
                access_id=${BASH_REMATCH[1]}
            fi
            ;;
    esac

    RequirePattern \
        "\\[param\\] type=DmaLoopSizeParamField direction=${direction} loop1Size=2 loop2Size=3([[:space:]]|$)" \
        'active DMA outer-loop size state' "${output}" || return 1
    RequirePattern \
        "\\[param\\] type=DmaLoopStrideParamField direction=${direction} loopIndex=0 srcStride=${loop1_src_stride} dstStride=${loop1_dst_stride}([[:space:]]|$)" \
        'active DMA loop1 stride state' "${output}" || return 1
    RequirePattern \
        "\\[param\\] type=DmaLoopStrideParamField direction=${direction} loopIndex=1 srcStride=${loop2_src_stride} dstStride=${loop2_dst_stride}([[:space:]]|$)" \
        'active DMA loop2 stride state' "${output}" || return 1
    CheckOrderedPatterns \
        "\\[raw\\].*instrId=${size_id}([[:space:]]|$)" \
        "\\[raw\\].*instrId=${loop1_id}([[:space:]]|$)" \
        "ID ${size_id} before ID ${loop1_id}" "${output}" || return 1
    CheckOrderedPatterns \
        "\\[raw\\].*instrId=${loop1_id}([[:space:]]|$)" \
        "\\[raw\\].*instrId=${loop2_id}([[:space:]]|$)" \
        "ID ${loop1_id} before ID ${loop2_id}" "${output}" || return 1
    CheckOrderedPatterns \
        "\\[raw\\].*instrId=${loop2_id}([[:space:]]|$)" \
        "\\[raw\\].*instrId=${access_id}([[:space:]]|$)" \
        "ID ${loop2_id} before ID ${access_id}" "${output}" || return 1
    if [[ "${case_name}" == cube_gm_to_l1_* ]]; then
        local expected_allocation=480
        if [[ "${case_name}" == *_oob ]]; then
            expected_allocation=448
        fi
        RequirePattern \
            "CUBE_LOOP_INSTRUCTION_ID=${access_id} CUBE_LOOP_EXPECTED_GM_FOOTPRINT_BYTES=480 CUBE_LOOP_ALLOCATED_GM_BYTES=${expected_allocation}([[:space:]]|$)" \
            'cube outer-loop sample footprint contract' "${output}" || return 1
        RequirePatternCount \
            "\\[raw\\].*instrId=${access_id}([[:space:]]|$)" 1 \
            "one active-loop ID ${access_id} access" "${output}" || return 1

        case "${access_id}" in
            73) data_bits=0 ;;
            74) data_bits=8 ;;
            75) data_bits=16 ;;
            76) data_bits=32 ;;
        esac
        RequirePattern \
            "\\[cbdata\\] type=AclsanDeviceMemoryAccessData .*dataBits=${data_bits} .*layoutKind=4([[:space:]]|$)" \
            'outer-loop ND-affine GM layout' "${output}" || return 1
        if [[ ${access_id} -eq 73 ]]; then
            RequirePattern \
                '\[param\] type=CopyGmToCbufV2ParamField .*burstNum=1 burstLen=1 padFunctionMode=0 .*srcStride=0([[:space:]]|$)' \
                'ID 73 active-loop inner access fields' "${output}" || return 1
        else
            RequirePattern \
                "\\[param\\] type=CopyGmToCbufAlignV2ParamField instrId=${access_id} .*burstNum=1 burstLen=32 .*burstSrcStride=0 " \
                "ID ${access_id} active-loop inner access fields" "${output}" || return 1
        fi
    fi
}

CheckFixpipeOutput()
{
    local case_name=$1
    local output=$2
    if [[ "${case_name}" == fixpipe_quant_* ]]; then
        local quant_case=${case_name#fixpipe_quant_}
        quant_case=${quant_case%_valid}
        quant_case=${quant_case%_oob}
        local expected_quant=0
        local expected_bits=32
        local expected_records=1
        local expected_bytes
        expected_bytes=$(sed -n 's/.*FIXPIPE_EXPECTED_GM_BYTES=\([0-9][0-9]*\).*/\1/p' "${output}" | head -1)
        case "${quant_case}" in
            q*)
                expected_quant=${quant_case#q}
                expected_quant=${expected_quant%%_*}
                ;;
            relu_pre | relu_scalar | relu_vector | unit_keep | unit_update | clip_relu_pre)
                expected_quant=1
                ;;
            channel_merge_tail)
                expected_quant=24
                ;;
            b4_channel_merge_two_group)
                expected_quant=26
                expected_records=2
                ;;
            b8_f32_nz2nd) expected_quant=24 ;;
            b8_s32_nz2dn) expected_quant=9 ;;
            b4_f32_nz2nd) expected_quant=26; expected_records=32 ;;
            b4_s32_nz2dn) expected_quant=22; expected_records=32 ;;
        esac
        case "${quant_case}" in
            q2[1-2] | q2[5-6]) expected_bits=4 ;;
            b4_channel_merge_two_group | b4_f32_nz2nd | b4_s32_nz2dn) expected_bits=4 ;;
            q2 | q3 | q4 | q5 | q8 | q9 | q12 | q13 | q23 | q24 | channel_merge_tail | \
                b8_f32_nz2nd | b8_s32_nz2dn) expected_bits=8 ;;
            q1 | q10 | q11 | q16 | q31 | q32 | q33 | q34 | q35 | q36 | relu_pre | relu_scalar | relu_vector | unit_keep | unit_update | clip_relu_pre)
                expected_bits=16
                ;;
        esac
        if [[ "${quant_case}" == channel_merge_tail ]]; then
            expected_records=2
        fi
        RequirePattern \
            "\[param\] type=FixL0cToOutParamField instrId=$(ExpectedInstructionId "${case_name}") dataBits=32 .*quantPre=${expected_quant} " \
            'decoded Fixpipe source intrinsic and quantization fields' "${output}" || return 1
        RequirePatternCount \
            "\[cbdata\] type=AclsanDeviceMemoryAccessData .*dataBits=${expected_bits} " \
            "${expected_records}" 'Fixpipe quantization cbdata records' "${output}" || return 1
        if [[ "${quant_case}" == b4_channel_merge_two_group ]]; then
            RequirePatternCount '\[cbdata\] layout=range bytes=512([[:space:]]|$)' 2 \
                'B4 channel-merge full groups' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=0 accessCount=2 ' \
                'B4 channel-merge first-group index' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=1 accessCount=2 ' \
                'B4 channel-merge second-group index' "${output}" || return 1
            CheckCbdataAddressDelta "${output}" 600 || return 1
        elif [[ "${quant_case}" == b8_f32_nz2nd || "${quant_case}" == b8_s32_nz2dn ]]; then
            RequirePattern \
                '\[cbdata\] layout=block_repeat blockNum=1 blockSize=16 blockStride=0 repeatTimes=32 repeatStride=32([[:space:]]|$)' \
                'B8 conversion exact strided GM footprint' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=0 accessCount=1 ' \
                'B8 conversion single-footprint index' "${output}" || return 1
            CheckFirstCbdataAddressMatchesFixpipeDst "${output}" || return 1
        elif [[ "${quant_case}" == b4_f32_nz2nd || "${quant_case}" == b4_s32_nz2dn ]]; then
            RequirePatternCount '\[cbdata\] layout=range bytes=8([[:space:]]|$)' 32 \
                'B4 conversion nibble-accurate GM segments' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=0 accessCount=32 ' \
                'B4 conversion first segment index' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=31 accessCount=32 ' \
                'B4 conversion last segment index' "${output}" || return 1
            CheckB4ConversionCbdataAddresses "${output}" || return 1
        elif [[ ${expected_records} -eq 1 ]]; then
            RequirePattern "\[cbdata\] layout=range bytes=${expected_bytes}([[:space:]]|$)" \
                'exact quantized Fixpipe GM footprint' "${output}" || return 1
        elif [[ "${quant_case}" == channel_merge_tail ]]; then
            RequirePattern '\[cbdata\] layout=range bytes=512([[:space:]]|$)' \
                'B8 channel-merge full group' "${output}" || return 1
            RequirePattern '\[cbdata\] layout=range bytes=256([[:space:]]|$)' \
                'B8 channel-merge tail group' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=0 accessCount=2 ' \
                'B8 channel-merge full-group index' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] type=AclsanDeviceMemoryAccessData .*accessIndex=1 accessCount=2 ' \
                'B8 channel-merge tail-group index' "${output}" || return 1
            CheckCbdataAddressDelta "${output}" 1024 || return 1
        else
            RequirePattern '\[cbdata\] layout=range bytes=1024([[:space:]]|$)' \
                'NZ full group' "${output}" || return 1
            RequirePattern '\[cbdata\] layout=range bytes=64([[:space:]]|$)' \
                'NZ tail group' "${output}" || return 1
        fi
        case "${quant_case}" in
            b8_f32_nz2nd | b4_f32_nz2nd)
                RequirePattern '\[param\] type=Loop3ParamField loopCount=2 srcStride=16 dstStride=512' \
                    'quantized NZ2ND LOOP3 state' "${output}" || return 1
                RequirePattern \
                    '\[param\] type=FixL0cToOutParamField .*nz2ndEnable=1 .*nz2dnEnable=0([[:space:]]|$)' \
                    'quantized NZ2ND Fixpipe fields' "${output}" || return 1
                ;;
            b8_s32_nz2dn | b4_s32_nz2dn)
                RequirePattern '\[param\] type=Loop3ParamField loopCount=2 srcStride=16 dstStride=512' \
                    'quantized NZ2DN LOOP3 state' "${output}" || return 1
                RequirePattern \
                    '\[param\] type=FixL0cToOutParamField .*nz2ndEnable=0 .*nz2dnEnable=1([[:space:]]|$)' \
                    'quantized NZ2DN Fixpipe fields' "${output}" || return 1
                ;;
        esac
        case "${quant_case}" in
            relu_pre)
                RequirePattern 'clipReluPre=0 unitFlag=0 .*reluPre=1 ' \
                    'Fixpipe ReLU-pre field' "${output}" || return 1
                ;;
            relu_scalar)
                RequirePattern 'clipReluPre=0 unitFlag=0 .*reluPre=2 ' \
                    'Fixpipe scalar ReLU-pre field' "${output}" || return 1
                ;;
            relu_vector)
                RequirePattern 'clipReluPre=0 unitFlag=0 .*reluPre=3 ' \
                    'Fixpipe vector ReLU-pre field' "${output}" || return 1
                RequirePattern '\[param\] type=LocalMemoryTransferParamField instrId=167 .*localOnly=1' \
                    'decoded vector ReLU ID 167 L1-to-FBUF intrinsic' "${output}" || return 1
                RequirePattern '\[cbdata\] no GM access for local-only memory instruction instrId=167' \
                    'explicit vector ReLU ID 167 no-GM result' "${output}" || return 1
                ;;
            unit_keep)
                RequirePattern 'clipReluPre=0 unitFlag=2 .*reluPre=0 ' \
                    'Fixpipe unit-flag field' "${output}" || return 1
                ;;
            unit_update)
                RequirePattern 'clipReluPre=0 unitFlag=3 .*reluPre=0 ' \
                    'Fixpipe unit-flag update field' "${output}" || return 1
                ;;
            clip_relu_pre)
                RequirePattern 'clipReluPre=1 unitFlag=0 .*reluPre=1 ' \
                    'Fixpipe clip-ReLU-pre field' "${output}" || return 1
                ;;
        esac
        case "${quant_case}" in
            q2 | q4 | q8 | q10 | q12 | q14 | q21 | q23 | q25 | q31 | q33 | q35)
                RequirePattern '\[param\] type=LocalMemoryTransferParamField instrId=167 .*localOnly=1' \
                    'decoded ID 167 L1-to-FBUF intrinsic' "${output}" || return 1
                RequirePattern '\[cbdata\] no GM access for local-only memory instruction instrId=167' \
                    'explicit ID 167 no-GM result' "${output}" || return 1
                RejectPattern 'unsupported raw trace.*instrId=167' \
                    'unsupported ID 167 local-memory trace' "${output}" || return 1
                ;;
        esac
        return 0
    fi

    if [[ "${case_name}" == fixpipe_channel_split_* ]]; then
        local expected_n_size=8
        local expected_bytes=512
        local expected_allocated_bytes=512
        if [[ "${case_name}" == fixpipe_channel_split_n16_* ]]; then
            expected_n_size=16
            expected_bytes=1024
            expected_allocated_bytes=1024
        fi
        local expected_result=normal
        if [[ "${case_name}" == *_oob ]]; then
            expected_allocated_bytes=$((expected_allocated_bytes - 32))
            expected_result=oob
        fi
        RequirePattern \
            "FIXPIPE_CHANNEL_SPLIT_N_SIZE=${expected_n_size} FIXPIPE_CHANNEL_SPLIT_EXPECTED_GM_BYTES=${expected_bytes} FIXPIPE_CHANNEL_SPLIT_ALLOCATED_GM_BYTES=${expected_allocated_bytes} FIXPIPE_CHANNEL_SPLIT_EXPECTED_RESULT=${expected_result}([[:space:]]|$)" \
            'selected supported basic API channel-split profile' "${output}" || return 1
        RequirePattern \
            "\[param\] type=FixL0cToOutParamField instrId=91 dataBits=32 .*nSize=${expected_n_size} mSize=16 loopDstStride=128 loopSrtStride=1 .*quantPre=0 reluPre=0 splitEnable=1 nz2ndEnable=0 .*nz2dnEnable=0([[:space:]]|$)" \
            'basic API channel-split intrinsic fields' "${output}" || return 1
        RequirePattern "\[cbdata\] layout=range bytes=${expected_bytes}([[:space:]]|$)" \
            'exact supported channel-split GM footprint' "${output}" || return 1
        return 0
    fi

    if [[ "${case_name}" == fixpipe_c0pad_* ]]; then
        local expected_bytes
        expected_bytes=$(sed -n 's/.*FIXPIPE_C0PAD_EXPECTED_GM_BYTES=\([0-9][0-9]*\).*/\1/p' "${output}" | head -1)
        RequirePattern \
            '\[param\] type=FixL0cToOutParamField instrId=91 dataBits=32 .*sid=0 .*mSize=16 loopDstStride=256 loopSrtStride=16 .*quantPre=0 reluPre=0 splitEnable=0 nz2ndEnable=0 .*c0PadEnable=1 .*nz2dnEnable=0([[:space:]]|$)' \
            'decoded DumpTensor C0-pad Fixpipe fields' "${output}" || return 1
        RejectPattern '\[raw\].*instrId=90([[:space:]]|$)' \
            'unexpected LOOP3 state for DumpTensor C0-pad NZ path' "${output}" || return 1
        RequirePattern "\[cbdata\] layout=range bytes=${expected_bytes}([[:space:]]|$)" \
            'exact C0-pad rounded GM footprint' "${output}" || return 1
        return 0
    fi

    if [[ "${case_name}" == *nz2nd* || "${case_name}" == *nz2dn* ]]; then
        local instruction_id
        local expected_block_size=32
        local expected_repeat_stride=64
        instruction_id=$(ExpectedInstructionId "${case_name}")
        if [[ "${case_name}" == fixpipe_s32_* ]]; then
            expected_block_size=64
            expected_repeat_stride=128
        fi
        RequirePattern '\[raw\].*instrId=90([[:space:]]|$)' 'ID 90 Fixpipe loop3 state' "${output}" || return 1
        RequirePattern \
            '\[param\] type=Loop3ParamField loopCount=2 srcStride=16 dstStride=512([[:space:]]|$)' \
            'decoded Fixpipe two-matrix loop3 state' "${output}" || return 1
        CheckOrderedPatterns '\[raw\].*instrId=90([[:space:]]|$)' \
            "\\[raw\\].*instrId=${instruction_id}([[:space:]]|$)" \
            "ID 90 state before ID ${instruction_id} access" "${output}" || return 1
        RequirePattern \
            "\\[cbdata\\] layout=block_repeat blockNum=1 blockSize=${expected_block_size} blockStride=0 repeatTimes=32 repeatStride=${expected_repeat_stride}([[:space:]]|$)" \
            'Fixpipe two-matrix GM destination footprint' "${output}" || return 1
        return 0
    fi

    local expected_bytes=512
    if [[ "${case_name}" == fixpipe_s32_* ]]; then
        expected_bytes=1024
    fi
    RequirePattern "\\[cbdata\\] layout=range bytes=${expected_bytes}([[:space:]]|$)" \
        'exact Fixpipe GM destination footprint' "${output}" || return 1
}

CheckApiDataCopyOutput()
{
    local case_name=$1
    local output=$2

    case "${case_name}" in
        api_datacopypad_gm2ub_*)
            CheckOrderedPatterns '\[raw\].*instrId=128([[:space:]]|$)' \
                '\[raw\].*instrId=84([[:space:]]|$)' 'ID 128 state before API-lowered ID 84' "${output}" || return 1
            RequirePattern \
                '\[param\] type=CopyGmToUbufAlignV2ParamField .*burstNum=3 burstLen=47 .*burstSrcStride=64 ' \
                'DataCopyPad GM-to-UB intrinsic fields' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] layout=block_repeat .*blockSize=47 .*repeatTimes=3 repeatStride=64([[:space:]]|$)' \
                'DataCopyPad GM-to-UB exact GM footprint' "${output}" || return 1
            ;;
        api_datacopypad_ub2gm_*)
            CheckOrderedPatterns '\[raw\].*instrId=125([[:space:]]|$)' \
                '\[raw\].*instrId=83([[:space:]]|$)' 'ID 125 state before API-lowered ID 83' "${output}" || return 1
            RequirePattern \
                '\[param\] type=CopyUbufToGmAlignV2ParamField .*burstNum=3 burstLen=47 .*dstStride=64 ' \
                'DataCopyPad UB-to-GM intrinsic fields' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] layout=block_repeat .*blockSize=47 .*repeatTimes=3 repeatStride=64([[:space:]]|$)' \
                'DataCopyPad UB-to-GM exact GM footprint' "${output}" || return 1
            ;;
        api_datacopy_nd2nz_*)
            CheckOrderedPatterns '\[raw\].*instrId=128([[:space:]]|$)' \
                '\[raw\].*instrId=84([[:space:]]|$)' 'ID 128 state before software-lowered ID 84' "${output}" || return 1
            RequirePatternCount '\[raw\].*instrId=84([[:space:]]|$)' 2 \
                'ND2NZ ID 84 records' "${output}" || return 1
            RequirePattern \
                '\[param\] type=CopyGmToUbufAlignV2ParamField .*burstNum=3 burstLen=32 .*burstSrcStride=64 ' \
                'ND2NZ full-C0 ID 84 fields' "${output}" || return 1
            RequirePattern \
                '\[param\] type=CopyGmToUbufAlignV2ParamField .*burstNum=3 burstLen=15 .*burstSrcStride=64 ' \
                'ND2NZ tail ID 84 fields' "${output}" || return 1
            RequirePattern '^tool=memcheck .*device_operations=2 ' \
                'two ND2NZ device operations' "${output}" || return 1
            ;;
        api_datacopy_nz2nd_*)
            CheckOrderedPatterns '\[raw\].*instrId=125([[:space:]]|$)' \
                '\[raw\].*instrId=83([[:space:]]|$)' 'ID 125 state before software-lowered ID 83' "${output}" || return 1
            RequirePatternCount '\[raw\].*instrId=83([[:space:]]|$)' 2 \
                'NZ2ND ID 83 records' "${output}" || return 1
            RequirePatternCount \
                '\[param\] type=CopyUbufToGmAlignV2ParamField .*burstNum=3 burstLen=16 .*dstStride=64 ' 2 \
                'NZ2ND ID 83 fields' "${output}" || return 1
            RequirePattern '^tool=memcheck .*device_operations=2 ' \
                'two NZ2ND device operations' "${output}" || return 1
            ;;
    esac
}

CheckApiGmToLocalOutput()
{
    local case_name=$1
    local output=$2

    case "${case_name}" in
        api_load_data_*)
            if [[ "${case_name}" == api_load_data_2d_* ]]; then
                RequirePattern '\[raw\].*instrId=124([[:space:]]|$)' \
                    'ID 124 MTE2 source state' "${output}" || return 1
                RequirePattern '\[param\] type=Mte2SourceParamField srcStride=3([[:space:]]|$)' \
                    'LoadData 2D source stride' "${output}" || return 1
                RequirePattern \
                    '\[param\] type=LoadGmToCbuf2DV2ParamField .*mStartPosition=0 kStartPosition=1 dstStride=1 mStep=3 kStep=2 .*decompMode=0([[:space:]]|$)' \
                    'LoadData 2D lowered ID 72 fields' "${output}" || return 1
                RequirePattern '\[cbdata\] layout=range bytes=3072([[:space:]]|$)' \
                    'LoadData 2D exact contiguous GM footprint' "${output}" || return 1
                CheckOrderedPatterns '\[raw\].*instrId=124([[:space:]]|$)' \
                    '\[raw\].*instrId=72([[:space:]]|$)' 'ID 124 state before ID 72 access' "${output}" || return 1
            else
                CheckLoad2dOutput "${case_name}" "${output}" || return 1
            fi
            ;;
        api_datacopypad_gm2l1_*)
            RequirePattern \
                '\[param\] type=CopyGmToCbufAlignV2ParamField .*burstNum=3 burstLen=48 .*burstSrcStride=64 ' \
                'DataCopyPad GM-to-L1 intrinsic fields' "${output}" || return 1
            RequirePattern \
                '\[cbdata\] layout=block_repeat .*blockSize=48 .*repeatTimes=3 repeatStride=64([[:space:]]|$)' \
                'DataCopyPad GM-to-L1 exact GM footprint' "${output}" || return 1
            ;;
        api_nddma_b64_*)
            local expected_records=1
            if [[ "${case_name}" == api_nddma_b64_5d_* ]]; then
                expected_records=2
            fi

            local state_id
            for state_id in 131 132 133 134 135 136; do
                RequirePatternCount "\\[raw\\].*instrId=${state_id}([[:space:]]|$)" "${expected_records}" \
                    "b64 NDDMA ID ${state_id} records" "${output}" || return 1
            done
            RequirePatternCount '\[raw\].*instrId=89([[:space:]]|$)' "${expected_records}" \
                'b64-lowered ID 89 records' "${output}" || return 1
            RequirePatternCount \
                '\[param\] type=NdDmaPadCountParamField left=\[0,0,0,0\] right=\[0,0,0,0\]([[:space:]]|$)' \
                "${expected_records}" 'b64 NDDMA zero-padding state' "${output}" || return 1

            local expected_strides=(1 2 8 32 128)
            if [[ "${case_name}" == api_nddma_b64_5d_* ]]; then
                expected_strides=(2 8 32 128 512)
            fi
            local loop_index
            for loop_index in 0 1 2 3 4; do
                RequirePatternCount \
                    "\\[param\\] type=NdDmaLoopStrideParamField loopIndex=${loop_index} srcStride=${expected_strides[loop_index]}([[:space:]]|$)" \
                    "${expected_records}" "b64 NDDMA loop ${loop_index} stride" "${output}" || return 1
            done
            RequirePatternCount \
                '\[param\] type=NdDmaParamField instrId=89 dataBits=32 .*loopSizes=\[2,2,2,2,2\] ' \
                "${expected_records}" 'b64 API lowered ID 89 fields' "${output}" || return 1
            RequirePattern "^tool=memcheck .*device_operations=${expected_records} " \
                'b64 NDDMA device operation count' "${output}" || return 1

            if [[ ${expected_records} -eq 2 ]]; then
                local source_addresses=()
                mapfile -t source_addresses < <(
                    grep -E '\[param\] type=NdDmaParamField instrId=89 ' "${output}" |
                        sed -E 's/.* srcAddr=(0x[0-9a-fA-F]+).*/\1/'
                )
                if [[ ${#source_addresses[@]} -ne 2 ||
                    $((source_addresses[1] - source_addresses[0])) -ne 4 ]]; then
                    printf 'unexpected b64 5D split source addresses\n' >&2
                    return 1
                fi
            fi
            ;;
    esac
}

CheckCaseOutput()
{
    local case_name=$1
    local output=$2

    if [[ "${case_name}" == nddma_missing_active_stride ]]; then
        CheckNdDmaMissingStrideOutput "${output}" || return 1
        return 0
    fi
    CheckCommonOutput "${case_name}" "${output}" || return 1
    if [[ "${case_name}" == local_ub_to_l1_valid ]]; then
        RequirePattern '\[raw\].*instrId=173([[:space:]]|$)' 'raw ID 173 MOV_UB_TO_L1' "${output}" || return 1
        RequirePattern '\[param\] type=LocalMemoryTransferParamField instrId=173 .*localOnly=1' \
            'decoded ID 173 local-memory transfer' "${output}" || return 1
        RequirePattern '\[cbdata\] no GM access for local-only memory instruction instrId=173' \
            'explicit ID 173 no-GM result' "${output}" || return 1
        RejectPattern 'unsupported raw trace.*instrId=173' 'unsupported ID 173 trace' "${output}" || return 1
    fi
    case "${case_name}" in
        load2dv2_*)
            CheckLoad2dOutput "${case_name}" "${output}" || return 1
            ;;
        nddma_p0_*)
            CheckNdDmaP0Output "${case_name}" "${output}" || return 1
            ;;
        nddma_padding_*)
            CheckNdDmaPaddingOutput "${case_name}" "${output}" || return 1
            ;;
        nddma_*)
            CheckNdDmaOutput "${case_name}" "${output}" || return 1
            ;;
        gm_to_ub_outer_loop_* | ub_to_gm_outer_loop_* | cube_gm_to_l1_outer_loop_* | \
            cube_gm_to_l1_id7[3-6]_outer_loop_*)
            CheckDmaOuterLoopOutput "${case_name}" "${output}" || return 1
            ;;
        cube_gm_to_l1_id*)
            CheckCubeDmaOutput "${case_name}" "${output}" || return 1
            ;;
        multi_*)
            CheckMultiOutput "${case_name}" "${output}" || return 1
            ;;
        fixpipe_*)
            CheckFixpipeOutput "${case_name}" "${output}" || return 1
            ;;
        api_datacopy* | api_datacopypad_gm2ub_* | api_datacopypad_ub2gm_*)
            CheckApiDataCopyOutput "${case_name}" "${output}" || return 1
            ;;
        api_load_data_* | api_datacopypad_gm2l1_* | api_nddma_*)
            CheckApiGmToLocalOutput "${case_name}" "${output}" || return 1
            ;;
    esac
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
    grep -Eq '^callbacks=0 malformed_callbacks=0 framework_errors=0 dropped_messages=0 ' "${output}" || return 1
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
