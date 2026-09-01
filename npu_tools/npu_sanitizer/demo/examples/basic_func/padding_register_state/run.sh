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

npu_check="../../../build/npu_tools/bin/npu_check"

# 设置 DBI 运行环境。
export ASCEND_GLOBAL_LOG_LEVEL=0
export NPU_SAN_DEBUG=1
# 冒烟断言依赖 [CLI] / [UDS] 的过程记录，而它们默认不打屏，必须显式打开。
# 结果摘要行 [CLI] outcome=... 不受该开关控制，任何路径下都会输出。
export NPU_CHECK_CLI_DEBUG=1

# 配置并构建示例。
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel

# 执行不应产生内存错误的 SET_PADDING 状态跟踪示例。
set +e
"${npu_check}" --tool memcheck -- build/demo
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

# 关注参数解码和 Device 结果：依次识别 0x12、0x34，应用侧最终输出 pass。
for expected_text in \
    '[param] type=SetPaddingParamField value=0x12' \
    '[param] type=SetPaddingParamField value=0x34' \
    'padding register state demo pass!'; do
    if ! grep -Fq "${expected_text}" "${output}"; then
        printf 'missing expected output in %s: %s\n' "${output}" "${expected_text}" >&2
        exit 1
    fi
done

# instrId=392 必须被识别为 SET_PADDING，不能出现 unsupported raw trace。
if grep -Fq 'unsupported raw trace instrId=392' "${output}"; then
    printf 'unexpected unsupported SET_PADDING trace: %s\n' "${output}" >&2
    exit 1
fi

# 关注寄存器状态更新：0x12 和 0x34 两次更新都必须存在。
first_update=$(grep -m 1 -E \
    '\[register\] action=update register=set_padding .*value=0x12' "${output}" || true)
second_update=$(grep -m 1 -E \
    '\[register\] action=update register=set_padding .*value=0x34' "${output}" || true)
if [[ -z "${first_update}" || -z "${second_update}" ]]; then
    printf 'missing SET_PADDING register-state updates\n' >&2
    exit 1
fi

# 两次更新必须属于同一 launchId、blockType 和 blockId，证明状态键没有串核或串 launch。
first_key=$(sed -E 's/.*launchId=([0-9]+) blockType=([0-9]+) blockId=([0-9]+).*/\1 \2 \3/' \
    <<<"${first_update}")
second_key=$(sed -E 's/.*launchId=([0-9]+) blockType=([0-9]+) blockId=([0-9]+).*/\1 \2 \3/' \
    <<<"${second_update}")
if [[ "${first_key}" != "${second_key}" ]]; then
    printf 'padding updates used different register-state keys: %s != %s\n' "${first_key}" "${second_key}" >&2
    exit 1
fi

printf '[PASSED] basic_func/padding_register_state\n'
