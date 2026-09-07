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

smoke_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
runner="${smoke_dir}/compare_tools.sh"
comparator="${smoke_dir}/compare_reports.py"

require_literal() {
    local literal=$1
    local path=$2
    if ! grep -Fq -- "${literal}" "${path}"; then
        printf 'missing required text %q in %s\n' "${literal}" "${path}" >&2
        exit 1
    fi
}

if [[ ! -x "${runner}" ]]; then
    printf 'missing required executable: %s\n' "${runner}" >&2
    exit 1
fi
if [[ ! -f "${comparator}" ]]; then
    printf 'missing required comparator: %s\n' "${comparator}" >&2
    exit 1
fi

bash -n "${runner}"
require_literal '--npu-compute-env' "${runner}"
require_literal '--msopprof-env' "${runner}"
require_literal 'copy_collection_data' "${runner}"
require_literal 'npu-compute: data-directory=' "${runner}"
require_literal 'cp -a "${data_directories[0]}/." "${raw_output_dir}/"' "${runner}"
require_literal 'npu-compute --import' "${runner}"
require_literal '--export "${npu_import_dir}"' "${runner}"
require_literal 'PipeUtilization' "${runner}"
require_literal 'MemoryL0' "${runner}"
require_literal 'MemoryUB' "${runner}"
require_literal 'L2Cache' "${runner}"
for case_name in vector_add cube_mmad mix_1_1 mix_1_2 reg_add simt_hello; do
    require_literal "${case_name}" "${runner}"
done
if grep -Fq 'mix_1_2_msprof' "${runner}"; then
    printf 'comparison runner must exclude mix_1_2_msprof\n' >&2
    exit 1
fi
require_literal 'compare_reports.py' "${runner}"
require_literal 'msopprof' "${runner}"
require_literal 'npu-compute' "${runner}"
require_literal 'mkdir -p "${npu_raw_dir}"' "${runner}"

python3 "${smoke_dir}/tests/test_compare_tools.py"
printf 'npu-compute and msopprof comparison checks passed\n'
