#!/usr/bin/python3
# coding=utf-8

# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_common import verify  # noqa: E402

EXPECTED_CASE = "aic_wait_without_set"
EXPECTED_SUMMARY = {
    "sync_events": 1,
    "synchronizations": 1,
    "matched_pairs": 0,
    "duplicate_opens": 0,
    "unmatched_closes": 1,
    "unconsumed_opens": 0,
    "errors": 1,
}
EXPECTED_DIAGNOSTICS = ["Synchronization pairing mismatch: unmatched WAIT_FLAG."]


if __name__ == "__main__":
    verify(EXPECTED_CASE, EXPECTED_SUMMARY, EXPECTED_DIAGNOSTICS)
