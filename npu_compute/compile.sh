#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CANN_ROOT="${NPUCOMPUTE_CANN_ROOT:-/usr/local/Ascend/ascend-toolkit/latest}"
CANN_ENV_SCRIPT="${NPU_COMPUTE_CANN_ENV_SCRIPT:-/usr/local/Ascend/cann-9.2.0/bin/setenv.bash}"
BUILD_JOBS="${NPU_COMPUTE_BUILD_JOBS:-$(nproc)}"

set +u
source "${CANN_ENV_SCRIPT}"
set -u

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -U NPU_COMPUTE_BUILD_CANN_BACKEND \
    -U NPU_COMPUTE_BUILD_INTEGRATION_STUBS \
    -DNPUCOMPUTE_CANN_ROOT="${CANN_ROOT}" \
    -DNPU_COMPUTE_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build "${BUILD_DIR}" \
    --target npu_compute npu_compute_cli -j"${BUILD_JOBS}"
