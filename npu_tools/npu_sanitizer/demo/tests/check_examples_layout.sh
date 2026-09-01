#!/usr/bin/env bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
output=$(mktemp)
smoke_fixture_dir=$(mktemp -d)
trap 'rm -f -- "${output}"; rm -rf -- "${smoke_fixture_dir}"' EXIT

test -x "${demo_dir}/build.sh"
test -f "${demo_dir}/examples/memcheck/add/add.asc"
test -x "${demo_dir}/examples/memcheck/add/run.sh"
test -f "${demo_dir}/examples/memcheck/datacopy_stride/datacopy_stride.asc"
test -f "${demo_dir}/examples/memcheck/datacopy_stride/CMakeLists.txt"
test -x "${demo_dir}/examples/memcheck/datacopy_stride/run.sh"
test -x "${demo_dir}/tests/check_datacopy_stride_end_to_end.sh"
test -f "${demo_dir}/examples/basic_func/multi_kernel/multi_kernel.asc"
test -f "${demo_dir}/examples/basic_func/multi_kernel/CMakeLists.txt"
test -x "${demo_dir}/examples/basic_func/multi_kernel/run.sh"
test -f "${demo_dir}/examples/basic_func/padding_register_state/padding_register_state.asc"
test -x "${demo_dir}/examples/basic_func/padding_register_state/run.sh"
test -f "${demo_dir}/examples/basic_func/dual_tool_multi_launch_aggregate/dual_tool_multi_launch_aggregate.asc"
test -f "${demo_dir}/examples/basic_func/dual_tool_multi_launch_aggregate/CMakeLists.txt"
test -x "${demo_dir}/examples/basic_func/dual_tool_multi_launch_aggregate/run.sh"
test ! -e "${demo_dir}/examples/test"
test -x "${demo_dir}/examples/memcheck/matmul_basic_api/run.sh"
test -x "${demo_dir}/examples/memcheck/matmul_leakyrelu_basic_api/run.sh"
for example in matmul_basic_api matmul_leakyrelu_basic_api; do
    example_dir="${demo_dir}/examples/memcheck/${example}"
    grep -Fq 'add_executable(demo' "${example_dir}/CMakeLists.txt"
    grep -Fq 'target_link_libraries' "${example_dir}/CMakeLists.txt"
    grep -Fq -- '-- ./demo' "${example_dir}/run.sh"
    if rg -q 'NPU_CHECK_DBI_SOURCE_ROOT|dbi_runtime_sources' "${example_dir}/run.sh"; then
        printf '%s runner still depends on an external Probe source directory\n' "${example}" >&2
        exit 1
    fi
    if grep -Fq 'build/run' "${example_dir}/run.sh"; then
        printf '%s runner must use build directly\n' "${example}"
        exit 1
    fi
    grep -Fq 'scripts/gen_data.py' "${example_dir}/run.sh"
    grep -Fq 'scripts/verify_result.py' "${example_dir}/run.sh"
    grep -Fq 'cd "$(dirname "$0")"' "${example_dir}/run.sh"
    grep -Fq 'cmake -B build' "${example_dir}/run.sh"
    bash -n "${example_dir}/run.sh"
done
if grep -Fq 'examples/test' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo CMake still references the removed examples/test directory\n' >&2
    exit 1
fi
if grep -Fq 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo CMake still registers example subdirectories\n' >&2
    exit 1
fi
standalone_examples=(
    basic_func/multi_kernel
    basic_func/padding_register_state
    basic_func/dual_tool_multi_launch_aggregate
    memcheck/add
    memcheck/datacopy_stride
    memcheck/matmul_basic_api
    memcheck/matmul_leakyrelu_basic_api
)
for example in "${standalone_examples[@]}"; do
    example_dir="${demo_dir}/examples/${example}"
    cmake_file="${example_dir}/CMakeLists.txt"
    runner="${example_dir}/run.sh"
    grep -Fq 'cmake_minimum_required(VERSION 3.16)' "${cmake_file}"
    grep -Fq 'find_package(ASC REQUIRED)' "${cmake_file}"
    grep -Fq 'find_library(ACL_RT_LIBRARY' "${cmake_file}"
    grep -Fq 'HINTS $ENV{ASCEND_HOME_PATH}/lib64' "${cmake_file}"
    if grep -Fq 'ASCEND_CANN_PACKAGE_LINUX_PATH' "${cmake_file}"; then
        printf 'standalone example still uses the secondary CANN package path: %s\n' "${example}" >&2
        exit 1
    fi
    grep -Fq 'target_link_libraries' "${cmake_file}"
    grep -Fq 'target_link_libraries(demo PRIVATE' "${cmake_file}"
    grep -Fq '${ACL_RT_LIBRARY}' "${cmake_file}"
    if rg -q 'ACLSAN_DEMO_ACL_RT_DIRECTORY|RUNTIME_OUTPUT_DIRECTORY|BUILD_RPATH' "${cmake_file}"; then
        printf 'standalone example still overrides its output or build RPATH: %s\n' "${example}" >&2
        exit 1
    fi
    if grep -Fq 'acl_runtime_backend' "${cmake_file}"; then
        printf 'standalone example still links the shared-tree target: %s\n' "${example}" >&2
        exit 1
    fi
done
grep -Fq 'add_executable(demo' \
    "${demo_dir}/examples/memcheck/datacopy_stride/CMakeLists.txt"
grep -Fq 'add_executable(demo' \
    "${demo_dir}/examples/basic_func/multi_kernel/CMakeLists.txt"
datacopy_stride_source="${demo_dir}/examples/memcheck/datacopy_stride/datacopy_stride.asc"
grep -Fq '#define ACL_CHECK' "${datacopy_stride_source}"
grep -Fq 'ACL_CHECK(aclInit(nullptr));' "${datacopy_stride_source}"
if grep -Eq 'RuntimeResources|InitializeRuntimeResources|ReleaseRuntimeResources' "${datacopy_stride_source}"; then
    printf 'datacopy_stride still hides ACL Runtime calls behind resource helpers\n' >&2
    exit 1
fi
if [[ -e "${demo_dir}/run.sh" ]]; then
    printf 'obsolete top-level demo/run.sh still exists\n' >&2
    exit 1
fi
bash -n "${demo_dir}/build.sh"
test ! -e "${demo_dir}/examples/common.sh"

while IFS= read -r cmake_file; do
    grep -Fq 'HINTS $ENV{ASCEND_HOME_PATH}/lib64' "${cmake_file}"
    if grep -Fq 'ASCEND_CANN_PACKAGE_LINUX_PATH' "${cmake_file}"; then
        printf 'example CMake still uses the secondary CANN package path: %s\n' "${cmake_file}" >&2
        exit 1
    fi
    if rg -q 'ACLSAN_DEMO_ACL_RT_DIRECTORY|RUNTIME_OUTPUT_DIRECTORY|BUILD_RPATH' "${cmake_file}"; then
        printf 'example CMake still overrides its output or build RPATH: %s\n' "${cmake_file}" >&2
        exit 1
    fi
done < <(rg -l 'find_library\(ACL_RT_LIBRARY' "${demo_dir}/examples" -g 'CMakeLists.txt')

legacy_cann_root='NPUCOMPUTE_CANN_''ROOT'
if rg -q "${legacy_cann_root}" "${demo_dir}" -g '!**/build/**'; then
    printf 'demo still references the private CANN root variable\n' >&2
    exit 1
fi

for runner in "${demo_dir}"/examples/basic_func/*/run.sh "${demo_dir}"/examples/memcheck/*/run.sh; do
    if rg -q 'source .*common\.sh|aclsan_(prepare|capture|expect|require|verify|complete)' "${runner}"; then
        printf 'example runner still depends on common helper functions: %s\n' "${runner}" >&2
        exit 1
    fi
    grep -Fq 'cd "$(dirname "$0")"' "${runner}"
    grep -Fq 'output="build/npu_check.log"' "${runner}"
    grep -Fq 'exec > >(tee -a "${output}") 2>&1' "${runner}"
    grep -Fq 'cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510' "${runner}"
    grep -Fq 'cmake --build build --parallel' "${runner}"
    grep -Fq 'set +e' "${runner}"
    if rg -q 'npu_check_status|npu_check raw status|expected npu_check status|--error-exitcode' "${runner}"; then
        printf 'example runner still validates npu_check status: %s\n' "${runner}" >&2
        exit 1
    fi
    if rg -q 'example_dir=|npu_check_build_dir=' "${runner}"; then
        printf 'example runner still uses an expanded path: %s\n' "${runner}" >&2
        exit 1
    fi
    bash -n "${runner}"
done

add_runner="${demo_dir}/examples/memcheck/add/run.sh"
grep -Fq '^tool=memcheck .*errors=1' "${add_runner}"
grep -Fq "Invalid GM read of size 8256 bytes" "${add_runner}"
grep -Fq "'test pass!'" "${add_runner}"

datacopy_runner="${demo_dir}/examples/memcheck/datacopy_stride/run.sh"
grep -Fq '^tool=memcheck .*errors=4' "${datacopy_runner}"
grep -Fq "Invalid GM read of size 32 bytes" "${datacopy_runner}"

multi_kernel_runner="${demo_dir}/examples/basic_func/multi_kernel/run.sh"
if rg -q 'source .*common\.sh|aclsan_(prepare|capture|expect|require|verify|complete)' \
    "${multi_kernel_runner}"; then
    printf 'multi_kernel runner still depends on common helper functions\n' >&2
    exit 1
fi
grep -Fq 'cd "$(dirname "$0")"' "${multi_kernel_runner}"
grep -Fq 'output="build/npu_check.log"' "${multi_kernel_runner}"
grep -Fq 'exec > >(tee -a "${output}") 2>&1' "${multi_kernel_runner}"
grep -Fq 'cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510' "${multi_kernel_runner}"
grep -Fq 'cmake --build build --parallel' "${multi_kernel_runner}"
grep -Fq 'build/demo' "${multi_kernel_runner}"
grep -Fq '^tool=memcheck .*errors=2' "${multi_kernel_runner}"
grep -Fq 'Invalid GM read of size 32 bytes' "${multi_kernel_runner}"
grep -Fq 'DataCopyStrideSingleInput' "${multi_kernel_runner}"
grep -Fq 'DataCopyStrideDualInput' "${multi_kernel_runner}"
grep -Fq 'launch_count=' "${multi_kernel_runner}"

padding_runner="${demo_dir}/examples/basic_func/padding_register_state/run.sh"
grep -Fq '^tool=memcheck .*errors=0' "${padding_runner}"
grep -Fq 'value=0x12' "${padding_runner}"
grep -Fq 'value=0x34' "${padding_runner}"

dual_tool_runner="${demo_dir}/examples/basic_func/dual_tool_multi_launch_aggregate/run.sh"
grep -Fq -- '--tool memcheck --tool synccheck -- build/demo' "${dual_tool_runner}"
grep -Fq '^tool=memcheck .*synchronizations=1.*errors=1' "${dual_tool_runner}"
grep -Fq '^tool=synccheck .*sync_events=1 synchronizations=1.*unconsumed_opens=1.*errors=1' "${dual_tool_runner}"
grep -Fq 'Invalid GM read of size 32 bytes' "${dual_tool_runner}"
grep -Fq 'Synchronization pairing mismatch: redundant SET_FLAG.' "${dual_tool_runner}"

for example in matmul_basic_api matmul_leakyrelu_basic_api; do
    matmul_runner="${demo_dir}/examples/memcheck/${example}/run.sh"
    grep -Fq '^tool=memcheck .*errors=0' "${matmul_runner}"
    grep -Fq "grep -Fq 'unsupported raw trace'" "${matmul_runner}"
done

smoke_runner="${demo_dir}/run_smoke.sh"
test -x "${smoke_runner}"
bash -n "${smoke_runner}"
grep -Fq 'run_smoke_examples()' "${smoke_runner}"
smoke_examples=(
    memcheck/add
    memcheck/datacopy_stride
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
for example in "${smoke_examples[@]}"; do
    grep -Fq "    ${example}" "${smoke_runner}"
done

workflow_fixture_dir="${smoke_fixture_dir}/workflow"
mkdir -p "${workflow_fixture_dir}/build"
touch "${workflow_fixture_dir}/build/stale"
cp "${smoke_runner}" "${workflow_fixture_dir}/run_smoke.sh"
chmod +x "${workflow_fixture_dir}/run_smoke.sh"
printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -euo pipefail' \
    'fixture_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)' \
    'test ! -e "${fixture_dir}/build/stale"' \
    'printf "BUILD\n" >>"${fixture_dir}/trace.log"' \
    'export ASCEND_HOME_PATH="${fixture_dir}/fake-cann"' \
    'mkdir -p "${fixture_dir}/build/npu_tools/bin"' \
    'printf "#!/usr/bin/env bash\n" >"${fixture_dir}/build/npu_tools/bin/npu_check"' \
    'chmod +x "${fixture_dir}/build/npu_tools/bin/npu_check"' \
    >"${workflow_fixture_dir}/build.sh"
chmod +x "${workflow_fixture_dir}/build.sh"

for example in "${smoke_examples[@]}"; do
    fixture_runner_dir="${workflow_fixture_dir}/examples/${example}"
    mkdir -p "${fixture_runner_dir}"
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'set -euo pipefail' \
        'runner_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)' \
        'fixture_dir=$(cd "${runner_dir}/../../.." && pwd)' \
        'test "${ASCEND_HOME_PATH}" = "${fixture_dir}/fake-cann"' \
        'printf "RUN %s\n" "${runner_dir#"${fixture_dir}/examples/"}" >>"${fixture_dir}/trace.log"' \
        >"${fixture_runner_dir}/run.sh"
    chmod +x "${fixture_runner_dir}/run.sh"
done

set +e
bash "${workflow_fixture_dir}/run_smoke.sh" >"${output}" 2>&1
workflow_status=$?
set -e
if [[ ${workflow_status} -ne 0 ]]; then
    printf 'expected self-contained smoke workflow status 0, got %s\n' "${workflow_status}" >&2
    cat "${output}" >&2
    exit 1
fi
test ! -e "${workflow_fixture_dir}/build/stale"
test "$(sed -n '1p' "${workflow_fixture_dir}/trace.log")" = "BUILD"
test "$(grep -c '^RUN ' "${workflow_fixture_dir}/trace.log")" -eq 17
grep -Fq 'total=17 passed=17 failed=0' "${output}"

printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -euo pipefail' \
    'fixture_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)' \
    'printf "BUILD_FAILED\n" >"${fixture_dir}/trace.log"' \
    'false' \
    >"${workflow_fixture_dir}/build.sh"
set +e
bash "${workflow_fixture_dir}/run_smoke.sh" >"${output}" 2>&1
workflow_status=$?
set -e
if [[ ${workflow_status} -eq 0 ]]; then
    printf 'expected smoke workflow to fail when build.sh fails\n' >&2
    exit 1
fi
grep -Fxq 'BUILD_FAILED' "${workflow_fixture_dir}/trace.log"
if grep -Fq 'RUN ' "${workflow_fixture_dir}/trace.log"; then
    printf 'smoke workflow ran an example after build.sh failed\n' >&2
    exit 1
fi

mkdir -p "${workflow_fixture_dir}/build"
touch "${workflow_fixture_dir}/build/sentinel"
: >"${workflow_fixture_dir}/trace.log"
set +e
bash "${workflow_fixture_dir}/run_smoke.sh" unexpected >"${output}" 2>&1
workflow_status=$?
set -e
if [[ ${workflow_status} -ne 2 ]]; then
    printf 'expected invalid smoke arguments status 2, got %s\n' "${workflow_status}" >&2
    exit 1
fi
test -e "${workflow_fixture_dir}/build/sentinel"
test ! -s "${workflow_fixture_dir}/trace.log"

for fixture in memcheck/pass basic_func/fail synccheck/after; do
    mkdir -p "${smoke_fixture_dir}/examples/${fixture}"
done
printf '%s\n' '#!/usr/bin/env bash' 'printf "fixture pass\\n"' \
    >"${smoke_fixture_dir}/examples/memcheck/pass/run.sh"
printf '%s\n' '#!/usr/bin/env bash' 'printf "fixture failure\\n"' 'exit 3' \
    >"${smoke_fixture_dir}/examples/basic_func/fail/run.sh"
printf '%s\n' '#!/usr/bin/env bash' 'printf "fixture after failure\\n"' \
    >"${smoke_fixture_dir}/examples/synccheck/after/run.sh"
chmod +x "${smoke_fixture_dir}"/examples/*/*/run.sh

source "${smoke_runner}"
examples_dir="${smoke_fixture_dir}/examples"
smoke_log_dir="${smoke_fixture_dir}/logs-with-failure"
set +e
run_smoke_examples memcheck/pass basic_func/fail synccheck/after >"${output}" 2>&1
smoke_status=$?
set -e
if [[ ${smoke_status} -ne 1 ]]; then
    printf 'expected failed smoke suite status 1, got %s\n' "${smoke_status}" >&2
    exit 1
fi
grep -Fq 'fixture after failure' "${smoke_log_dir}/synccheck/after.log"
grep -Fq 'PASS  memcheck/pass' "${output}"
grep -Fq 'FAIL  basic_func/fail (status=3)' "${output}"
grep -Fq 'PASS  synccheck/after' "${output}"
grep -Fq 'total=3 passed=2 failed=1' "${output}"

smoke_log_dir="${smoke_fixture_dir}/logs-all-pass"
run_smoke_examples memcheck/pass synccheck/after >"${output}" 2>&1
grep -Fq 'total=2 passed=2 failed=0' "${output}"
