#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

"""comparison_result 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_comparison_result.py -q
"""

from __future__ import annotations

import json
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from analysis.comparison_result import (
    ComparisonResultError,
    execution_issue,
    read_result,
)


@pytest.mark.parametrize(
    "payload,expected",
    [
        (
            {
                "is_pass": True,
                "matched_ratio": 0.99,
                "max_abs_error": 0.005,
                "error_elements": 1,
            },
            "PASS",
        ),
        (
            {
                "is_pass": False,
                "matched_ratio": 1.0,
                "max_abs_error": 0.02,
                "error_elements": 0,
            },
            "MISMATCH",
        ),
        ({"status": "MISMATCH", "metrics": {"error_elements": 1}}, "MISMATCH"),
        ({"status": "FAIL", "is_pass": False}, "MISMATCH"),
    ],
)
def test_retains_selected_verdict_and_original_fields(tmp_path, payload, expected):
    payload["project_specific"] = {"reason": "keep this"}
    path = tmp_path / "evaluation.json"
    original = json.dumps(payload)
    path.write_text(original)
    result = read_result(path)
    assert result["status"] == expected
    assert result["raw"] == payload
    assert path.read_text() == original
    assert result["metrics"]["mismatch_index_sha256"] is None


@pytest.mark.parametrize(
    "payload",
    [
        {"is_pass": "false"},
        {"is_pass": 1},
        {},
        {"is_pass": True, "status": "FAIL"},
        {"is_pass": False, "status": "PASS"},
        {"status": "INVALID", "is_pass": False},
    ],
)
def test_rejects_invalid_or_conflicting_verdicts(tmp_path, payload):
    path = tmp_path / "evaluation.json"
    path.write_text(json.dumps(payload))
    with pytest.raises(ComparisonResultError):
        read_result(path)


def test_missing_or_nonfinite_metrics_remain_unknown(tmp_path):
    path = tmp_path / "evaluation.json"
    path.write_text(json.dumps({"is_pass": False, "max_abs_error": float("nan")}))
    result = read_result(path)
    assert result["status"] == "MISMATCH"
    assert result["metrics"]["max_abs_error"] is None
    assert result["metrics"]["error_elements"] is None


@pytest.mark.parametrize(
    "outcome,code,declared,verdict,valid",
    [
        ("completed", 1, [1], "MISMATCH", True),
        ("completed", 1, [], "MISMATCH", False),
        ("completed", 1, [1], "PASS", False),
        ("completed", 1, [1], None, False),
        ("completed", -15, [143], "MISMATCH", False),
        ("timeout", 124, [124], "MISMATCH", False),
        ("launch_failed", 127, [127], "MISMATCH", False),
    ],
)
def test_execution_and_acceptance_are_separate(outcome, code, declared, verdict, valid):
    metadata = {
        "outcome": outcome,
        "child_returncode": code,
        "wrapper_returncode": code,
    }
    assert (execution_issue(metadata, declared, verdict) is None) is valid


def test_report_binding_rejects_old_files_within_the_same_second(tmp_path):
    from datetime import datetime, timezone
    import os
    import time
    from analysis.comparison_result import evaluation_for_execution

    path = tmp_path / "evaluation.json"
    path.write_text(json.dumps({"is_pass": False}))
    second = time.time_ns() // 1_000_000_000 * 1_000_000_000
    stamp = datetime.fromtimestamp(second / 1e9, timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )
    metadata = {
        "started_at": stamp,
        "finished_at": stamp,
        "started_at_ns": second + 500_000_000,
        "finished_at_ns": second + 900_000_000,
    }
    os.utime(path, ns=(second, second + 100_000_000))
    with pytest.raises(ComparisonResultError, match="not generated during"):
        evaluation_for_execution(path, metadata)
    os.utime(path, ns=(second, second + 700_000_000))
    assert evaluation_for_execution(path, metadata)["status"] == "MISMATCH"
