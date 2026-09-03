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

# 准备构建目录并记录完整运行日志。
rm -rf build
mkdir -p build
output="build/npu_check.log"
: >"${output}"
exec > >(tee -a "${output}") 2>&1

export ASCEND_GLOBAL_LOG_LEVEL=0
export NPU_SAN_DEBUG=1
# 冒烟断言依赖 [CLI] / [UDS] 的过程记录，而它们默认不打屏，必须显式打开。
# 结果摘要行 [CLI] outcome=... 不受该开关控制，任何路径下都会输出。
export NPU_CHECK_CLI_DEBUG=1

# 配置并构建示例。
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel

# 执行包含受控越界读的 add 示例。
set +e
npu-check --tool memcheck -- build/demo
set -e

# 关注 summary：本例只有 1 个逻辑内存错误，errors 应为 1。
if [[ $(grep -Ec '^tool=memcheck .*errors=1([[:space:]]|$)' "${output}" || true) -ne 1 ]]; then
    printf 'missing memcheck summary with errors=1: %s\n' "${output}" >&2
    exit 1
fi

# 关注会话完整性：handshake、result 和 CLI 转发记录都应各出现 1 次。
if [[ $(grep -Ec '^\[UDS\] phase=handshake .* result=ok$' "${output}" || true) -ne 1 ||
    $(grep -Ec '^\[UDS\] phase=result .* has_errors=[01]$' "${output}" || true) -ne 1 ||
    $(grep -Ec '^\[CLI\] outcome=forwarded has_errors=[01] truncated=[01] child_exit=0 exit=(0|2)$' \
        "${output}" || true) -ne 1 ]]; then
    printf 'incomplete npu_check session: %s\n' "${output}" >&2
    exit 1
fi

# 关注访问大小和日志数量：1 个逻辑错误在完整报告中应出现 1 条 8256-byte GM 越界读标题。
if [[ $(grep -Fc 'Invalid GM read of size 8256 bytes' "${output}" || true) -ne 1 ]]; then
    printf 'unexpected Invalid GM read diagnostic count: %s\n' "${output}" >&2
    exit 1
fi

# 关注计算结果：8 * 2048 个 float 输出应逐元素严格等于 x + y，并输出 test pass!。
if ! grep -Fq 'test pass!' "${output}"; then
    printf 'missing expected output in %s: test pass!\n' "${output}" >&2
    exit 1
fi

printf '[PASSED] memcheck/add\n'
