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

# 进入用例目录，所有构建和日志均使用相对路径。
cd "$(dirname "$0")"
example=$(basename "$PWD")

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

# 配置并构建 Synccheck 用例。
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel

# 同一 AIC flag 执行 SET、SET、WAIT、WAIT，组合触发重复打开和无匹配关闭。
set +e
npu-check --tool synccheck -- build/demo
npu_check_status=$?
set -e

# 关注 summary：sync_events=4、synchronizations=1、matched_pairs=1、duplicate_opens=1，
# unmatched_closes=1、unconsumed_opens=0、errors=2。
# 同时必须报告 duplicate SET_FLAG 和 unmatched WAIT_FLAG，并确认 child/session 生命周期完整。
python3 verify.py "${output}" "${npu_check_status}"

printf 'example verification passed: synccheck/%s\n' "${example}"
