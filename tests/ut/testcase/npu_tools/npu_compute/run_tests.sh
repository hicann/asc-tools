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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd -P)"
BUILD_DIR="/tmp/asc_tools_npu_compute_integration"

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
  -DENABLE_TEST=ON

cmake --build "${BUILD_DIR}" -j2

ctest --test-dir "${BUILD_DIR}" --output-on-failure

NPU_COMPUTE_BUILD_DIR="${BUILD_DIR}" \
NPU_COMPUTE_TEST_BUILD_DIR="${BUILD_DIR}" \
NPU_COMPUTE_TEST_BIN_DIR="${BUILD_DIR}/tests/ut/testcase/npu_tools/bin" \
python3 -m pytest -q -p no:cacheprovider \
  "${REPO_ROOT}/tests/py_ut/testcase/npu_compute"
