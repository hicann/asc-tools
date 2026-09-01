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

cd "$(dirname "$0")"

if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
    printf 'ASCEND_HOME_PATH must be set. Please source set_env.sh\n' >&2
    exit 1
fi

rm -rf build
mkdir -p build
output="build/npu_check.log"
: >"${output}"
exec > >(tee -a "${output}") 2>&1

export ASCEND_GLOBAL_LOG_LEVEL=0
export NPU_SAN_DEBUG=1

NPU_CHECK_E2E_REUSE_EXISTING_BUILD=1 bash ../../../tests/check_memory_access_end_to_end.sh smoke

printf '[PASSED] memcheck/memory_access\n'
