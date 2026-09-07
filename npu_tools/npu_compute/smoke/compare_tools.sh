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

smoke_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
readonly smoke_dir
readonly cases=(vector_add cube_mmad mix_1_1 mix_1_2 reg_add simt_hello)
readonly sections=(PipeUtilization Memory MemoryL0 MemoryUB L2Cache)

usage() {
    cat <<'EOF'
Usage: bash npu_tools/npu_compute/smoke/compare_tools.sh \
  --npu-compute-env <set_env.sh> --msopprof-env <set_env.sh> [--output <directory>]

Builds and profiles every fixed smoke sample in the supplied npu-compute and
msopprof CANN environments, then writes a report-only comparison. Metric
differences are recorded but never make the command fail.
EOF
}

output_dir="${smoke_dir}/build/compare-tools-$(date +%Y%m%d%H%M%S)"
npu_compute_env=
msopprof_env=
while (( $# > 0 )); do
    case "$1" in
        --npu-compute-env)
            if (( $# < 2 )); then
                printf '%s requires a set_env.sh path\n' "$1" >&2
                exit 2
            fi
            npu_compute_env=$2
            shift 2
            ;;
        --msopprof-env)
            if (( $# < 2 )); then
                printf '%s requires a set_env.sh path\n' "$1" >&2
                exit 2
            fi
            msopprof_env=$2
            shift 2
            ;;
        --output)
            if (( $# < 2 )); then
                printf '%s requires a directory\n' "$1" >&2
                exit 2
            fi
            output_dir=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

output_dir=$(realpath -m "${output_dir}")
if [[ -e "${output_dir}" ]]; then
    printf 'comparison output already exists: %s\n' "${output_dir}" >&2
    exit 2
fi
if [[ -z "${npu_compute_env}" || -z "${msopprof_env}" ]]; then
    printf '%s\n' '--npu-compute-env and --msopprof-env are required' >&2
    exit 2
fi
for environment in "${npu_compute_env}" "${msopprof_env}"; do
    if [[ ! -f "${environment}" ]]; then
        printf 'required CANN environment script is missing: %s\n' "${environment}" >&2
        exit 2
    fi
done
npu_compute_env=$(realpath -e "${npu_compute_env}")
msopprof_env=$(realpath -e "${msopprof_env}")
readonly npu_compute_env
readonly msopprof_env
if [[ ! -f "${smoke_dir}/compare_reports.py" ]]; then
    printf 'comparison parser is missing: %s\n' "${smoke_dir}/compare_reports.py" >&2
    exit 2
fi

mkdir -p "${output_dir}/logs"
readonly output_dir

run_in_environment() {
    local environment=$1
    local working_directory=$2
    shift 2
    bash -c 'source "$1"; shift; cd "$1"; shift; exec "$@"' bash "${environment}" "${working_directory}" "$@"
}

run_logged() {
    local environment=$1
    local working_directory=$2
    local log_file=$3
    shift 3
    run_in_environment "${environment}" "${working_directory}" "$@" > "${log_file}" 2>&1
}

copy_collection_data() {
    local collection_log=$1
    local raw_output_dir=$2
    local -a data_directories=()

    mapfile -t data_directories < <(sed -n 's/^npu-compute: data-directory=//p' "${collection_log}")
    if (( ${#data_directories[@]} != 1 )); then
        printf 'expected exactly one npu-compute data directory in %s, found %d\n' \
            "${collection_log}" "${#data_directories[@]}" >&2
        return 1
    fi
    if [[ ! -d "${data_directories[0]}" ]]; then
        printf 'npu-compute data directory is missing: %s\n' "${data_directories[0]}" >&2
        return 1
    fi
    cp -a "${data_directories[0]}/." "${raw_output_dir}/"
}

collector_failures=0
for case_name in "${cases[@]}"; do
    case_dir="${smoke_dir}/examples/${case_name}"
    npu_build_dir="${output_dir}/build/npu-compute/${case_name}"
    msopprof_build_dir="${output_dir}/build/msopprof/${case_name}"
    npu_output_dir="${output_dir}/npu-compute/${case_name}"
    npu_raw_dir="${npu_output_dir}/raw"
    npu_import_dir="${npu_output_dir}/imported"
    npu_comparison_root="${npu_raw_dir}"
    msopprof_output_dir="${output_dir}/msopprof/${case_name}"
    comparison_output_dir="${output_dir}/comparison/${case_name}"
    npu_ready=1
    msopprof_ready=1

    printf '[%s] build with npu-compute CANN\n' "${case_name}"
    if ! run_logged "${npu_compute_env}" "${smoke_dir}" "${output_dir}/logs/${case_name}.npu-compute.build.log" \
        cmake -S "${case_dir}" -B "${npu_build_dir}" -DCMAKE_ASC_ARCHITECTURES=dav-3510; then
        npu_ready=0
        collector_failures=1
    elif ! run_logged "${npu_compute_env}" "${smoke_dir}" "${output_dir}/logs/${case_name}.npu-compute.compile.log" \
        cmake --build "${npu_build_dir}" --parallel; then
        npu_ready=0
        collector_failures=1
    fi

    printf '[%s] build with msopprof CANN\n' "${case_name}"
    if ! run_logged "${msopprof_env}" "${smoke_dir}" "${output_dir}/logs/${case_name}.msopprof.build.log" \
        cmake -S "${case_dir}" -B "${msopprof_build_dir}" -DCMAKE_ASC_ARCHITECTURES=dav-3510; then
        msopprof_ready=0
        collector_failures=1
    elif ! run_logged "${msopprof_env}" "${smoke_dir}" "${output_dir}/logs/${case_name}.msopprof.compile.log" \
        cmake --build "${msopprof_build_dir}" --parallel; then
        msopprof_ready=0
        collector_failures=1
    fi

    if (( npu_ready )); then
        mkdir -p "${npu_raw_dir}"
        npu_command=(npu-compute --replay-mode kernel)
        for section in "${sections[@]}"; do
            npu_command+=(--section "${section}")
        done
        npu_command+=(--export "${npu_output_dir}" "${npu_build_dir}/demo")
        printf '[%s] collect with npu-compute\n' "${case_name}"
        npu_collect_status=0
        if ! run_logged "${npu_compute_env}" "${npu_build_dir}" "${output_dir}/logs/${case_name}.npu-compute.collect.log" \
            "${npu_command[@]}"; then
            npu_collect_status=1
        fi
        if ! copy_collection_data "${output_dir}/logs/${case_name}.npu-compute.collect.log" "${npu_raw_dir}"; then
            collector_failures=1
        fi
        if (( npu_collect_status )); then
            collector_failures=1
        else
            mapfile -t npu_reports < <(find "${npu_output_dir}" -maxdepth 1 -type f -name '*.npu-rep' -print)
            if (( ${#npu_reports[@]} != 1 )); then
                printf 'expected exactly one npu-compute report in %s, found %d\n' \
                    "${npu_output_dir}" "${#npu_reports[@]}" > "${output_dir}/logs/${case_name}.npu-compute.import.log"
                collector_failures=1
            else
                mkdir -p "${npu_import_dir}"
                if ! run_logged "${npu_compute_env}" "${npu_import_dir}" \
                    "${output_dir}/logs/${case_name}.npu-compute.import.log" \
                    npu-compute --import "${npu_reports[0]}" --export "${npu_import_dir}"; then
                    collector_failures=1
                else
                    npu_comparison_root="${npu_import_dir}"
                fi
            fi
        fi
    fi

    if (( msopprof_ready )); then
        for section in "${sections[@]}"; do
            printf '[%s] collect %s with msopprof\n' "${case_name}" "${section}"
            if ! run_logged "${msopprof_env}" "${msopprof_build_dir}" \
                "${output_dir}/logs/${case_name}.msopprof.${section}.log" \
                msopprof --output="${msopprof_output_dir}/${section}" --aic-metrics="${section}" \
                --launch-count=1 --warm-up=0 --replay-mode=kernel "${msopprof_build_dir}/demo"; then
                collector_failures=1
            fi
        done
    fi

    comparison_command=(python3 "${smoke_dir}/compare_reports.py" --npu-root "${npu_comparison_root}" \
        --msopprof-root "${msopprof_output_dir}" --output "${comparison_output_dir}")
    for section in "${sections[@]}"; do
        comparison_command+=(--section "${section}")
    done
    "${comparison_command[@]}" > "${output_dir}/logs/${case_name}.comparison.log" 2>&1
done

printf 'comparison output: %s\n' "${output_dir}"
if (( collector_failures )); then
    printf 'one or more build or collection commands failed; inspect %s/logs\n' "${output_dir}" >&2
    exit 1
fi
printf 'comparison complete; metric differences are report-only\n'
