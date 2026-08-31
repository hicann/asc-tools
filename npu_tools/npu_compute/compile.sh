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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CANN_ROOT="${NPUCOMPUTE_CANN_ROOT:-${ASCEND_HOME_PATH:-}}"
BUILD_JOBS="${NPU_COMPUTE_BUILD_JOBS:-$(nproc)}"

if [[ -z "${CANN_ROOT}" ]]; then
    echo "NPUCOMPUTE_CANN_ROOT or ASCEND_HOME_PATH must be set" >&2
    exit 1
fi

cmake -S "${SCRIPT_DIR}/.." -B "${BUILD_DIR}" \
    -U NPU_COMPUTE_BUILD_CANN_BACKEND \
    -DINJECTION_CANN_ROOT="${CANN_ROOT}" \
    -DNPUCOMPUTE_CANN_ROOT="${CANN_ROOT}" \
    -DASC_TOOLS_BUILD_NPU_COMPUTE=ON \
    -DNPU_COMPUTE_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build "${BUILD_DIR}" \
    --target npu_compute npu_compute_cli -j"${BUILD_JOBS}"
