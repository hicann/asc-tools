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

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <production-map> <test-map>\n' "${0##*/}" >&2
    exit 2
fi

production_map=$1
test_map=$2
symbols=(
    aclsanTestMarkInstrumentedFunction
    aclsanTestRecordDeviceBinarySource
    aclsanTestResetTraceRuntimeState
)

for symbol in "${symbols[@]}"; do
    if grep -Fq "${symbol}" "${production_map}"; then
        printf 'test symbol must not be present in production map: %s\n' "${symbol}" >&2
        exit 1
    fi
    if ! grep -Fq "${symbol}" "${test_map}"; then
        printf 'test symbol is missing from test map: %s\n' "${symbol}" >&2
        exit 1
    fi
done
