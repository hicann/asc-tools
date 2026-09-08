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

cd "$(dirname "$0")"

if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
    printf 'ASCEND_HOME_PATH must be set. Please source set_env.sh\n' >&2
    exit 1
fi

rm -rf build
mkdir -p build
output="build/plog_check.log"
: >"${output}"
exec > >(tee -a "${output}") 2>&1

export ASCEND_PROCESS_LOG_PATH="${ASCEND_PROCESS_LOG_PATH:-$(pwd)/build/plog}"
export ASCEND_SLOG_PRINT_TO_STDOUT=0
export ASCEND_GLOBAL_LOG_LEVEL="${ASCEND_GLOBAL_LOG_LEVEL:-0}"

work_dir="$(pwd)/build/npu_check_work"
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel
pid_file="$(pwd)/build/application.pid"
npu-check --tool memcheck --work-dir "${work_dir}" -- "$(pwd)/build/plog_check" "${pid_file}" &
cli_pid=$!
wait "${cli_pid}"
pid=$(cat "${pid_file}")
if [[ ! "${pid}" =~ ^[0-9]+$ ]]; then
    printf 'invalid application PID in %s\n' "${pid_file}" >&2
    exit 1
fi
if [[ -e "${work_dir}/npu_check.log" ]]; then
    printf 'unexpected custom internal log: %s/npu_check.log\n' "${work_dir}" >&2
    exit 1
fi

shopt -s nullglob
plog_contains() {
    grep -F -- "$1" "${plog_files[@]}" >/dev/null
}
deadline=$((SECONDS + ${PLOG_FLUSH_TIMEOUT:-10}))
while true; do
    plog_files=(
        "${ASCEND_PROCESS_LOG_PATH}"/{debug,run}/plog/plog-"${pid}"_*.log
        "${ASCEND_PROCESS_LOG_PATH}"/{debug,run}/plog/plog-"${cli_pid}"_*.log
    )
    if ((${#plog_files[@]} > 0)) &&
        plog_contains '] ASCENDCKERNEL(' && plog_contains '[tool_manager.cpp:' &&
        plog_contains 'npu_check initialization completed' &&
        plog_contains 'synchronization completed' &&
        plog_contains 'status=complete' &&
        plog_contains 'aclsanSubscribe succeed' &&
        plog_contains 'aclsanUnsubscribe succeed'; then
        if [[ "${ASCEND_GLOBAL_LOG_LEVEL}" != 0 ]] ||
            { plog_contains '[process_runner.cpp:' && plog_contains 'phase=result'; }; then
            printf '[PASSED] plog_check app_pid=%s cli_pid=%s plog=%s\n' \
                "${pid}" "${cli_pid}" "${ASCEND_PROCESS_LOG_PATH}"
            exit 0
        fi
    fi
    if ((SECONDS >= deadline)); then
        break
    fi
    sleep 0.2
done

printf 'missing lifecycle plog records for app_pid=%s cli_pid=%s under %s\n' \
    "${pid}" "${cli_pid}" "${ASCEND_PROCESS_LOG_PATH}" >&2
exit 1
