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
readonly run_case_script="${smoke_dir}/run_case.sh"
readonly run_smoke_script="${smoke_dir}/run_smoke.sh"

for runner_script in "${run_case_script}" "${run_smoke_script}"; do
    if [[ ! -f "${runner_script}" ]]; then
        printf 'missing runner under test: %s\n' "${runner_script}" >&2
        exit 1
    fi
done

fixture_root=$(mktemp -d)
readonly fixture_root
trap 'rm -rf -- "${fixture_root}"' EXIT

fail()
{
    printf 'fixture assertion failed: %s\n' "$*" >&2
    exit 1
}

require_literal()
{
    local literal=$1
    local path=$2
    grep -Fq -- "${literal}" "${path}" || fail "missing ${literal@Q} in ${path}"
}

fake_bin="${fixture_root}/bin"
mkdir -p "${fake_bin}"
fake_npu_compute="${fake_bin}/npu-compute"
cat > "${fake_npu_compute}" <<'FAKE_NPU_COMPUTE'
#!/usr/bin/env bash
set -euo pipefail

readonly baseline=(PipeUtilization Memory MemoryL0 MemoryUB L2Cache FutureSection)
mode=${NPU_COMPUTE_FIXTURE_MODE:-pass}

if [[ ${1:-} == --list-sections ]]; then
    case ${mode} in
        list_missing)
            printf '%s\n' PipeUtilization Memory MemoryL0 L2Cache FutureSection
            ;;
        list_duplicate)
            printf '%s\n' PipeUtilization Memory MemoryL0 MemoryUB L2Cache Memory
            ;;
        list_blank)
            printf '%s\n' PipeUtilization Memory '' MemoryL0 MemoryUB L2Cache FutureSection
            ;;
        list_failure)
            printf 'synthetic list failure\n' >&2
            exit 19
            ;;
        *)
            printf '%s\n' "${baseline[@]}"
            ;;
    esac
    exit 0
fi

printf '%s\n' "$@" > "${NPU_COMPUTE_FIXTURE_ARGV}"
report_path=
while (( $# != 0 )); do
    if [[ $1 == --export ]]; then
        report_path=${2:-}
        break
    fi
    shift
done
[[ -n ${report_path} ]] || exit 65

data_dir="${PWD}/fixture-data"
mkdir -p "${data_dir}"
case_id=$(basename "$(dirname "${PWD}")")
cat > "${data_dir}/HardwareInfo.jsonl" <<'JSONL'
{"category":"Host Info","cpu physical count":1}
{"category":"Device Info","npu count":1,"chip info":"fixture","arch info":"3510"}
{"category":"CPU Information","control cpu count":1}
{"category":"AI Core Information","ai core count":1}
{"category":"Memory Information","hbm total(MB)":1}
JSONL
if [[ ${mode} == bad_arch ]]; then
    sed -i 's/,"arch info":"3510"//' "${data_dir}/HardwareInfo.jsonl"
fi

for section in "${baseline[@]}"; do
    printf '"metric,name",value\n"%s,fixture",1\n' "${section}" > "${data_dir}/${section}.csv"
done
case ${case_id} in
    vector_add)
        printf 'block_id,sub_block_id,value\n0,vector0,1\n0,vector0,2\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    reg_add)
        printf 'block_id,sub_block_id,value\n0,vector0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    cube_mmad)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    mix_1_1)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n0,vector0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    mix_1_2)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n0,vector0,1\n0,vector1,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    simt_hello)
        printf 'block_id,sub_block_id,value\n0,simt0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
esac
case ${mode} in
    vector_missing)
        printf 'block_id,sub_block_id,value\n0,,1\n' > "${data_dir}/PipeUtilization.csv"
        ;;
    vector_unexpected)
        printf 'block_id,sub_block_id,value\n0,vector0,1\n0,cube0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    cube_wrong)
        printf 'block_id,sub_block_id,value\n0,vector0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    cube_core_suffix)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n1,cube0_core18,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    mix11_missing)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    mix12_missing)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n0,vector0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    mix12_unexpected)
        printf 'block_id,sub_block_id,value\n0,cube0,1\n0,vector0,1\n0,vector1,1\n0,vector2,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
    core_missing_column)
        printf 'block_id,value\n0,1\n' > "${data_dir}/PipeUtilization.csv"
        ;;
    core_blank_block)
        printf 'block_id,sub_block_id,value\n,vector0,1\n' \
            > "${data_dir}/PipeUtilization.csv"
        ;;
esac
if [[ ${mode} == header_only ]]; then
    printf '"metric,name",value\n' > "${data_dir}/L2Cache.csv"
elif [[ ${mode} == mismatched_fields ]]; then
    printf '"metric,name",value\n"cache,read",1,extra\n' > "${data_dir}/L2Cache.csv"
elif [[ ${mode} == malformed_quote ]]; then
    printf 'metric,value\n"unterminated,1\n' > "${data_dir}/L2Cache.csv"
elif [[ ${mode} == duplicate_header ]]; then
    printf 'metric,metric\nread,1\n' > "${data_dir}/L2Cache.csv"
elif [[ ${mode} == blank_header ]]; then
    printf 'metric,,value\nread,,1\n' > "${data_dir}/L2Cache.csv"
elif [[ ${mode} == blank_data ]]; then
    printf 'metric,value\nread,\n' > "${data_dir}/L2Cache.csv"
fi

if [[ ${mode} != missing_rep ]]; then
    printf 'fixture report\n' > "${report_path}"
fi

if [[ ${mode} == misleading_marker ]]; then
    printf 'not result verification passed: %s\n' "${case_id}"
else
    printf 'result verification passed: %s\n' "${case_id}"
fi
printf 'npu-compute: data-directory=%s\n' "${data_dir}"
if [[ ${mode} == duplicate_data_diagnostic ]]; then
    printf 'npu-compute: data-directory=%s\n' "${data_dir}"
fi
printf 'npu-compute: report=%s\n' "${report_path}"
if [[ ${mode} == collection_failure ]]; then
    exit 23
fi
FAKE_NPU_COMPUTE
chmod +x "${fake_npu_compute}"

make_case()
{
    local case_name=$1
    local case_dir="${fixture_root}/cases/${case_name}"
    mkdir -p "${case_dir}/build"
    cat > "${case_dir}/build/demo" <<'DEMO'
#!/usr/bin/env bash
exit 0
DEMO
    chmod +x "${case_dir}/build/demo"
    printf '%s\n' "${case_dir}"
}

run_case_fixture()
{
    local mode=$1
    local case_name=$2
    local output_file=$3
    local case_dir
    case_dir=$(make_case "${case_name}")
    NPU_COMPUTE_FIXTURE_MODE="${mode}" \
        NPU_COMPUTE_FIXTURE_ARGV="${case_dir}/argv" \
        NPU_COMPUTE_SMOKE_CLI= \
        PATH="${fake_bin}:${PATH}" \
        bash "${run_case_script}" --skip-build "${case_dir}" > "${output_file}" 2>&1
}

pass_output="${fixture_root}/pass.out"
run_case_fixture pass simt_hello "${pass_output}"
require_literal '[PASSED] simt_hello' "${pass_output}"

blank_data_output="${fixture_root}/blank-data.out"
run_case_fixture blank_data simt_hello "${blank_data_output}" || \
    fail 'blank CSV data value unexpectedly failed'
require_literal '[PASSED] simt_hello' "${blank_data_output}"

for core_case in vector_add cube_mmad mix_1_1 mix_1_2 reg_add; do
    core_output="${fixture_root}/${core_case}.out"
    run_case_fixture pass "${core_case}" "${core_output}" || \
        fail "valid core rows failed for ${core_case}"
    require_literal "[PASSED] ${core_case}" "${core_output}"
done

poison_bin="${fixture_root}/poison-bin"
mkdir -p "${poison_bin}"
cat > "${poison_bin}/npu-compute" <<'POISON_CLI'
#!/usr/bin/env bash
printf 'unexpected PATH npu-compute invocation\n' >&2
exit 88
POISON_CLI
chmod +x "${poison_bin}/npu-compute"
explicit_case_dir=$(make_case simt_hello)
NPU_COMPUTE_FIXTURE_MODE=pass \
    NPU_COMPUTE_FIXTURE_ARGV="${explicit_case_dir}/argv" \
    NPU_COMPUTE_SMOKE_CLI="${fake_npu_compute}" \
    PATH="${poison_bin}:${PATH}" \
    bash "${run_case_script}" --skip-build "${explicit_case_dir}" \
    > "${fixture_root}/explicit-cli.out" 2>&1 || \
    fail 'run_case did not use NPU_COMPUTE_SMOKE_CLI over PATH'
require_literal '[PASSED] simt_hello' "${fixture_root}/explicit-cli.out"

pass_case_dir="${fixture_root}/cases/simt_hello"
expected_argv="${fixture_root}/expected-argv"
cat > "${expected_argv}" <<EOF
--section
PipeUtilization
--section
Memory
--section
MemoryL0
--section
MemoryUB
--section
L2Cache
--section
FutureSection
--export
${pass_case_dir}/build/result.npu-rep
./demo
EOF
cmp -s "${expected_argv}" "${pass_case_dir}/argv" || {
    diff -u "${expected_argv}" "${pass_case_dir}/argv" >&2 || :
    fail 'collection argv did not preserve discovered Section order'
}

expect_case_failure()
{
    local mode=$1
    local expected=$2
    local case_name=${3:-${mode}}
    local output_file="${fixture_root}/${mode}.out"
    set +e
    run_case_fixture "${mode}" "${case_name}" "${output_file}"
    local status=$?
    set -e
    (( status != 0 )) || fail "${mode} unexpectedly passed"
    require_literal "${expected}" "${output_file}"
}

expect_case_failure list_missing 'required Section is unavailable: MemoryUB'
expect_case_failure list_duplicate 'duplicate Section ID: Memory'
expect_case_failure list_blank 'blank Section ID'
expect_case_failure list_failure 'npu-compute --list-sections failed with status 19'
expect_case_failure header_only 'CSV requires a header and data row'
expect_case_failure mismatched_fields 'CSV field count mismatch'
expect_case_failure malformed_quote 'malformed CSV:'
expect_case_failure duplicate_header 'CSV header names must be unique:'
expect_case_failure blank_header 'CSV header names must be nonempty:'
expect_case_failure missing_rep 'missing or empty report'
expect_case_failure duplicate_data_diagnostic 'expected exactly one npu-compute: data-directory= diagnostic'
expect_case_failure bad_arch 'HardwareInfo Device Info arch info must be 3510'
expect_case_failure collection_failure 'npu-compute collection failed with status 23'
expect_case_failure misleading_marker 'missing application success marker for case simt_hello' simt_hello
expect_case_failure unknown_case 'unknown smoke case ID: unknown_case'

expect_core_failure()
{
    local mode=$1
    local case_name=$2
    local expected=$3
    local output_file="${fixture_root}/${mode}.out"
    set +e
    run_case_fixture "${mode}" "${case_name}" "${output_file}"
    local status=$?
    set -e
    (( status != 0 )) || fail "${mode} unexpectedly passed"
    require_literal "${expected}" "${output_file}"
}

expect_core_failure vector_missing vector_add \
    'core row mismatch for vector_add: expected sub_block_id set {vector0}'
expect_core_failure vector_unexpected vector_add \
    'core row mismatch for vector_add: expected sub_block_id set {vector0}'
expect_core_failure cube_wrong cube_mmad \
    'core row mismatch for cube_mmad: expected sub_block_id set {cube0}'
expect_core_failure cube_core_suffix cube_mmad \
    'core row mismatch for cube_mmad: expected sub_block_id set {cube0}'
expect_core_failure mix11_missing mix_1_1 \
    'core row mismatch for mix_1_1: expected sub_block_id set {cube0,vector0}'
expect_core_failure mix12_missing mix_1_2 \
    'core row mismatch for mix_1_2: expected sub_block_id set {cube0,vector0,vector1}'
expect_core_failure mix12_unexpected mix_1_2 \
    'core row mismatch for mix_1_2: expected sub_block_id set {cube0,vector0,vector1}'
expect_core_failure core_missing_column vector_add \
    'core row validation requires columns block_id and sub_block_id for vector_add'
expect_core_failure core_blank_block vector_add \
    'core row validation requires nonempty block_id for vector_add'

outside_build="${fixture_root}/outside-build"
symlink_case="${fixture_root}/cases/symlink_build"
mkdir -p "${outside_build}" "${symlink_case}"
cat > "${outside_build}/demo" <<'DEMO'
#!/usr/bin/env bash
exit 0
DEMO
chmod +x "${outside_build}/demo"
printf 'sentinel unchanged\n' > "${outside_build}/sentinel"
printf 'report unchanged\n' > "${outside_build}/result.npu-rep"
printf 'log unchanged\n' > "${outside_build}/npu_compute.log"
ln -s "${outside_build}" "${symlink_case}/build"
set +e
NPU_COMPUTE_FIXTURE_MODE=pass \
    NPU_COMPUTE_FIXTURE_ARGV="${symlink_case}/argv" \
    PATH="${fake_bin}:${PATH}" \
    bash "${run_case_script}" --skip-build "${symlink_case}" \
    > "${fixture_root}/symlink-build.out" 2>&1
symlink_build_status=$?
set -e
(( symlink_build_status != 0 )) || fail 'symlinked case build directory unexpectedly passed'
require_literal 'case build directory must not be a symlink:' "${fixture_root}/symlink-build.out"
[[ $(< "${outside_build}/sentinel") == 'sentinel unchanged' ]] || fail 'outside sentinel changed'
[[ $(< "${outside_build}/result.npu-rep") == 'report unchanged' ]] || fail 'outside report changed'
[[ $(< "${outside_build}/npu_compute.log") == 'log unchanged' ]] || fail 'outside log changed'
[[ ! -e "${outside_build}/fixture-data" ]] || fail 'collection wrote into outside build directory'

(
    set +u
    set +o pipefail
    before_flags=$-
    before_pipefail=$(set -o | awk '$1 == "pipefail" { print $2 }')
    source "${run_smoke_script}"
    after_flags=$-
    after_pipefail=$(set -o | awk '$1 == "pipefail" { print $2 }')
    [[ ${after_flags} == "${before_flags}" ]] || fail 'sourcing run_smoke.sh changed shell option flags'
    [[ ${after_pipefail} == "${before_pipefail}" ]] || fail 'sourcing run_smoke.sh changed pipefail'
)

source "${run_smoke_script}"
examples_dir="${fixture_root}/smoke-cases"
smoke_log_dir="${fixture_root}/smoke-logs"
mkdir -p "${examples_dir}"/{pass_one,fail_seven,pass_three}
cat > "${examples_dir}/pass_one/run.sh" <<'RUNNER'
#!/usr/bin/env bash
printf 'first runner passed\n'
RUNNER
cat > "${examples_dir}/fail_seven/run.sh" <<'RUNNER'
#!/usr/bin/env bash
printf 'second runner failed\n'
exit 7
RUNNER
cat > "${examples_dir}/pass_three/run.sh" <<RUNNER
#!/usr/bin/env bash
printf 'third runner passed\n'
printf 'ran\n' > "${fixture_root}/third-ran"
RUNNER
chmod +x "${examples_dir}"/*/run.sh

smoke_output="${fixture_root}/smoke.out"
set +e
run_smoke_examples pass_one fail_seven pass_three > "${smoke_output}" 2>&1
smoke_status=$?
set -e
[[ ${smoke_status} -eq 1 ]] || fail "mixed smoke status was ${smoke_status}, expected 1"
[[ -f "${fixture_root}/third-ran" ]] || fail 'third runner did not execute after a failure'
for case_name in pass_one fail_seven pass_three; do
    [[ -f "${smoke_log_dir}/${case_name}.log" ]] || fail "missing smoke log for ${case_name}"
done
require_literal 'PASS  pass_one' "${smoke_output}"
require_literal 'FAIL  fail_seven (status=7)' "${smoke_output}"
require_literal 'PASS  pass_three' "${smoke_output}"
require_literal 'total=3 passed=2 failed=1' "${smoke_output}"

smoke_log_dir="${fixture_root}/all-pass-logs"
all_pass_output="${fixture_root}/all-pass.out"
run_smoke_examples pass_one pass_three > "${all_pass_output}" 2>&1 || fail 'all-pass smoke returned nonzero'
require_literal 'total=2 passed=2 failed=0' "${all_pass_output}"

main_output="${fixture_root}/main-args.out"
set +e
bash "${run_smoke_script}" unexpected > "${main_output}" 2>&1
main_status=$?
set -e
[[ ${main_status} -eq 2 ]] || fail "run_smoke main argument rejection returned ${main_status}, expected 2"
require_literal 'usage:' "${main_output}"

skip_build_output="${fixture_root}/skip-asc-tools-build.out"
(
    unset ASCEND_HOME_PATH
    export NPU_COMPUTE_SMOKE_CLI="${fake_npu_compute}"
    source "${run_smoke_script}"
    run_smoke_examples()
    {
        printf 'skip asc-tools build fixture ran: %s\n' "$*"
    }
    main --skip-asc-tools-build
) > "${skip_build_output}" 2>&1 || fail 'run_smoke --skip-asc-tools-build did not use the installed CLI'
require_literal 'skip asc-tools build fixture ran: vector_add cube_mmad mix_1_1 mix_1_2 reg_add simt_hello' \
    "${skip_build_output}"

run_case_args_output="${fixture_root}/run-case-args.out"
set +e
bash "${run_case_script}" --skip-build > "${run_case_args_output}" 2>&1
run_case_args_status=$?
set -e
[[ ${run_case_args_status} -eq 2 ]] || \
    fail "run_case.sh argument rejection returned ${run_case_args_status}, expected 2"
require_literal 'usage:' "${run_case_args_output}"

printf 'npu-compute smoke runner fixture checks passed\n'
