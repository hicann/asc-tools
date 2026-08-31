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

npu_check="../../../build/npu_tools/bin/npu_check"

# 设置 DBI 运行环境。
export ASCEND_GLOBAL_LOG_LEVEL=0
export NPU_SAN_DEBUG=1
export NPU_CHECK_DBI_ARCH="${NPU_CHECK_DBI_ARCH:-dav-3510}"
export NPU_CHECK_DBI_TOOLCHAIN_ROOT="${NPU_CHECK_DBI_TOOLCHAIN_ROOT:-${ASCEND_HOME_PATH}}"

# 配置并构建 Synccheck 用例。
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel

# 8 个 AIC 实例各执行 1 组 SET_FLAG/WAIT_FLAG，共 16 个事件和 8 个成功配对。
set +e
"${npu_check}" --tool synccheck -- build/demo
set -e

# 关注 summary：sync_events=16、synchronizations=1、matched_pairs=8、duplicate_opens=0，
# unmatched_closes=0、unconsumed_opens=0、errors=0。
# 输出中不应有任何 npu_check DIAGNOSTIC，并且 child/session 生命周期必须完整。
python3 verify.py "${output}"

printf '[PASSED] synccheck/%s\n' "${example}"
