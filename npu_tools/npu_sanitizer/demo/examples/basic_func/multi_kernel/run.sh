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

# 执行预期检出非法访问的 memcheck 示例。
set +e
npu-check --tool memcheck -- build/demo 2>&1 | tee "${output}"
run_status=${PIPESTATUS[0]}
set -e
if [[ ${run_status} -ne 0 ]]; then
    printf 'npu-check exited with status %d\n' "${run_status}" >&2
    exit 1
fi

# 关注 summary 的逻辑错误总数：两个 kernel 各产生 1 个错误，errors 应为 2。
if [[ $(grep -Ec '^tool=memcheck .*errors=2([[:space:]]|$)' "${output}" || true) -ne 1 ]]; then
    printf 'missing memcheck summary with errors=2: %s\n' "${output}" >&2
    exit 1
fi

# 关注非法访问的大小和报告次数：kernel1 与 kernel2 的 AIC 各报告 1 次 32-byte GM 越界读。
if [[ $(grep -Fxc '========= ERROR:[MEMCHECK] Invalid GM read of size 32 bytes' "${output}" || true) -ne 2 ]]; then
    printf 'unexpected Invalid GM read diagnostic count: %s\n' "${output}" >&2
    exit 1
fi

# 关注调用栈归属：两次错误应分别定位到两个 kernel，不能合并或漏报。
for expected_text in DataCopyStrideSingleInput DataCopyStrideDualInput; do
    if ! grep -Fq "${expected_text}" "${output}"; then
        printf 'missing expected output in %s: %s\n' "${output}" "${expected_text}" >&2
        exit 1
    fi
done

# 客户可见的会话和 CLI 结果必须完整，不能用内部 trace 日志作为样例判据。
if [[ $(grep -Fxc 'status=complete aclsan_unsubscribe=0 dropped_messages=0 analysis_complete=true report_truncated=false' \
    "${output}" || true) -ne 1 ]]; then
    printf 'incomplete npu-check session: %s\n' "${output}" >&2
    exit 1
fi
if [[ $(grep -Fxc '[CLI] outcome=forwarded has_errors=1 truncated=0 child_exit=0 exit=0' \
    "${output}" || true) -ne 1 ]]; then
    printf 'unexpected npu-check result: %s\n' "${output}" >&2
    exit 1
fi

printf '[PASSED] basic_func/multi_kernel\n'
