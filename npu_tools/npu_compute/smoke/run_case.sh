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

npu_compute_case_smoke_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
readonly npu_compute_case_smoke_dir
readonly npu_compute_baseline_sections=(PipeUtilization Memory MemoryL0 MemoryUB L2Cache)
declare -a sections=()
declare -a section_args=()

discover_sections()
{
    local output_file
    local status
    local section
    local baseline
    local found
    local npu_compute_cli=${NPU_COMPUTE_SMOKE_CLI:-npu-compute}
    local -A seen=()

    output_file=$(mktemp)
    # The fallback discovery command is npu-compute --list-sections.
    if "${npu_compute_cli}" --list-sections > "${output_file}"; then
        status=0
    else
        status=$?
    fi
    if (( status != 0 )); then
        rm -f -- "${output_file}"
        printf 'npu-compute --list-sections failed with status %d\n' "${status}" >&2
        return 1
    fi

    sections=()
    while IFS= read -r section || [[ -n "${section}" ]]; do
        if [[ -z "${section}" ]]; then
            rm -f -- "${output_file}"
            printf 'blank Section ID returned by npu-compute --list-sections\n' >&2
            return 1
        fi
        if [[ ! "${section}" =~ ^[A-Za-z][A-Za-z0-9_]*$ ]]; then
            rm -f -- "${output_file}"
            printf 'invalid Section ID: %s\n' "${section}" >&2
            return 1
        fi
        if [[ -n "${seen[${section}]:-}" ]]; then
            rm -f -- "${output_file}"
            printf 'duplicate Section ID: %s\n' "${section}" >&2
            return 1
        fi
        seen["${section}"]=1
        sections+=("${section}")
    done < "${output_file}"
    rm -f -- "${output_file}"

    if (( ${#sections[@]} == 0 )); then
        printf 'npu-compute --list-sections returned no Section IDs\n' >&2
        return 1
    fi
    for baseline in "${npu_compute_baseline_sections[@]}"; do
        found=0
        for section in "${sections[@]}"; do
            if [[ "${section}" == "${baseline}" ]]; then
                found=1
                break
            fi
        done
        if (( ! found )); then
            printf 'required Section is unavailable: %s\n' "${baseline}" >&2
            return 1
        fi
    done
}

build_section_args()
{
    local section
    section_args=()
    for section in "${sections[@]}"; do
        section_args+=(--section "${section}")
    done
}

validate_hardware_info()
{
    local hardware_info=$1
    python3 - "${hardware_info}" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
expected_categories = [
    "Host Info",
    "Device Info",
    "CPU Information",
    "AI Core Information",
    "Memory Information",
]
records = []
with path.open(encoding="utf-8") as stream:
    for line_number, line in enumerate(stream, 1):
        if not line.strip():
            raise SystemExit(f"blank HardwareInfo JSONL row at line {line_number}: {path}")
        try:
            record = json.loads(line)
        except json.JSONDecodeError as error:
            raise SystemExit(f"invalid HardwareInfo JSONL at line {line_number}: {error}")
        if not isinstance(record, dict):
            raise SystemExit(f"HardwareInfo JSONL row is not an object at line {line_number}: {path}")
        records.append(record)

categories = [record.get("category") for record in records]
if categories != expected_categories:
    raise SystemExit(f"HardwareInfo categories are invalid: {categories!r}")
device = records[1]
arch = device.get("arch info")
if isinstance(arch, bool) or not (
    (isinstance(arch, str) and arch == "3510")
    or (isinstance(arch, (int, float)) and arch == 3510)
):
    raise SystemExit("HardwareInfo Device Info arch info must be 3510")
PY
}

validate_csv()
{
    local csv_path=$1
    python3 - "${csv_path}" <<'PY'
import csv
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
with path.open(newline="", encoding="utf-8") as stream:
    try:
        rows = list(csv.reader(stream, strict=True))
    except csv.Error as error:
        raise SystemExit(f"malformed CSV: {path}: {error}") from error
if len(rows) < 2 or not rows[0]:
    raise SystemExit(f"CSV requires a header and data row: {path}")
header = rows[0]
if any(not name.strip() for name in header):
    raise SystemExit(f"CSV header names must be nonempty: {path}")
if len(set(header)) != len(header):
    raise SystemExit(f"CSV header names must be unique: {path}")
width = len(header)
if any(len(row) != width for row in rows[1:]):
    raise SystemExit(f"CSV field count mismatch: {path}")
PY
}

validate_case_core_rows()
{
    local case_id=$1
    local pipe_utilization_csv=$2
    local expected_sub_blocks

    case "${case_id}" in
        vector_add|reg_add)
            expected_sub_blocks=vector0
            ;;
        cube_mmad)
            expected_sub_blocks=cube0
            ;;
        mix_1_1)
            expected_sub_blocks=cube0,vector0
            ;;
        mix_1_2)
            expected_sub_blocks=cube0,vector0,vector1
            ;;
        simt_hello)
            return 0
            ;;
        *)
            printf 'unknown smoke case ID: %s\n' "${case_id}" >&2
            return 1
            ;;
    esac

    python3 - "${case_id}" "${pipe_utilization_csv}" "${expected_sub_blocks}" <<'PY'
import csv
import pathlib
import sys

case_id = sys.argv[1]
path = pathlib.Path(sys.argv[2])
expected = set(sys.argv[3].split(","))

with path.open(newline="", encoding="utf-8") as stream:
    reader = csv.DictReader(stream, strict=True)
    fieldnames = reader.fieldnames or []
    if fieldnames.count("block_id") != 1 or fieldnames.count("sub_block_id") != 1:
        raise SystemExit(
            f"core row validation requires columns block_id and sub_block_id for {case_id}: "
            f"actual columns {fieldnames!r}"
        )
    rows = list(reader)

if any(not (row["block_id"] or "").strip() for row in rows):
    raise SystemExit(f"core row validation requires nonempty block_id for {case_id}")

actual = {row["sub_block_id"] for row in rows}
if actual != expected:
    format_set = lambda values: "{" + ",".join(
        "<empty>" if value == "" else value for value in sorted(values)
    ) + "}"
    raise SystemExit(
        f"core row mismatch for {case_id}: expected sub_block_id set {format_set(expected)}, "
        f"actual {format_set(actual)}"
    )
PY
}

run_case()
{
    local skip_build=$1
    local requested_case_dir=$2
    local case_dir
    local case_id
    local build_dir
    local report_path
    local log_file
    local prepare_script
    local verify_script
    local collection_status
    local data_directory
    local data_real
    local build_real
    local section
    local csv_path
    local hardware_info
    local npu_compute_cli=${NPU_COMPUTE_SMOKE_CLI:-npu-compute}
    local errexit_was_set=0
    local -a pipeline_status
    local -a data_diagnostics
    local -a report_diagnostics

    if [[ ! -d "${requested_case_dir}" ]]; then
        printf 'case directory does not exist: %s\n' "${requested_case_dir}" >&2
        return 1
    fi
    case_dir=$(cd "${requested_case_dir}" && pwd -P)
    case_id=$(basename "${case_dir}")
    if (( ! skip_build )) && [[ "${case_dir}" != "${npu_compute_case_smoke_dir}/examples/"* ]]; then
        printf 'case directory must be directly below %s/examples: %s\n' \
            "${npu_compute_case_smoke_dir}" "${case_dir}" >&2
        return 1
    fi
    if (( ! skip_build )) && \
        [[ "$(dirname "${case_dir}")" != "${npu_compute_case_smoke_dir}/examples" ]]; then
        printf 'case directory must be directly below %s/examples: %s\n' \
            "${npu_compute_case_smoke_dir}" "${case_dir}" >&2
        return 1
    fi

    build_dir="${case_dir}/build"
    if [[ -L "${build_dir}" ]]; then
        printf 'case build directory must not be a symlink: %s\n' "${build_dir}" >&2
        return 1
    fi
    build_real=$(realpath -m -- "${build_dir}")
    if [[ "${build_real}" != "${build_dir}" ]]; then
        printf 'case build directory is not canonical: %s\n' "${build_dir}" >&2
        return 1
    fi
    if (( skip_build )); then
        if [[ ! -d "${build_dir}" ]]; then
            printf 'missing case build directory: %s\n' "${build_dir}" >&2
            return 1
        fi
        if [[ ! -x "${build_dir}/demo" ]]; then
            printf 'missing executable case program: %s\n' "${build_dir}/demo" >&2
            return 1
        fi
    else
        rm -rf -- "${build_dir}"
        cmake -S "${case_dir}" -B "${build_dir}" -DCMAKE_ASC_ARCHITECTURES=dav-3510
        cmake --build "${build_dir}" --parallel
        if [[ ! -x "${build_dir}/demo" ]]; then
            printf 'case build did not create executable: %s\n' "${build_dir}/demo" >&2
            return 1
        fi
    fi

    discover_sections
    build_section_args

    prepare_script="${case_dir}/prepare.sh"
    if [[ -e "${prepare_script}" && ! -x "${prepare_script}" ]]; then
        printf 'case prepare script is not executable: %s\n' "${prepare_script}" >&2
        return 1
    fi
    if [[ -x "${prepare_script}" ]]; then
        (cd "${build_dir}" && "${prepare_script}")
    fi

    report_path="${build_dir}/result.npu-rep"
    log_file="${build_dir}/npu_compute.log"
    rm -f -- "${report_path}" "${log_file}"
    [[ $- == *e* ]] && errexit_was_set=1
    set +e
    (cd "${build_dir}" && \
        "${npu_compute_cli}" "${section_args[@]}" --export "${report_path}" ./demo) \
        2>&1 | tee "${log_file}"
    pipeline_status=("${PIPESTATUS[@]}")
    if (( errexit_was_set )); then
        set -e
    fi
    collection_status=${pipeline_status[0]}
    if (( collection_status != 0 )); then
        printf 'npu-compute collection failed with status %d\n' "${collection_status}" >&2
        return 1
    fi
    if (( pipeline_status[1] != 0 )); then
        printf 'failed to write collection log with status %d\n' "${pipeline_status[1]}" >&2
        return 1
    fi

    mapfile -t data_diagnostics < <(grep '^npu-compute: data-directory=' "${log_file}" || :)
    mapfile -t report_diagnostics < <(grep '^npu-compute: report=' "${log_file}" || :)
    if (( ${#data_diagnostics[@]} != 1 )); then
        printf 'expected exactly one npu-compute: data-directory= diagnostic, found %d\n' \
            "${#data_diagnostics[@]}" >&2
        return 1
    fi
    if (( ${#report_diagnostics[@]} != 1 )); then
        printf 'expected exactly one npu-compute: report= diagnostic, found %d\n' \
            "${#report_diagnostics[@]}" >&2
        return 1
    fi
    data_directory=${data_diagnostics[0]#npu-compute: data-directory=}
    if [[ "${data_directory}" != /* ]]; then
        printf 'collection data directory is not absolute: %s\n' "${data_directory}" >&2
        return 1
    fi
    if [[ ! -d "${data_directory}" || -L "${data_directory}" ]]; then
        printf 'collection data directory is not a real directory: %s\n' "${data_directory}" >&2
        return 1
    fi
    data_real=$(realpath -e -- "${data_directory}")
    if [[ "${data_real}" != "${data_directory}" || "$(dirname "${data_real}")" != "${build_dir}" ]]; then
        printf 'collection data directory must resolve directly below build directory: %s\n' \
            "${data_directory}" >&2
        return 1
    fi

    if [[ "${report_diagnostics[0]#npu-compute: report=}" != "${report_path}" ]]; then
        printf 'collection report diagnostic does not match requested report: %s\n' \
            "${report_diagnostics[0]}" >&2
        return 1
    fi
    if [[ ! -f "${report_path}" || ! -s "${report_path}" || -L "${report_path}" ]]; then
        printf 'missing or empty report: %s\n' "${report_path}" >&2
        return 1
    fi

    hardware_info="${data_directory}/HardwareInfo.jsonl"
    if [[ ! -f "${hardware_info}" || ! -s "${hardware_info}" || -L "${hardware_info}" ]]; then
        printf 'missing or empty HardwareInfo file: %s\n' "${hardware_info}" >&2
        return 1
    fi
    validate_hardware_info "${hardware_info}"

    for section in "${sections[@]}"; do
        csv_path="${data_directory}/${section}.csv"
        if [[ ! -f "${csv_path}" || ! -s "${csv_path}" || -L "${csv_path}" ]]; then
            printf 'missing or empty Section CSV: %s\n' "${csv_path}" >&2
            return 1
        fi
        validate_csv "${csv_path}"
    done
    validate_case_core_rows "${case_id}" "${data_directory}/PipeUtilization.csv"

    verify_script="${case_dir}/verify.sh"
    if [[ -e "${verify_script}" && ! -x "${verify_script}" ]]; then
        printf 'case verify script is not executable: %s\n' "${verify_script}" >&2
        return 1
    fi
    if [[ -x "${verify_script}" ]]; then
        (cd "${build_dir}" && "${verify_script}")
    elif ! grep -Fxq "result verification passed: ${case_id}" "${log_file}"; then
        printf 'missing application success marker for case %s\n' "${case_id}" >&2
        return 1
    fi

    printf '[PASSED] %s\n' "${case_id}"
}

main()
{
    local skip_build=0
    local case_dir
    if [[ $# -eq 1 && $1 != --* ]]; then
        case_dir=$1
    elif [[ $# -eq 2 && $1 == --skip-build ]]; then
        skip_build=1
        case_dir=$2
    else
        printf 'usage: %s [--skip-build] <case-dir>\n' "${BASH_SOURCE[0]}" >&2
        return 2
    fi
    run_case "${skip_build}" "${case_dir}"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    main "$@"
fi
