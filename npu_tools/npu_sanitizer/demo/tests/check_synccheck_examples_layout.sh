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
synccheck_dir="${demo_dir}/examples/synccheck"
readme="${synccheck_dir}/README.md"
readme_en="${synccheck_dir}/README_en.md"
synccheck_examples=(
    no_sync
    single_pair
    single_unconsumed
    duplicate_set
    multi_block_isolation
    wait_without_set
    aic_wait_without_set
    mix_wait_without_set
    mutex_pair
    mutex_unreleased
    aic_mutex_unreleased
    mix_mutex_unreleased
    mutex_unlock_without_lock
    mutex_duplicate_lock
    mutex_multi_block_isolation
)

for example in "${synccheck_examples[@]}"; do
    example_dir="${synccheck_dir}/${example}"
    test -f "${example_dir}/${example}.asc"
    test -f "${example_dir}/CMakeLists.txt"
    test -x "${example_dir}/run.sh"
    test -x "${example_dir}/verify.py"
    test ! -e "${example_dir}/verify_common.py"
    test ! -e "${example_dir}/symbol_ordering.txt"
    if rg -q 'SynccheckExample\.cmake|add_synccheck_example' "${example_dir}/CMakeLists.txt"; then
        printf 'synccheck example still depends on a shared CMake module: %s\n' "${example}" >&2
        exit 1
    fi
    grep -Fq 'find_library(ACL_RT_LIBRARY' "${example_dir}/CMakeLists.txt"
    grep -Fq "add_executable(demo" \
        "${example_dir}/CMakeLists.txt"
    grep -Fq 'target_compile_options(demo PRIVATE' "${example_dir}/CMakeLists.txt"
    grep -Fq 'target_link_libraries(demo PRIVATE' "${example_dir}/CMakeLists.txt"
    if rg -q 'ACLSAN_DEMO_ACL_RT_DIRECTORY|RUNTIME_OUTPUT_DIRECTORY|BUILD_RPATH' \
        "${example_dir}/CMakeLists.txt"; then
        printf 'synccheck example still overrides its output or build RPATH: %s\n' "${example}" >&2
        exit 1
    fi
    grep -Fq 'cd "$(dirname "$0")"' "${example_dir}/run.sh"
    grep -Fq 'example=$(basename "$PWD")' "${example_dir}/run.sh"
    if rg -q 'source .*common\.sh|aclsan_(prepare|capture|expect|require|verify|complete)' \
        "${example_dir}/run.sh"; then
        printf 'synccheck runner still depends on common helper functions: %s\n' "${example}" >&2
        exit 1
    fi
    grep -Fq 'output="build/npu_check.log"' "${example_dir}/run.sh"
    grep -Fq 'exec > >(tee -a "${output}") 2>&1' "${example_dir}/run.sh"
    grep -Fq 'cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510' "${example_dir}/run.sh"
    grep -Fq 'cmake --build build --parallel' "${example_dir}/run.sh"
    grep -Fq '"${npu_check}" --tool synccheck -- build/demo' \
        "${example_dir}/run.sh"
    grep -Fq 'python3 verify.py "${output}"' \
        "${example_dir}/run.sh"
    if rg -q 'npu_check_status|EXPECTED_STATUS|expected npu_check status|--error-exitcode' \
        "${example_dir}/run.sh" "${example_dir}/verify.py"; then
        printf 'synccheck example still validates npu_check status: %s\n' "${example}" >&2
        exit 1
    fi
    if rg -q 'example_dir=|build_dir=|npu_check_build_dir=' "${example_dir}/run.sh"; then
        printf 'synccheck runner still uses an expanded path: %s\n' "${example}" >&2
        exit 1
    fi
    if rg -q 'ACLSAN_PROBE_|probe_resources|run_example\.sh|--strict|--keep-temp|--work-dir|--probe-cache-dir' \
        "${example_dir}/run.sh"; then
        printf 'synccheck example runner references obsolete CLI or runtime logic: %s\n' "${example}" >&2
        exit 1
    fi
    redundant_validation='invalid CANN root|missing CANN environment|missing synccheck tool artifact|missing synccheck executable|missing CANN Runtime library|for artifact in|for library in'
    if rg -q "${redundant_validation}" "${example_dir}/run.sh"; then
        printf 'synccheck example runner contains redundant validation: %s\n' "${example}" >&2
        exit 1
    fi
    grep -Fq 'sys.path.insert(0, str(Path(__file__).resolve().parents[1]))' "${example_dir}/verify.py"
    grep -Fq 'from verify_common import verify' "${example_dir}/verify.py"
    grep -Fq "EXPECTED_CASE = \"${example}\"" "${example_dir}/verify.py"
    grep -Fq "| \`${example}/\` |" "${readme}"
    grep -Fq "| \`${example}/\` |" "${readme_en}"
    grep -Fq '#include "acl/acl.h"' "${example_dir}/${example}.asc"
    grep -Fq '#include "kernel_operator.h"' "${example_dir}/${example}.asc"
    grep -Fq 'aclInit(nullptr)' "${example_dir}/${example}.asc"
    grep -Fq '// Scenario:' "${example_dir}/${example}.asc"
    if rg -q 'GlobalTensor|LocalTensor|LocalMemAllocator|DataCopy|PipeBarrier|aclrtMalloc|aclrtMemcpy|std::vector' \
        "${example_dir}/${example}.asc"; then
        printf 'synccheck example contains operations unrelated to synchronization: %s\n' "${example}" >&2
        exit 1
    fi
    bash -n "${example_dir}/run.sh"
    test ! -e "${synccheck_dir}/${example}.asc"
done

test ! -e "${synccheck_dir}/build.sh"
test -x "${demo_dir}/build.sh"
test -x "${synccheck_dir}/run_all.sh"
test ! -e "${synccheck_dir}/run_example.sh"
test ! -e "${synccheck_dir}/SynccheckExample.cmake"
test ! -e "${synccheck_dir}/CMakeLists.txt"
if rg -q 'examples/synccheck' "${demo_dir}/CMakeLists.txt"; then
    printf 'demo still includes the removed Synccheck CMake aggregate\n' >&2
    exit 1
fi
test ! -e "${demo_dir}/examples/common.sh"
test -f "${synccheck_dir}/verify_common.py"
if rg -q 'npu-check-status|expected_status|actual_status' "${synccheck_dir}/verify_common.py"; then
    printf 'synccheck common verifier still validates npu_check status\n' >&2
    exit 1
fi
test ! -e "${synccheck_dir}/symbol_ordering.txt"
test ! -e "${synccheck_dir}/build_example.sh"
grep -Fq 'if [[ ! -x "${demo_dir}/build.sh" ]]; then' "${synccheck_dir}/run_all.sh"
if rg -q 'bash .*build\.sh|SYNCCHECK_TOOL_BUILD_DIR' "${synccheck_dir}/run_all.sh"; then
    printf 'run_all.sh still builds tools or references the removed Synccheck build directory\n' >&2
    exit 1
fi
grep -Fq 'example verification passed: synccheck/${example}' "${synccheck_dir}/run_all.sh"
grep -Fq 'if [[ ${run_status} -eq 0 ]]' "${synccheck_dir}/run_all.sh"
grep -Fq 'total=%d passed=%d failed=%d' "${synccheck_dir}/run_all.sh"
grep -Fq 'run_all.sh' "${readme}"
grep -Fq 'run_all.sh' "${readme_en}"
for example in "${synccheck_examples[@]}"; do
    grep -Fq "    ${example}" "${synccheck_dir}/run_all.sh"
done

grep -Fq 'aclrtDestroyStreamForce' "${synccheck_dir}/wait_without_set/wait_without_set.asc"
grep -Fq 'aclrtDestroyStreamForce' "${synccheck_dir}/aic_wait_without_set/aic_wait_without_set.asc"
grep -Fq 'aclrtDestroyStreamForce' "${synccheck_dir}/mix_wait_without_set/mix_wait_without_set.asc"
grep -Fq 'aclrtDestroyStreamForce' "${synccheck_dir}/mutex_duplicate_lock/mutex_duplicate_lock.asc"

for example in single_pair single_unconsumed mutex_pair mutex_unreleased aic_mutex_unreleased mix_mutex_unreleased; do
    grep -Fq '#include "c_api/asc_simd.h"' "${synccheck_dir}/${example}/${example}.asc"
done
grep -Fq '__global__ __cube__' "${synccheck_dir}/single_pair/single_pair.asc"
grep -Fq 'asc_sync_notify(PIPE_MTE1, PIPE_M, EVENT_ID0)' "${synccheck_dir}/single_pair/single_pair.asc"
grep -Fq 'asc_sync_wait(PIPE_MTE1, PIPE_M, EVENT_ID0)' "${synccheck_dir}/single_pair/single_pair.asc"
grep -Fq 'asc_sync_notify(PIPE_V, PIPE_MTE2, EVENT_ID0)' \
    "${synccheck_dir}/single_unconsumed/single_unconsumed.asc"
grep -Fq 'asc_lock(PIPE_MTE2, mutexId)' "${synccheck_dir}/mutex_pair/mutex_pair.asc"
grep -Fq 'asc_unlock(PIPE_MTE2, mutexId)' "${synccheck_dir}/mutex_pair/mutex_pair.asc"
grep -Fq 'asc_lock(PIPE_V, mutexId)' "${synccheck_dir}/mutex_pair/mutex_pair.asc"
grep -Fq 'asc_unlock(PIPE_V, mutexId)' "${synccheck_dir}/mutex_pair/mutex_pair.asc"
grep -Fq 'asc_lock(PIPE_MTE2, mutexId)' "${synccheck_dir}/mutex_unreleased/mutex_unreleased.asc"
grep -Fq '__global__ __cube__' "${synccheck_dir}/aic_wait_without_set/aic_wait_without_set.asc"
grep -Fq 'HardEvent::MTE1_M' "${synccheck_dir}/aic_wait_without_set/aic_wait_without_set.asc"
grep -Fq '__global__ __cube__' "${synccheck_dir}/duplicate_set/duplicate_set.asc"
grep -Fq 'HardEvent::MTE1_M' "${synccheck_dir}/duplicate_set/duplicate_set.asc"
grep -Fq '__global__ __cube__' "${synccheck_dir}/mutex_unlock_without_lock/mutex_unlock_without_lock.asc"
grep -Fq 'Mutex::Unlock<PIPE_M>' "${synccheck_dir}/mutex_unlock_without_lock/mutex_unlock_without_lock.asc"
grep -Fq '__global__ __cube__' "${synccheck_dir}/mutex_multi_block_isolation/mutex_multi_block_isolation.asc"
grep -Fq 'Mutex::Lock<PIPE_M>' "${synccheck_dir}/mutex_multi_block_isolation/mutex_multi_block_isolation.asc"
grep -Fq 'Mutex::Unlock<PIPE_M>' "${synccheck_dir}/mutex_multi_block_isolation/mutex_multi_block_isolation.asc"
grep -Fq '__global__ __mix__(1, 1)' "${synccheck_dir}/mix_wait_without_set/mix_wait_without_set.asc"
grep -Fq 'if ASCEND_IS_AIC' "${synccheck_dir}/mix_wait_without_set/mix_wait_without_set.asc"
grep -Fq 'if ASCEND_IS_AIV' "${synccheck_dir}/mix_wait_without_set/mix_wait_without_set.asc"
grep -Fq 'HardEvent::MTE1_M' "${synccheck_dir}/mix_wait_without_set/mix_wait_without_set.asc"
grep -Fq 'HardEvent::MTE2_V' "${synccheck_dir}/mix_wait_without_set/mix_wait_without_set.asc"
grep -Fq '"sync_events": 2' "${synccheck_dir}/mix_wait_without_set/verify.py"
grep -Fq '"unmatched_closes": 2' "${synccheck_dir}/mix_wait_without_set/verify.py"
grep -Fq '"errors": 2' "${synccheck_dir}/mix_wait_without_set/verify.py"
grep -Fq '__global__ __cube__' "${synccheck_dir}/aic_mutex_unreleased/aic_mutex_unreleased.asc"
grep -Fq 'asc_lock(PIPE_M, mutexId)' "${synccheck_dir}/aic_mutex_unreleased/aic_mutex_unreleased.asc"
grep -Fq '__global__ __mix__(1, 2)' "${synccheck_dir}/mix_mutex_unreleased/mix_mutex_unreleased.asc"
grep -Fq 'if ASCEND_IS_AIC' "${synccheck_dir}/mix_mutex_unreleased/mix_mutex_unreleased.asc"
grep -Fq 'if ASCEND_IS_AIV' "${synccheck_dir}/mix_mutex_unreleased/mix_mutex_unreleased.asc"
grep -Fq 'asc_lock(PIPE_M, mutexId)' "${synccheck_dir}/mix_mutex_unreleased/mix_mutex_unreleased.asc"
grep -Fq 'asc_lock(PIPE_V, mutexId)' "${synccheck_dir}/mix_mutex_unreleased/mix_mutex_unreleased.asc"
grep -Fq '"sync_events": 3' "${synccheck_dir}/mix_mutex_unreleased/verify.py"
grep -Fq '"unconsumed_opens": 3' "${synccheck_dir}/mix_mutex_unreleased/verify.py"
grep -Fq '"errors": 3' "${synccheck_dir}/mix_mutex_unreleased/verify.py"
bash -n "${demo_dir}/build.sh"
bash -n "${synccheck_dir}/run_all.sh"

forbidden_arg_size='NPU_CHECK_DBI_''ARG_SIZE'
if rg -q "${forbidden_arg_size}" "${demo_dir}" -g '*.sh'; then
    printf 'demo scripts still configure the removed DBI argument size override\n' >&2
    exit 1
fi

current_result_fixture=$(mktemp /tmp/aclsan-synccheck-result.XXXXXX)
trap 'rm -f "${current_result_fixture}"' EXIT
printf '%s\n' \
    'npu_check: DIAGNOSTIC ========= ERROR: Synchronization pairing mismatch: redundant GET_BUF.' \
    '===== npu_check summary =====' \
    'tool=synccheck sync_events=1 synchronizations=1 matched_pairs=0 duplicate_opens=0 unmatched_closes=0 unconsumed_opens=1 pending_opens=0 errors=1 warnings=0' \
    'callbacks=2 malformed_callbacks=0 framework_errors=0 dropped_messages=0' \
    'status=complete aclsan_unsubscribe=0 dropped_messages=0 analysis_complete=true report_truncated=false' \
    '[CLI] outcome=forwarded has_errors=1 truncated=0 child_exit=0 exit=0' \
    >"${current_result_fixture}"
python3 "${synccheck_dir}/aic_mutex_unreleased/verify.py" "${current_result_fixture}"
