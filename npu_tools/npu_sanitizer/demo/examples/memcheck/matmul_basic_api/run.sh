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

# 准备构建目录、数据目录和完整运行日志。
rm -rf build
mkdir -p build
output="build/npu_check.log"
: >"${output}"
exec > >(tee -a "${output}") 2>&1

# 设置 DBI 运行环境。
export ASCEND_GLOBAL_LOG_LEVEL=0
export NPU_SAN_DEBUG=1
# 冒烟断言依赖 [CLI] / [UDS] 的过程记录，而它们默认不打屏，必须显式打开。
# 结果摘要行 [CLI] outcome=... 不受该开关控制，任何路径下都会输出。
export NPU_CHECK_CLI_DEBUG=1

# 配置并构建示例。
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel

# 生成 [256, 64] * [64, 256] 的 FP16 输入；golden 使用 FP32 Matmul 后转为 FP16。
(cd build && python3 ../scripts/gen_data.py)

# 从数据目录运行不应报错的 memcheck 示例。
set +e
(cd build && ../../../../build/npu_tools/bin/npu_check --tool memcheck -- ./demo)
set -e

# 关注 summary：逻辑错误总数 errors 应为 0。
if [[ $(grep -Ec '^tool=memcheck .*errors=0([[:space:]]|$)' "${output}" || true) -ne 1 ]]; then
    printf 'missing memcheck summary with errors=0: %s\n' "${output}" >&2
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

# 所有 raw trace 都必须被当前 decoder 支持。
if grep -Fq 'unsupported raw trace' "${output}"; then
    printf 'unexpected unsupported raw trace: %s\n' "${output}" >&2
    exit 1
fi

# 关注数值结果：逐元素使用 rtol=1e-6、atol=1e-9 比较，错误元素比例必须 <= 1e-4。
python3 scripts/verify_result.py \
    build/output/output.bin build/output/golden.bin

printf '[PASSED] memcheck/matmul_basic_api\n'
