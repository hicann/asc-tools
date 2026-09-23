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

"""checkpoint_analyzer 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_checkpoint_analyzer.py -q
"""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "analysis" / "checkpoint_analyzer.py"
COMPARE = ROOT / "scripts" / "analysis" / "precision_compare.py"


def comparison(
    tmp_path: Path,
    name: str,
    values: list[float],
    *,
    atol: str = "0",
    mode: str = "auto",
) -> Path:
    actual = tmp_path / f"{name}-actual.npy"
    golden = tmp_path / f"{name}-golden.npy"
    report = tmp_path / f"{name}-comparison.json"
    np.save(actual, np.array(values, dtype=np.float32))
    np.save(golden, np.zeros(2, dtype=np.float32))
    result = subprocess.run(
        [
            sys.executable,
            str(COMPARE),
            "--actual",
            str(actual),
            "--golden",
            str(golden),
            "--rtol",
            "0",
            "--atol",
            atol,
            "--mode",
            mode,
            "--output",
            str(report),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode in {0, 1}, result.stderr
    return report


def checkpoint(index: int, report: Path | None = None) -> dict:
    item = {
        "checkpoint_id": f"K{index:03d}",
        "node_id": f"N{index:03d}",
        "order": index * 10,
        "semantic": f"stage-{index}",
        "source_location": f"kernel.cpp:{100 + index}",
        "tensor": {
            "actual_dtype": "float32",
            "oracle_dtype": "float32",
            "logical_shape": [2],
            "physical_shape": [2],
            "layout": "ND",
            "valid_region": {"mode": "all", "element_count": 2},
        },
        "oracle": {
            "contract_id": "ORACLE-001",
            "kind": "stage-reference",
            "description": "matching local oracle",
        },
        "tolerance": {"mode": "auto", "rtol": 0.0, "atol": 0.0},
        "collection": {
            "plane": "NPU",
            "method": "dumptensor",
            "perturbed": False,
            "evidence_refs": [f"A{index:03d}"] if report else [],
        },
    }
    if report:
        item["comparison_report"] = report.name
    return item


def run_plan(tmp_path: Path, checkpoints: list[dict]) -> dict:
    plan = {
        "schema_version": "1.0.0",
        "diagnostic_thread_id": "B001",
        "comparison_id": "CMP-001",
        "checkpoints": checkpoints,
    }
    plan_path = tmp_path / "plan.json"
    output = tmp_path / "analysis.json"
    plan_path.write_text(json.dumps(plan), encoding="utf-8")
    result = subprocess.run(
        [sys.executable, str(TOOL), "--plan", str(plan_path), "--output", str(output)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    report = json.loads(output.read_text(encoding="utf-8"))
    assert report["status"] in {"VALID", "INVALID"}
    assert report["checkpoint_results"]
    return report


def test_bisection_midpoint_and_progressive_trend(tmp_path: Path):
    passed = comparison(tmp_path, "pass", [0.0, 0.0])
    failed_small = comparison(tmp_path, "small", [1.0, 0.0])
    failed_large = comparison(tmp_path, "large", [3.0, 0.0])
    report = run_plan(
        tmp_path,
        [
            checkpoint(1, passed),
            checkpoint(2),
            checkpoint(3, failed_small),
            checkpoint(4, failed_large),
        ],
    )
    assert report["bisection_advice"] == {
        "status": "ELIGIBLE",
        "reason": "an untested semantic midpoint can reduce the current interval",
        "last_normal_checkpoint": "K001",
        "first_abnormal_checkpoint": "K003",
        "recommended_checkpoint": "K002",
    }
    trend = report["progressive_error_trend"]
    assert trend["first_measurable_error_checkpoint"] == "K003"
    assert trend["largest_observed_max_abs_increase"]["to_checkpoint"] == "K004"


def test_non_monotonic_results_disable_bisection(tmp_path: Path):
    failed = comparison(tmp_path, "fail", [1.0, 0.0])
    passed = comparison(tmp_path, "pass", [0.0, 0.0])
    report = run_plan(tmp_path, [checkpoint(1, failed), checkpoint(2, passed)])
    assert report["bisection_advice"]["status"] == "INELIGIBLE"
    assert "PASS after FAIL" in report["bisection_advice"]["reason"]


def test_external_minimal_plan_keeps_verdicts_and_unknown_context(tmp_path):
    for name, payload in {
        "copy": {
            "is_pass": True,
            "matched_ratio": 0.99,
            "max_abs_error": 0.005,
            "error_elements": 1,
        },
        "reduce": {
            "is_pass": False,
            "matched_ratio": 1.0,
            "max_abs_error": 0.02,
            "error_elements": 0,
        },
    }.items():
        (tmp_path / f"{name}.json").write_text(json.dumps(payload))
    plan = tmp_path / "minimal.json"
    plan.write_text(
        json.dumps(
            {
                "checkpoints": [
                    {
                        "checkpoint_id": name,
                        "semantic": name,
                        "comparison_report": f"{name}.json",
                    }
                    for name in ("copy", "reduce")
                ]
            }
        )
    )
    output = tmp_path / "analysis.json"
    result = subprocess.run(
        [sys.executable, str(TOOL), "--plan", str(plan), "--output", str(output)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    report = json.loads(output.read_text())
    assert [p["status"] for p in report["checkpoint_results"]] == ["PASS", "FAIL"]
    assert report["bisection_advice"]["status"] == "INELIGIBLE"
    assert (
        report["progressive_error_trend"]["first_measurable_error_checkpoint"] == "copy"
    )
    assert report["progressive_error_trend"]["first_failed_checkpoint"] == "reduce"


def test_small_nonzero_error_precedes_first_acceptance_failure(tmp_path):
    early = comparison(tmp_path, "early", [1e-5, 0], atol="0.0001")
    late = comparison(tmp_path, "late", [0.01, 0], atol="0.0001")
    points = [checkpoint(1, early), checkpoint(2, late)]
    for point in points:
        point["tolerance"]["atol"] = 0.0001
    report = run_plan(tmp_path, points)
    trend = report["progressive_error_trend"]
    assert trend["first_measurable_error_checkpoint"] == "K001"
    assert trend["first_failed_checkpoint"] == "K002"


def test_bitwise_reports_do_not_require_nonzero_numeric_error(tmp_path):
    clean = comparison(tmp_path, "clean-bits", [0.0, 0.0], mode="bitwise")
    changed = comparison(tmp_path, "changed-bits", [-0.0, 0.0], mode="bitwise")
    points = [checkpoint(1, clean), checkpoint(2, changed)]
    for point in points:
        point["tolerance"]["mode"] = "bitwise"
    report = run_plan(tmp_path, points)
    assert report["bisection_advice"]["status"] == "ADJACENT_BOUNDARY"
    assert (
        report["progressive_error_trend"]["first_measurable_error_checkpoint"] is None
    )
    assert report["progressive_error_trend"]["first_failed_checkpoint"] == "K002"


@pytest.mark.parametrize("change", ["missing-context", "different-rule"])
def test_partial_or_incompatible_context_prevents_bisection(tmp_path, change):
    clean = comparison(tmp_path, "clean", [0, 0])
    failed = comparison(tmp_path, "failed", [1, 0])
    points = [checkpoint(1, clean), checkpoint(2, failed)]
    if change == "missing-context":
        points[0]["collection"].pop("perturbed")
    else:
        points[0]["comparison_basis"] = "another acceptance rule"
    report = run_plan(tmp_path, points)
    assert report["bisection_advice"]["status"] == "INELIGIBLE"
    assert [p["status"] for p in report["checkpoint_results"]] == ["PASS", "FAIL"]


def test_verdict_only_does_not_invent_zero_error(tmp_path):
    report_path = tmp_path / "verdict.json"
    report_path.write_text(json.dumps({"is_pass": False}))
    report = run_plan(
        tmp_path,
        [
            {
                "checkpoint_id": "out",
                "semantic": "final output",
                "comparison_report": report_path.name,
            }
        ],
    )
    assert report["checkpoint_results"][0]["metrics"]["max_abs_error"] is None
    assert (
        report["progressive_error_trend"]["first_measurable_error_checkpoint"] is None
    )
    assert report["progressive_error_trend"]["first_failed_checkpoint"] == "out"


def test_shared_description_does_not_hide_different_comparison_parameters(tmp_path):
    clean = comparison(tmp_path, "clean", [0, 0])
    failed = comparison(tmp_path, "failed", [1, 0], atol="0.1")
    points = [checkpoint(1, clean), checkpoint(2, failed)]
    points[1]["tolerance"]["atol"] = 0.1
    for point in points:
        point["comparison_basis"] = "same named standard"
    report = run_plan(tmp_path, points)
    assert report["bisection_advice"]["status"] == "INELIGIBLE"


def test_unobserved_midpoint_needs_no_claim_about_perturbation(tmp_path):
    passed = comparison(tmp_path, "pass", [0, 0])
    failed = comparison(tmp_path, "fail", [1, 0])
    point = {"checkpoint_id": "K002", "order": 20, "semantic": "next observation"}
    report = run_plan(tmp_path, [checkpoint(1, passed), point, checkpoint(3, failed)])
    assert report["bisection_advice"]["status"] == "ELIGIBLE"
    assert report["bisection_advice"]["recommended_checkpoint"] == "K002"
    assert report["checkpoint_results"][1]["status"] == "UNTESTED"
    assert report["checkpoint_results"][1]["perturbed"] is None


@pytest.mark.parametrize(
    "middle",
    [
        {"ambiguous_reason": "no applicable local reference"},
        {"collection": {"perturbed": True}},
    ],
)
def test_known_unusable_midpoint_still_blocks_bisection(tmp_path, middle):
    passed = comparison(tmp_path, "pass", [0, 0])
    failed = comparison(tmp_path, "fail", [1, 0])
    point = {"checkpoint_id": "K002", "order": 20, "semantic": "middle", **middle}
    report = run_plan(tmp_path, [checkpoint(1, passed), point, checkpoint(3, failed)])
    assert report["bisection_advice"]["status"] == "INELIGIBLE"
    assert report["bisection_advice"]["recommended_checkpoint"] is None
