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

synccheck_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
demo_dir=$(cd "${synccheck_dir}/../.." && pwd)
if [[ ! -x "${demo_dir}/build.sh" ]]; then
    printf 'missing demo build script: %s/build.sh\n' "${demo_dir}" >&2
    exit 1
fi
if [[ ! -x "${demo_dir}/build/npu_tools/bin/npu_check" ]]; then
    printf 'demo tools are not built; run %s/build.sh first\n' "${demo_dir}" >&2
    exit 1
fi

examples=(
    no_sync
    multi_launch_unconsumed
    multi_launch_pairs
    flag_set_set_wait_wait
    flag_mutex_error_bundle
    mix_wait_without_set
    mutex_pair
    mix_mutex_unreleased
    split_wrong_side_mutex_noop
    mutex_multi_block_isolation
)
temporary_dir=$(mktemp -d)
trap 'rm -rf -- "${temporary_dir}"' EXIT

passed_examples=()
failed_examples=()
for example in "${examples[@]}"; do
    output_file="${temporary_dir}/${example}.log"
    printf '\n===== RUN %s =====\n' "${example}"
    bash "${synccheck_dir}/${example}/run.sh" 2>&1 | tee "${output_file}"
    run_status=${PIPESTATUS[0]}

    if [[ ${run_status} -eq 0 ]] &&
        grep -Fqx "example verification passed: synccheck/${example}" "${output_file}"; then
        passed_examples+=("${example}")
    else
        failed_examples+=("${example} (status=${run_status})")
    fi
done

printf '\n===== SYNCCHECK SMOKE RESULTS =====\n'
for example in "${passed_examples[@]}"; do
    printf 'PASS  %s\n' "${example}"
done
for result in "${failed_examples[@]}"; do
    printf 'FAIL  %s\n' "${result}"
done

passed_count=${#passed_examples[@]}
failed_count=${#failed_examples[@]}
printf '\n===== SYNCCHECK SMOKE SUMMARY =====\n'
printf 'total=%d passed=%d failed=%d\n' "${#examples[@]}" "${passed_count}" "${failed_count}"

if ((failed_count != 0)); then
    exit 1
fi
