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

# 进入示例目录，所有构建和日志均使用相对路径。
cd "$(dirname "$0")"

# 校验 CANN 环境。
if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
    printf 'ASCEND_HOME_PATH must be set. Please source set_env.sh\n' >&2
    exit 1
fi

# 准备构建目录并记录 npu-check 运行日志。
rm -rf build
mkdir -p build
output="build/npu_check.log"
: >"${output}"

export ASCEND_GLOBAL_LOG_LEVEL=0
export ASCEND_SLOG_PRINT_TO_STDOUT=0

# 配置并构建示例。
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel

# 第一个 launch 触发 memcheck，第二个 launch 留下未消费 SET_FLAG；二者只在一次 synchronize 后结算。
set +e
npu-check --tool memcheck --tool synccheck -- build/demo 2>&1 | tee "${output}"
run_status=${PIPESTATUS[0]}
set -e
if [[ ${run_status} -ne 0 ]]; then
    printf 'npu-check exited with status %d\n' "${run_status}" >&2
    exit 1
fi

if [[ $(grep -Ec '^tool=memcheck .*synchronizations=1 .*errors=1([[:space:]]|$)' "${output}" || true) -ne 1 ]]; then
    printf 'missing memcheck summary with synchronizations=1 and errors=1: %s\n' "${output}" >&2
    exit 1
fi

if [[ $(grep -Ec '^tool=synccheck sync_events=1 synchronizations=1 matched_pairs=0 duplicate_opens=0 unmatched_closes=0 unconsumed_opens=1 .*errors=1([[:space:]]|$)' "${output}" || true) -ne 1 ]]; then
    printf 'missing synccheck aggregate summary: %s\n' "${output}" >&2
    exit 1
fi

for diagnostic in \
    '========= ERROR:[MEMCHECK] Invalid GM read of size 32 bytes' \
    '========= ERROR:[SYNCCHECK] Synchronization pairing mismatch: redundant SET_FLAG.'; do
    if [[ $(grep -Fxc "${diagnostic}" "${output}" || true) -ne 1 ]]; then
        printf 'unexpected diagnostic count for %s: %s\n' "${diagnostic}" "${output}" >&2
        exit 1
    fi
done

if [[ $(grep -Fxc '[CLI] outcome=forwarded has_errors=1 truncated=0 child_exit=0 exit=0' "${output}" || true) -ne 1 ]]; then
    printf 'unexpected npu-check result: %s\n' "${output}" >&2
    exit 1
fi
if [[ $(grep -Fxc 'status=complete aclsan_unsubscribe=0 dropped_messages=0 analysis_complete=true report_truncated=false' \
    "${output}" || true) -ne 1 ]]; then
    printf 'incomplete npu-check session: %s\n' "${output}" >&2
    exit 1
fi

printf '[PASSED] basic_func/dual_tool_multi_launch_aggregate\n'
