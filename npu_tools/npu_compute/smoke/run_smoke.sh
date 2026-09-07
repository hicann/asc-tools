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

npu_compute_suite_smoke_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${npu_compute_suite_smoke_dir}/build"
examples_dir="${npu_compute_suite_smoke_dir}/examples"
smoke_log_dir="${build_dir}/logs"

run_smoke_examples()
{
    local case_name
    local log_file
    local run_status
    local runner
    local errexit_was_set=0
    local -a pipeline_status
    local -a passed_cases=()
    local -a failed_cases=()

    [[ $- == *e* ]] && errexit_was_set=1
    mkdir -p "${smoke_log_dir}"
    for case_name in "$@"; do
        runner="${examples_dir}/${case_name}/run.sh"
        log_file="${smoke_log_dir}/${case_name}.log"
        printf '\n===== RUN %s =====\n' "${case_name}"

        if [[ ! -x "${runner}" ]]; then
            printf 'missing executable case runner: %s\n' "${runner}" | tee "${log_file}" >&2
            failed_cases+=("${case_name} (status=1)")
            continue
        fi

        set +e
        bash "${runner}" 2>&1 | tee "${log_file}"
        pipeline_status=("${PIPESTATUS[@]}")
        if (( errexit_was_set )); then
            set -e
        fi
        run_status=${pipeline_status[0]}
        if (( pipeline_status[1] != 0 )); then
            run_status=${pipeline_status[1]}
        fi
        if (( run_status == 0 )); then
            passed_cases+=("${case_name}")
        else
            failed_cases+=("${case_name} (status=${run_status})")
        fi
    done

    printf '\n===== NPU COMPUTE SMOKE RESULTS =====\n'
    for case_name in "${passed_cases[@]}"; do
        printf 'PASS  %s\n' "${case_name}"
    done
    for case_name in "${failed_cases[@]}"; do
        printf 'FAIL  %s\n' "${case_name}"
    done

    local passed_count=${#passed_cases[@]}
    local failed_count=${#failed_cases[@]}
    local total_count=$((passed_count + failed_count))
    printf '\n===== NPU COMPUTE SMOKE SUMMARY =====\n'
    printf 'total=%d passed=%d failed=%d\n' "${total_count}" "${passed_count}" "${failed_count}"
    (( failed_count == 0 ))
}

main()
{
    local skip_asc_tools_build=0
    local canonical_cli
    local resolved_cli
    case $# in
        0)
            ;;
        1)
            if [[ $1 == --skip-asc-tools-build ]]; then
                skip_asc_tools_build=1
            else
                printf 'usage: %s [--skip-asc-tools-build]\n' "${BASH_SOURCE[0]}" >&2
                return 2
            fi
            ;;
        *)
            printf 'usage: %s [--skip-asc-tools-build]\n' "${BASH_SOURCE[0]}" >&2
            return 2
            ;;
    esac

    if (( ! skip_asc_tools_build )); then
        source "${npu_compute_suite_smoke_dir}/build.sh"
    elif [[ -z "${NPU_COMPUTE_SMOKE_CLI:-}" ]]; then
        if ! resolved_cli=$(command -v npu-compute); then
            printf 'npu-compute is not available in PATH; omit --skip-asc-tools-build to build and install asc-tools\n' \
                >&2
            return 1
        fi
        NPU_COMPUTE_SMOKE_CLI=${resolved_cli}
    fi

    if [[ "${NPU_COMPUTE_SMOKE_CLI}" != /* || ! -x "${NPU_COMPUTE_SMOKE_CLI}" ]]; then
        printf 'NPU_COMPUTE_SMOKE_CLI must name an absolute executable: %s\n' \
            "${NPU_COMPUTE_SMOKE_CLI}" >&2
        return 1
    fi
    NPU_COMPUTE_SMOKE_CLI=$(realpath -e -- "${NPU_COMPUTE_SMOKE_CLI}")
    export NPU_COMPUTE_SMOKE_CLI

    export PATH="$(dirname -- "${NPU_COMPUTE_SMOKE_CLI}"):${PATH}"
    hash -r
    if ! resolved_cli=$(command -v npu-compute); then
        printf 'npu-compute is not available in PATH\n' >&2
        return 2
    fi
    if [[ "${resolved_cli}" != /* || ! -x "${resolved_cli}" ]]; then
        printf 'npu-compute resolves to a non-file command instead of the selected executable: %s\n' \
            "${resolved_cli}" >&2
        return 1
    fi
    canonical_cli=$(realpath -e -- "${resolved_cli}")
    if [[ "${canonical_cli}" != "${NPU_COMPUTE_SMOKE_CLI}" ]]; then
        printf 'npu-compute resolves to %s instead of the installed executable %s\n' \
            "${canonical_cli}" "${NPU_COMPUTE_SMOKE_CLI}" >&2
        return 1
    fi
    bash -c 'source "$1"; discover_sections' _ "${npu_compute_suite_smoke_dir}/run_case.sh"

    local -a cases=(vector_add cube_mmad mix_1_1 mix_1_2 reg_add simt_hello)
    run_smoke_examples "${cases[@]}"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    set -uo pipefail
    main "$@"
fi
