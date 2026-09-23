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

"""precision_compare 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_precision_compare.py -q
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys

import numpy as np
import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "analysis" / "precision_compare.py"


def run_tool(*arguments: str) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return subprocess.run(
        [sys.executable, str(TOOL), *arguments],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
        env=environment,
        check=False,
    )


def save_pair(
    tmp_path: Path, actual: np.ndarray, golden: np.ndarray
) -> tuple[Path, Path]:
    actual_path = tmp_path / "actual.npy"
    golden_path = tmp_path / "golden.npy"
    np.save(actual_path, actual)
    np.save(golden_path, golden)
    return actual_path, golden_path


def parse_report(result: subprocess.CompletedProcess[str]) -> dict:
    return json.loads(result.stdout)


def test_float_tolerance_passes_and_report_keeps_consumer_contract(tmp_path: Path):
    golden = np.array([[0.0, 1.0], [10.0, -5.0]], dtype=np.float32)
    actual = golden + np.array([[5e-7, 5e-6], [5e-5, -5e-6]], dtype=np.float32)
    actual_path, golden_path = save_pair(tmp_path, actual, golden)

    result = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--rtol",
        "1e-5",
        "--atol",
        "1e-6",
    )
    assert result.returncode == 0, result.stderr
    report = parse_report(result)
    assert report["schema_version"] == "1.0.0"
    assert report["status"] == "PASS"
    assert report["metrics"]["effective_mode"] == "tolerance"
    assert report["metrics"]["error_elements"] == 0
    assert report["selection"]["compared_elements"] == 4


def test_mismatch_returns_one_and_writes_strict_json(tmp_path: Path):
    actual_path, golden_path = save_pair(
        tmp_path,
        np.array([1.0, 4.0], dtype=np.float32),
        np.array([1.0, 2.0], dtype=np.float32),
    )
    output = tmp_path / "reports" / "comparison.json"
    result = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--rtol",
        "0",
        "--atol",
        "0",
        "--output",
        str(output),
    )
    assert result.returncode == 1, result.stderr
    assert result.stdout == ""
    report = json.loads(output.read_text(encoding="utf-8"))
    assert report["schema_version"] == "1.0.0"
    assert report["status"] == "MISMATCH"
    assert report["metrics"]["error_elements"] == 1
    assert report["metrics"]["top_error_positions"] == [[1]]
    assert report["metrics"]["top_mismatches"][0]["absolute_error"] == 2.0


def test_integer_auto_mode_is_exact_even_with_large_tolerance(tmp_path: Path):
    actual_path, golden_path = save_pair(
        tmp_path,
        np.array([100, 101], dtype=np.int32),
        np.array([100, 100], dtype=np.int32),
    )
    result = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--rtol",
        "10",
        "--atol",
        "10",
    )
    assert result.returncode == 1
    report = parse_report(result)
    assert report["metrics"]["effective_mode"] == "exact"
    assert report["metrics"]["finite_mismatch_count"] == 1


def test_special_value_categories_are_compared_without_nonstandard_json(tmp_path: Path):
    matching_actual, matching_golden = save_pair(
        tmp_path,
        np.array([np.nan, np.inf, -np.inf, 1.0], dtype=np.float32),
        np.array([np.nan, np.inf, -np.inf, 1.0], dtype=np.float32),
    )
    matching = run_tool(
        "--actual", str(matching_actual), "--golden", str(matching_golden)
    )
    assert matching.returncode == 0, matching.stderr
    matching_report = parse_report(matching)
    assert matching_report["metrics"]["matched_nan_count"] == 1
    assert matching_report["metrics"]["matched_inf_count"] == 2

    np.save(matching_actual, np.array([0.0, -np.inf, np.inf, 1.0], dtype=np.float32))
    mismatch = run_tool(
        "--actual", str(matching_actual), "--golden", str(matching_golden)
    )
    assert mismatch.returncode == 1
    report = parse_report(mismatch)
    assert report["schema_version"] == "1.0.0"
    assert report["metrics"]["special_value_mismatch_count"] == 3
    assert [item["actual"] for item in report["metrics"]["top_mismatches"]] == [
        0.0,
        "-Inf",
        "+Inf",
    ]


def test_bitwise_mode_reports_float_bit_categories_and_patterns(tmp_path: Path):
    actual = np.array(
        [0x80000000, 0x7FC00001, 0xFFC00003, 0x3F800001, 0x7FC00100],
        dtype=np.uint32,
    ).view(np.float32)
    golden = np.array(
        [0x00000000, 0x7FC00002, 0x7FC00004, 0x3F800000, 0x7FC00100],
        dtype=np.uint32,
    ).view(np.float32)
    actual_path, golden_path = save_pair(tmp_path, actual, golden)

    tolerant = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--rtol",
        "1e-5",
        "--atol",
        "0",
    )
    assert tolerant.returncode == 0, tolerant.stderr

    bitwise = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--mode",
        "bitwise",
    )
    assert bitwise.returncode == 1, bitwise.stderr
    report = parse_report(bitwise)
    metrics = report["metrics"]
    assert report["schema_version"] == "1.0.0"
    assert metrics["effective_mode"] == "bitwise"
    assert metrics["error_elements"] == 4
    assert metrics["bitwise_mismatch_count"] == 4
    assert metrics["signed_zero_mismatch_count"] == 1
    assert metrics["nan_payload_mismatch_count"] == 2
    assert metrics["nan_sign_mismatch_count"] == 1
    assert metrics["numeric_bit_mismatch_count"] == 1
    assert metrics["matched_nan_count"] == 1
    by_position = {tuple(item["position"]): item for item in metrics["top_mismatches"]}
    assert by_position[(0,)]["kind"] == "signed-zero"
    assert by_position[(0,)]["actual_bits"] == "0x80000000"
    assert by_position[(0,)]["golden_bits"] == "0x00000000"
    assert by_position[(1,)]["kind"] == "nan-payload"
    assert by_position[(2,)]["kind"] == "nan-sign-and-payload"
    assert by_position[(3,)]["kind"] == "numeric-bits"


def test_bitwise_mode_requires_matching_dtypes(tmp_path: Path):
    actual_path, golden_path = save_pair(
        tmp_path,
        np.array([1.0], dtype=np.float16),
        np.array([1.0], dtype=np.float32),
    )
    result = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--mode",
        "bitwise",
    )
    assert result.returncode == 2
    error = json.loads(result.stderr)
    assert error["status"] == "INVALID"
    assert "requires the same dtype" in error["error"]


def test_zero_reference_relative_error_is_not_hidden_by_epsilon(tmp_path: Path):
    actual_path, golden_path = save_pair(
        tmp_path,
        np.array([0.0, 1.0, 2.0], dtype=np.float32),
        np.array([0.0, 0.0, 2.0], dtype=np.float32),
    )
    result = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--rtol",
        "0",
        "--atol",
        "0",
    )
    assert result.returncode == 1
    metrics = parse_report(result)["metrics"]
    assert metrics["zero_reference_nonzero_count"] == 1
    assert metrics["relative_error_defined_count"] == 1
    assert metrics["max_rel_error"] == 0.0


def test_valid_count_and_boolean_mask_exclude_unchecked_storage(tmp_path: Path):
    actual = np.array([1.0, 2.0, 999.0, 999.0], dtype=np.float32)
    golden = np.array([1.0, 2.0, 0.0, 0.0], dtype=np.float32)
    actual_path, golden_path = save_pair(tmp_path, actual, golden)

    prefix = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--valid-count",
        "2",
        "--rtol",
        "0",
        "--atol",
        "0",
    )
    assert prefix.returncode == 0
    prefix_report = parse_report(prefix)
    assert prefix_report["selection"]["excluded_elements"] == 2

    mask = np.array([True, False, True, False], dtype=np.bool_)
    mask_path = tmp_path / "mask.npy"
    np.save(mask_path, mask)
    masked = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--mask",
        str(mask_path),
        "--rtol",
        "0",
        "--atol",
        "0",
    )
    assert masked.returncode == 1
    masked_report = parse_report(masked)
    assert masked_report["selection"]["mode"] == "boolean-mask"
    assert masked_report["metrics"]["top_error_positions"] == [[2]]


def test_bin_inputs_can_use_distinct_storage_dtypes(tmp_path: Path):
    actual = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float16)
    golden = actual.astype(np.float32)
    actual_path = tmp_path / "actual.bin"
    golden_path = tmp_path / "golden.bin"
    actual.tofile(actual_path)
    golden.tofile(golden_path)

    result = run_tool(
        "--actual",
        str(actual_path),
        "--actual-dtype",
        "fp16",
        "--actual-shape",
        "2,2",
        "--golden",
        str(golden_path),
        "--golden-dtype",
        "fp32",
        "--golden-shape",
        "2,2",
        "--rtol",
        "0",
        "--atol",
        "0",
    )
    assert result.returncode == 0, result.stderr
    report = parse_report(result)
    assert report["actual"]["dtype"] == "float16"
    assert report["golden"]["dtype"] == "float32"


def test_scalar_npy_is_supported_and_report_is_versioned(tmp_path: Path):
    actual_path, golden_path = save_pair(
        tmp_path,
        np.array(1.0, dtype=np.float32),
        np.array(1.0, dtype=np.float32),
    )
    result = run_tool("--actual", str(actual_path), "--golden", str(golden_path))
    assert result.returncode == 0, result.stderr
    report = parse_report(result)
    assert report["schema_version"] == "1.0.0"
    assert report["actual"]["shape"] == []


def test_invalid_contracts_return_two(tmp_path: Path):
    actual_path, golden_path = save_pair(
        tmp_path,
        np.ones((2, 2), dtype=np.float32),
        np.ones((2, 2), dtype=np.float32),
    )
    negative = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--rtol",
        "-1",
    )
    assert negative.returncode == 2
    assert json.loads(negative.stderr)["status"] == "INVALID"

    wrong_shape = tmp_path / "wrong.npy"
    np.save(wrong_shape, np.ones((4,), dtype=np.float32))
    mismatch = run_tool("--actual", str(actual_path), "--golden", str(wrong_shape))
    assert mismatch.returncode == 2
    assert "shape mismatch" in json.loads(mismatch.stderr)["error"]

    invalid_mask = tmp_path / "invalid-mask.npy"
    np.save(invalid_mask, np.ones((2, 2), dtype=np.int32))
    mask_result = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--mask",
        str(invalid_mask),
    )
    assert mask_result.returncode == 2
    assert "boolean" in json.loads(mask_result.stderr)["error"]

    overwrite = run_tool(
        "--actual",
        str(actual_path),
        "--golden",
        str(golden_path),
        "--output",
        str(actual_path),
    )
    assert overwrite.returncode == 2
    assert "must not overwrite" in json.loads(overwrite.stderr)["error"]
    assert np.load(actual_path).shape == (2, 2)


def test_missing_numpy_is_invalid_and_help_remains_available(tmp_path: Path):
    environment = {
        key: value for key, value in os.environ.items() if key != "PYTHONPATH"
    }
    command = [sys.executable, "-B", "-S", str(TOOL)]
    help_result = subprocess.run(
        [*command, "--help"],
        cwd=tmp_path,
        env=environment,
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert help_result.returncode == 0, help_result.stderr
    output = tmp_path / "comparison.json"
    result = subprocess.run(
        [
            *command,
            "--actual",
            "actual.bin",
            "--golden",
            "golden.bin",
            "--dtype",
            "float32",
            "--output",
            str(output),
        ],
        cwd=tmp_path,
        env=environment,
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode == 2
    assert result.stdout == ""
    assert "Traceback" not in result.stderr
    report = json.loads(result.stderr)
    assert report["status"] == "INVALID"
    assert "NumPy" in report["error"]
    assert not output.exists()


@pytest.mark.parametrize("swap", [False, True])
def test_exact_mixed_integer_float_preserves_integer_identity(
    tmp_path: Path, swap: bool
):
    actual = np.array([2**53 + 1, -(2**53 + 1), 2**53, 0, 3], dtype=np.int64)
    golden = np.array([2**53, -(2**53), 2**53, -0.0, 3.5], dtype=np.float64)
    if swap:
        actual, golden = golden, actual
    paths = save_pair(tmp_path, actual, golden)
    result = run_tool(
        "--actual", str(paths[0]), "--golden", str(paths[1]), "--mode", "exact"
    )
    assert result.returncode == 1, result.stderr
    metrics = parse_report(result)["metrics"]
    assert metrics["error_elements"] == 3
    assert {item["flat_index"] for item in metrics["top_mismatches"]} == {0, 1, 4}


def test_exact_mixed_signed_unsigned_keeps_full_range(tmp_path: Path):
    paths = save_pair(
        tmp_path,
        np.array([-1, 2**63 - 1, 5], dtype=np.int64),
        np.array([2**64 - 1, 2**63, 5], dtype=np.uint64),
    )
    result = run_tool(
        "--actual", str(paths[0]), "--golden", str(paths[1]), "--mode", "exact"
    )
    assert result.returncode == 1, result.stderr
    assert parse_report(result)["metrics"]["error_elements"] == 2


@pytest.mark.parametrize("storage_format", ["npy", "bin"])
def test_bf16_retains_original_bits_and_dtype(tmp_path: Path, storage_format: str):
    actual = np.array([0x8000, 0x7FC1, 0xFFC3, 0x3F81, 0x7FC5], dtype=np.uint16)
    golden = np.array([0x0000, 0x7FC2, 0x7FC4, 0x3F80, 0x7FC5], dtype=np.uint16)
    paths = [tmp_path / f"{side}.{storage_format}" for side in ("actual", "golden")]
    for path, values in zip(paths, (actual, golden)):
        if storage_format == "npy":
            np.save(path, values)
        else:
            values.tofile(path)
    result = run_tool(
        "--actual",
        str(paths[0]),
        "--golden",
        str(paths[1]),
        "--dtype",
        "bf16",
        "--shape",
        "5",
        "--mode",
        "bitwise",
    )
    assert result.returncode == 1, result.stderr
    report = parse_report(result)
    assert report["actual"]["dtype"] == report["golden"]["dtype"] == "bfloat16"
    assert report["actual"]["storage_dtype"] == "uint16"
    metrics = report["metrics"]
    assert metrics["error_elements"] == 4
    assert metrics["signed_zero_mismatch_count"] == 1
    assert metrics["nan_payload_mismatch_count"] == 2
    assert metrics["nan_sign_mismatch_count"] == 1
    by_index = {item["flat_index"]: item for item in metrics["top_mismatches"]}
    assert by_index[0]["actual_bits"] == "0x8000"
    assert by_index[3]["actual_bits"] == "0x3f81"


@pytest.mark.parametrize("shape", [(), (1,)])
def test_bf16_numeric_comparison_and_cross_dtype_bitwise_rejection(
    tmp_path: Path, shape
):
    paths = save_pair(
        tmp_path,
        np.full(shape, 0x3F80, dtype=np.uint16),
        np.full(shape, 1.0, dtype=np.float32),
    )
    args = (
        "--actual",
        str(paths[0]),
        "--golden",
        str(paths[1]),
        "--actual-dtype",
        "bf16",
    )
    numeric = run_tool(*args, "--mode", "exact")
    assert numeric.returncode == 0, numeric.stderr
    report = parse_report(numeric)
    assert report["actual"]["dtype"] == "bfloat16"
    assert report["actual"]["shape"] == list(shape)
    bitwise = run_tool(*args, "--mode", "bitwise")
    assert bitwise.returncode == 2
    assert "requires the same dtype" in json.loads(bitwise.stderr)["error"]


@pytest.mark.parametrize("magnitude", [1e200, 1e-200, np.finfo(np.float64).max])
def test_finite_fp64_equal_arrays_produce_valid_metrics(
    tmp_path: Path, magnitude: float
):
    values = np.array([magnitude, -magnitude], dtype=np.float64)
    paths = save_pair(tmp_path, values, values)
    result = run_tool(
        "--actual", str(paths[0]), "--golden", str(paths[1]), "--mode", "exact"
    )
    assert result.returncode == 0, result.stderr
    metrics = parse_report(result)["metrics"]
    assert metrics["max_abs_error"] == metrics["rmse"] == 0
    assert metrics["cosine_similarity"] == pytest.approx(1)
    assert metrics["nonfinite_metric_fields"] == []


@pytest.mark.parametrize("rtol,expected_code", [(0, 1), (1.9, 1), (2, 0)])
def test_overflowing_fp64_difference_keeps_correct_verdict(
    tmp_path: Path, rtol: float, expected_code: int
):
    paths = save_pair(
        tmp_path,
        np.array([-1e308], dtype=np.float64),
        np.array([1e308], dtype=np.float64),
    )
    result = run_tool(
        "--actual",
        str(paths[0]),
        "--golden",
        str(paths[1]),
        "--rtol",
        str(rtol),
        "--atol",
        "0",
    )
    assert result.returncode == expected_code, result.stderr
    report = parse_report(result)
    assert report["metrics"]["error_elements"] == int(expected_code == 1)
    assert report["metrics"]["max_abs_error"] is None
    assert "max_abs_error" in report["metrics"]["nonfinite_metric_fields"]
    assert report["metrics"]["cosine_similarity"] == pytest.approx(-1)
