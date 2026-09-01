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

set -uo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${demo_dir}/build"
examples_dir="${demo_dir}/examples"
smoke_log_dir="${build_dir}/smoke"

run_smoke_examples()
{
    local example
    local log_file
    local run_status
    local runner
    local errexit_was_set=0
    local -a pipeline_status
    local -a passed_examples=()
    local -a failed_examples=()

    [[ $- == *e* ]] && errexit_was_set=1
    mkdir -p "${smoke_log_dir}"
    for example in "$@"; do
        runner="${examples_dir}/${example}/run.sh"
        log_file="${smoke_log_dir}/${example}.log"
        mkdir -p "$(dirname "${log_file}")"
        printf '\n===== RUN %s =====\n' "${example}"

        if [[ ! -x "${runner}" ]]; then
            printf 'missing executable example runner: %s\n' "${runner}" | tee "${log_file}" >&2
            failed_examples+=("${example} (status=1)")
            continue
        fi

        set +e
        bash "${runner}" 2>&1 | tee "${log_file}"
        pipeline_status=("${PIPESTATUS[@]}")
        if ((errexit_was_set)); then
            set -e
        fi
        run_status=${pipeline_status[0]}
        if ((pipeline_status[1] != 0)); then
            run_status=${pipeline_status[1]}
        fi

        if [[ ${run_status} -eq 0 ]]; then
            passed_examples+=("${example}")
        else
            failed_examples+=("${example} (status=${run_status})")
        fi
    done

    printf '\n===== DEMO SMOKE RESULTS =====\n'
    for example in "${passed_examples[@]}"; do
        printf 'PASS  %s\n' "${example}"
    done
    for example in "${failed_examples[@]}"; do
        printf 'FAIL  %s\n' "${example}"
    done

    local passed_count=${#passed_examples[@]}
    local failed_count=${#failed_examples[@]}
    local total_count=$((passed_count + failed_count))
    printf '\n===== DEMO SMOKE SUMMARY =====\n'
    printf 'total=%d passed=%d failed=%d\n' "${total_count}" "${passed_count}" "${failed_count}"

    if ((failed_count != 0)); then
        return 1
    fi
}

main()
{
    if [[ $# -ne 0 ]]; then
        printf 'usage: %s\n' "${BASH_SOURCE[0]}" >&2
        return 2
    fi

    # 每次全量冒烟都从干净的公共工具构建目录开始，并复用 build.sh 的 CANN 环境。
    rm -rf -- "${build_dir}"
    source "${demo_dir}/build.sh"

    if [[ ! -x "${demo_dir}/build/npu_tools/bin/npu_check" ]]; then
        printf 'demo build did not produce npu_check: %s\n' \
            "${demo_dir}/build/npu_tools/bin/npu_check" >&2
        return 1
    fi

    local -a examples=(
        memcheck/add
        memcheck/datacopy_stride
        memcheck/memory_access
        memcheck/matmul_basic_api
        memcheck/matmul_leakyrelu_basic_api
        basic_func/multi_kernel
        basic_func/padding_register_state
        basic_func/dual_tool_multi_launch_aggregate
        synccheck/no_sync
        synccheck/multi_launch_unconsumed
        synccheck/multi_launch_pairs
        synccheck/flag_set_set_wait_wait
        synccheck/flag_mutex_error_bundle
        synccheck/mix_wait_without_set
        synccheck/mutex_pair
        synccheck/mix_mutex_unreleased
        synccheck/split_wrong_side_mutex_noop
        synccheck/mutex_multi_block_isolation
    )
    run_smoke_examples "${examples[@]}"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    main "$@"
fi
