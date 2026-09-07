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

smoke_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
readonly smoke_dir
readonly cases=(vector_add cube_mmad mix_1_1 mix_1_2 reg_add simt_hello)
readonly standalone_examples=(mix_1_2_msprof)
readonly baseline_sections=(PipeUtilization Memory MemoryL0 MemoryUB L2Cache)
check_tmp_dir=$(mktemp -d)
readonly check_tmp_dir
trap 'rm -rf -- "${check_tmp_dir}"' EXIT

require_file() {
    local path=$1
    if [[ ! -f "${path}" ]]; then
        printf 'missing required file: %s\n' "${path}" >&2
        exit 1
    fi
}

require_executable() {
    local path=$1
    if [[ ! -x "${path}" ]]; then
        printf 'missing required executable: %s\n' "${path}" >&2
        exit 1
    fi
}

require_literal() {
    local literal=$1
    local path=$2
    if ! grep -Fq -- "${literal}" "${path}"; then
        printf 'missing required text %q in %s\n' "${literal}" "${path}" >&2
        exit 1
    fi
}

reject_literal() {
    local literal=$1
    local path=$2
    if grep -Fq -- "${literal}" "${path}"; then
        printf 'forbidden text %q in %s\n' "${literal}" "${path}" >&2
        exit 1
    fi
}

require_regex() {
    local pattern=$1
    local path=$2
    if ! grep -Eq -- "${pattern}" "${path}"; then
        printf 'missing required pattern %q in %s\n' "${pattern}" "${path}" >&2
        exit 1
    fi
}

require_executable "${smoke_dir}/build.sh"
require_executable "${smoke_dir}/run_case.sh"
require_executable "${smoke_dir}/run_smoke.sh"
require_executable "${smoke_dir}/compare_tools.sh"
require_file "${smoke_dir}/README.md"
require_file "${smoke_dir}/README_en.md"
require_file "${smoke_dir}/LICENSE"
require_file "${smoke_dir}/compare_reports.py"

repo_dir=$(cd "${smoke_dir}/../../.." && pwd)
if ! cmp -s "${repo_dir}/LICENSE" "${smoke_dir}/LICENSE"; then
    printf 'smoke LICENSE must match the repository CANN license\n' >&2
    exit 1
fi

bash -n "${smoke_dir}/build.sh"
bash -n "${smoke_dir}/run_case.sh"
bash -n "${smoke_dir}/run_smoke.sh"
bash -n "${smoke_dir}/compare_tools.sh"

readme="${smoke_dir}/README.md"
readme_en="${smoke_dir}/README_en.md"
require_literal '日常不可变冒烟基线' "${readme}"
require_literal '日常功能修改不应改动固定 smoke 场景' "${readme}"
require_literal '确需更新样例、runner、测试或本文档时，应单独评审' "${readme}"
require_literal 'dav-3510' "${readme}"
require_literal 'ASCEND_HOME_PATH' "${readme}"
require_literal 'CANN 9.2' "${readme}"
require_literal 'Driver' "${readme}"
require_literal '有权限将生成的软件包安装到' "${readme}"
require_literal '先构建并安装 asc-tools 软件包' "${readme}"
require_literal '无需预先单独构建工具或样例' "${readme}"
require_literal 'bash npu_tools/npu_compute/smoke/run_smoke.sh' "${readme}"
require_literal '--skip-asc-tools-build' "${readme}"
require_literal '跳过打包安装时' "${readme}"
require_literal '许可证' "${readme}"
require_literal 'CANN Open Software License Agreement Version 2.0' "${readme}"
require_literal '# NPU Compute dav3510 Smoke Baseline' "${readme_en}"
require_literal '## License' "${readme_en}"
require_literal 'CANN Open Software License Agreement Version 2.0' "${readme_en}"
require_literal '--skip-asc-tools-build' "${readme_en}"
require_literal 'mix_1_2_msprof' "${readme}"
require_literal 'mix_1_2_msprof' "${readme_en}"
require_literal 'MsprofStart' "${readme}"
require_literal 'MsprofStop' "${readme}"
require_literal 'PROF_COMPUTE_ALL_BLOCK' "${readme}"
require_literal 'not one of the six fixed cases' "${readme_en}"
require_literal 'npu-compute 与 msopprof 对比' "${readme}"
require_literal 'compare_tools.sh' "${readme}"
require_literal '--npu-compute-env' "${readme}"
require_literal '--msopprof-env' "${readme}"
require_literal '仅报告模式' "${readme}"
require_literal 'npu-compute and msopprof Comparison' "${readme_en}"
require_literal 'report-only mode' "${readme_en}"
for case_name in "${cases[@]}"; do
    require_literal "| \`${case_name}\` |" "${readme}"
    require_literal "| \`${case_name}\` |" "${readme_en}"
    require_literal "bash npu_tools/npu_compute/smoke/examples/${case_name}/run.sh" "${readme}"
done
mapfile -t readme_case_rows < <(grep -E '^\| `[a-z][a-z0-9_]*` \|' "${readme}")
if (( ${#readme_case_rows[@]} != ${#cases[@]} )); then
    printf 'README fixed case table has %d rows, expected %d\n' \
        "${#readme_case_rows[@]}" "${#cases[@]}" >&2
    exit 1
fi
require_literal 'npu-compute --list-sections' "${readme}"
require_literal '为返回的每一个 Section' "${readme}"
for section in "${baseline_sections[@]}"; do
    require_literal "\`${section}\`" "${readme}"
done
require_literal 'npu_tools/npu_compute/smoke/build/logs/<case>.log' "${readme}"
require_literal '结果校验' "${readme}"
require_literal 'result verification passed: <case>' "${readme}"
require_literal 'result verification passed: <case> block=<n>' "${readme}"
require_literal '独立输入幅值并按独立期望值校验' "${readme}"
require_literal 'PipeUtilization.csv' "${readme}"
require_literal 'sub_block_id' "${readme}"
require_literal '{vector0}' "${readme}"
require_literal '{cube0}' "${readme}"
require_literal '{cube0,vector0}' "${readme}"
require_literal '{cube0,vector0,vector1}' "${readme}"
require_literal '每行 `block_id` 非空' "${readme}"
require_literal 'HardwareInfo.jsonl' "${readme}"
require_literal '设备架构为 `3510`' "${readme}"
require_literal '严格 CSV 语法' "${readme}"
require_literal '至少包含表头和一行数据' "${readme}"
require_literal '表头名称非空且唯一' "${readme}"
require_literal '所有数据行字段数与表头一致' "${readme}"
require_literal '计数器对应的数据或指标单元格可以为空' "${readme}"
require_literal '`result.npu-rep` 是非空普通文件' "${readme}"
require_literal 'total=6 passed=6 failed=0' "${readme}"

protected_symlinks="${check_tmp_dir}/protected-symlinks"
set +e
find "${smoke_dir}" \
    \( -path "${smoke_dir}/build" -o -path "${smoke_dir}/examples/*/build" \) -prune -o \
    -type l -print0 > "${protected_symlinks}"
symlink_scan_status=$?
set -e
if (( symlink_scan_status != 0 )); then
    printf 'protected smoke symlink scan failed with status %d\n' "${symlink_scan_status}" >&2
    exit 1
fi
if [[ -s "${protected_symlinks}" ]]; then
    printf 'protected smoke inputs must not contain symlinks:\n' >&2
    while IFS= read -r -d '' symlink_path; do
        printf '  %s\n' "${symlink_path#"${smoke_dir}/"}" >&2
    done < "${protected_symlinks}"
    exit 1
fi

if [[ ! -d "${smoke_dir}/examples" ]]; then
    printf 'missing required directory: %s\n' "${smoke_dir}/examples" >&2
    exit 1
fi
expected_case_dirs="${check_tmp_dir}/expected-case-dirs"
actual_case_dirs="${check_tmp_dir}/actual-case-dirs"
printf '%s\0' "${cases[@]}" "${standalone_examples[@]}" | LC_ALL=C sort -z > "${expected_case_dirs}"
find "${smoke_dir}/examples" -mindepth 1 -maxdepth 1 -type d -printf '%f\0' \
    | LC_ALL=C sort -z > "${actual_case_dirs}"
if ! cmp -s "${expected_case_dirs}" "${actual_case_dirs}"; then
    printf 'smoke examples must contain exactly these case directories:\n' >&2
    tr '\0' '\n' < "${expected_case_dirs}" >&2
    printf 'actual case directories:\n' >&2
    tr '\0' '\n' < "${actual_case_dirs}" >&2
    exit 1
fi

for case_name in "${cases[@]}"; do
    case_dir="${smoke_dir}/examples/${case_name}"
    cmake_file="${case_dir}/CMakeLists.txt"
    source_file="${case_dir}/${case_name}.asc"
    runner="${case_dir}/run.sh"

    require_file "${cmake_file}"
    require_file "${source_file}"
    require_executable "${runner}"
    require_regex '^[[:space:]]*set\(CMAKE_ASC_ARCHITECTURES[[:space:]]+"dav-3510"([[:space:]]|\))' \
        "${cmake_file}"
    require_literal 'case_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)' "${runner}"
    require_literal 'exec "${case_dir}/../../run_case.sh" "${case_dir}"' "${runner}"
    reject_literal 'exec "${case_dir}/../../../run_case.sh" "${case_dir}"' "${runner}"
    bash -n "${runner}"
done

msprof_dir="${smoke_dir}/examples/mix_1_2_msprof"
msprof_cmake="${msprof_dir}/CMakeLists.txt"
msprof_source="${msprof_dir}/mix_1_2_msprof.asc"
msprof_runner="${msprof_dir}/run.sh"
require_file "${msprof_cmake}"
require_file "${msprof_source}"
require_executable "${msprof_runner}"
bash -n "${msprof_runner}"
require_regex '^[[:space:]]*set\(CMAKE_ASC_ARCHITECTURES[[:space:]]+"dav-3510"([[:space:]]|\))' "${msprof_cmake}"
require_literal 'acl_tool_injection' "${msprof_cmake}"
require_literal 'MsprofStart' "${msprof_source}"
require_literal 'MsprofStop' "${msprof_source}"
require_literal 'acltoolUploaderInit' "${msprof_source}"
require_literal 'PROF_TASK_TIME_MASK | PROF_AICORE_METRICS_MASK' "${msprof_source}"
require_literal 'PROF_CONFIG_ATTR_AICORE_METRICS' "${msprof_source}"
require_literal 'PROF_CONFIG_ATTR_TASK_BLOCK' "${msprof_source}"
require_literal 'PROF_COMPUTE_ALL_BLOCK' "${msprof_source}"
require_literal 'std::array<uint32_t, COMPUTE_AICORE_METRICS_NUM>' "${msprof_source}"
require_literal 'std::copy(kPmuEvents.begin(), kPmuEvents.end(), metricsAttr.value.aicoreMetrics);' "${msprof_source}"
require_literal 'kTaskPmuFunction' "${msprof_source}"
require_literal 'kBlockPmuFunction' "${msprof_source}"
require_literal 'msprof PMU task:' "${msprof_source}"
require_literal 'msprof PMU block:' "${msprof_source}"
reject_literal 'PROF_LOG_MASK' "${msprof_source}"
reject_literal 'LOG_DATA_TYPE' "${msprof_source}"

entry_prefix='^[[:space:]]*(extern[[:space:]]+"C"[[:space:]]+)?'
require_regex "${entry_prefix}(__vector__[[:space:]]+__global__|__global__[[:space:]]+__vector__)[[:space:]].*void[[:space:]]" \
    "${smoke_dir}/examples/vector_add/vector_add.asc"
require_regex "${entry_prefix}(__cube__[[:space:]]+__global__|__global__[[:space:]]+__cube__)[[:space:]].*void[[:space:]]" \
    "${smoke_dir}/examples/cube_mmad/cube_mmad.asc"

for mix_case in mix_1_1 mix_1_2; do
    mix_source="${smoke_dir}/examples/${mix_case}/${mix_case}.asc"
    require_literal 'ASCEND_IS_AIC' "${mix_source}"
    require_literal 'ASCEND_IS_AIV' "${mix_source}"
    require_literal 'CrossCoreSetFlag' "${mix_source}"
    require_literal 'CrossCoreWaitFlag' "${mix_source}"
done
require_regex '^[[:space:]]*(extern[[:space:]]+"C"[[:space:]]+)?(__mix__\(1, 1\)[[:space:]]+__global__|__global__[[:space:]]+__mix__\(1, 1\))[[:space:]].*void[[:space:]]' \
    "${smoke_dir}/examples/mix_1_1/mix_1_1.asc"
require_regex '^[[:space:]]*(extern[[:space:]]+"C"[[:space:]]+)?(__mix__\(1, 2\)[[:space:]]+__global__|__global__[[:space:]]+__mix__\(1, 2\))[[:space:]].*void[[:space:]]' \
    "${smoke_dir}/examples/mix_1_2/mix_1_2.asc"

require_regex '^[[:space:]]*__simd_vf__[[:space:]]+.*void[[:space:]]' \
    "${smoke_dir}/examples/reg_add/reg_add.asc"
require_literal 'AscendC::Reg' "${smoke_dir}/examples/reg_add/reg_add.asc"
require_literal '--cce-simd-vf-fusion=false' "${smoke_dir}/examples/reg_add/CMakeLists.txt"
require_literal '--enable-simt' "${smoke_dir}/examples/simt_hello/CMakeLists.txt"
for case_name in vector_add cube_mmad mix_1_1 mix_1_2 reg_add; do
    require_literal 'constexpr uint32_t kBlockCount = 8;' \
        "${smoke_dir}/examples/${case_name}/${case_name}.asc"
    require_literal "result verification passed: ${case_name} block=" \
        "${smoke_dir}/examples/${case_name}/${case_name}.asc"
done
require_literal 'constexpr uint32_t kBlocksPerGrid = 8;' \
    "${smoke_dir}/examples/simt_hello/simt_hello.asc"
require_literal 'result verification passed: simt_hello block=' \
    "${smoke_dir}/examples/simt_hello/simt_hello.asc"

require_literal 'constexpr uint32_t kElementsPerBlock = 4096;' \
    "${smoke_dir}/examples/vector_add/vector_add.asc"
require_literal 'constexpr uint32_t kElementsPerBlock = 4096;' \
    "${smoke_dir}/examples/reg_add/reg_add.asc"
for matrix_case in cube_mmad mix_1_1 mix_1_2; do
    matrix_source="${smoke_dir}/examples/${matrix_case}/${matrix_case}.asc"
    require_literal 'constexpr uint32_t kM = 16;' "${matrix_source}"
    require_literal 'constexpr uint32_t kK = 16;' "${matrix_source}"
    require_literal 'constexpr uint32_t kN = 16;' "${matrix_source}"
    require_literal 'constexpr uint32_t kTilesPerBlock = 32;' "${matrix_source}"
    require_literal 'aGm[tile * kAElementsPerTile]' "${matrix_source}"
    require_literal 'bGm[tile * kBElementsPerTile]' "${matrix_source}"
    reject_literal 'SetGlobalBuffer(a + blockIndex * kAElementsPerBlock + tile' "${matrix_source}"
    reject_literal 'SetGlobalBuffer(b + blockIndex * kBElementsPerBlock + tile' "${matrix_source}"
done
require_literal 'cGm[tile * kCElementsPerTile]' "${smoke_dir}/examples/cube_mmad/cube_mmad.asc"
require_literal 'cGm[tile * kElementCountPerTile]' "${smoke_dir}/examples/mix_1_1/mix_1_1.asc"
require_literal 'cGm[tileOffset]' "${smoke_dir}/examples/mix_1_2/mix_1_2.asc"
reject_literal 'SetGlobalBuffer(c + blockIndex * kCElementsPerBlock + tile' \
    "${smoke_dir}/examples/cube_mmad/cube_mmad.asc"
reject_literal 'SetGlobalBuffer(c + blockIndex * kElementCount + tile' \
    "${smoke_dir}/examples/mix_1_1/mix_1_1.asc"
reject_literal 'SetGlobalBuffer(c + batchIndex * kElementCount + tile' \
    "${smoke_dir}/examples/mix_1_2/mix_1_2.asc"
require_literal 'constexpr uint32_t kThreadsPerBlock = 256;' \
    "${smoke_dir}/examples/simt_hello/simt_hello.asc"
require_literal 'constexpr uint32_t kValuesPerThread = 64;' \
    "${smoke_dir}/examples/simt_hello/simt_hello.asc"

run_case="${smoke_dir}/run_case.sh"
require_literal 'npu-compute --list-sections' "${run_case}"
for section in "${baseline_sections[@]}"; do
    require_literal "${section}" "${run_case}"
done
require_literal 'section_args+=(--section "${section}")' "${run_case}"
require_literal '${section}.csv' "${run_case}"
require_literal 'validate_case_core_rows()' "${run_case}"
require_literal 'csv.DictReader' "${run_case}"
require_literal 'expected_sub_blocks=vector0' "${run_case}"
require_literal 'expected_sub_blocks=cube0' "${run_case}"
require_literal 'expected_sub_blocks=cube0,vector0' "${run_case}"
require_literal 'expected_sub_blocks=cube0,vector0,vector1' "${run_case}"
require_literal 'validate_case_core_rows "${case_id}" "${data_directory}/PipeUtilization.csv"' \
    "${run_case}"

run_smoke="${smoke_dir}/run_smoke.sh"
require_literal 'skip_asc_tools_build=0' "${run_smoke}"
require_literal '--skip-asc-tools-build' "${run_smoke}"
require_literal 'source "${npu_compute_suite_smoke_dir}/build.sh"' "${run_smoke}"
case_declaration_pattern='^[[:space:]]*(readonly([[:space:]]+-a)?|local[[:space:]]+-a)[[:space:]]+cases=\(vector_add cube_mmad mix_1_1 mix_1_2 reg_add simt_hello\)[[:space:]]*(#.*)?$'
mapfile -t case_declarations < <(grep -En "${case_declaration_pattern}" "${run_smoke}")
if (( ${#case_declarations[@]} != 1 )); then
    printf 'run_smoke.sh active fixed case declarations: %d (expected 1)\n' \
        "${#case_declarations[@]}" >&2
    if (( ${#case_declarations[@]} != 0 )); then
        printf '  %s\n' "${case_declarations[@]}" >&2
    fi
    exit 1
fi
case_mutation_pattern='^[[:space:]]*((readonly|local|declare)([[:space:]]+-[[:alnum:]]+)*[[:space:]]+)?cases([[:space:]]*\[[^]]*\])?[[:space:]]*\+?='
mapfile -t case_mutations < <(grep -En "${case_mutation_pattern}" "${run_smoke}")
if (( ${#case_mutations[@]} != 1 )); then
    printf 'run_smoke.sh reassigns or appends to its fixed cases list\n' >&2
    exit 1
fi

private_devkit='/home/chenning/my-asc-''devkit'
private_examples='my-asc-''devkit/examples'
build_tree_cli='build/npu_compute/bin/npu-''compute'
forbidden_matches="${check_tmp_dir}/forbidden-matches"
set +e
rg --hidden --no-ignore -n "${private_devkit}|${private_examples}|${build_tree_cli}" \
    "${smoke_dir}" -g '!**/build/**' > "${forbidden_matches}"
forbidden_scan_status=$?
set -e
case ${forbidden_scan_status} in
    0)
        cat "${forbidden_matches}" >&2
        printf 'smoke suite references a forbidden private or build-tree path\n' >&2
        exit 1
        ;;
    1)
        ;;
    *)
        printf 'forbidden-reference scan failed with status %d\n' "${forbidden_scan_status}" >&2
        exit 1
        ;;
esac

printf 'npu-compute smoke layout check passed\n'
