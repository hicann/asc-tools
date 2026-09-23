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

"""用途：读取已有精度比较结果，保留原验收判据。
使用方法：由检查点、重复性和 dump 分析脚本导入；无独立命令行。
"""

from __future__ import annotations

from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
from typing import Any


class ComparisonResultError(ValueError):
    """An existing evaluation result cannot support a comparison observation."""


def normalize_result(payload: Any) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise ComparisonResultError("comparison result must be a JSON object")
    verdicts = []
    if "is_pass" in payload:
        if not isinstance(payload["is_pass"], bool):
            raise ComparisonResultError("is_pass must be a boolean")
        verdicts.append(payload["is_pass"])
    if "status" in payload:
        status = payload["status"]
        if status not in ("PASS", "MISMATCH", "FAIL"):
            raise ComparisonResultError(
                "comparison status must be PASS, MISMATCH or FAIL"
            )
        verdicts.append(status == "PASS")
    if not verdicts:
        raise ComparisonResultError(
            "comparison result needs is_pass or a completed comparison status"
        )
    if len(set(verdicts)) != 1:
        raise ComparisonResultError("is_pass and status contain conflicting verdicts")

    nested = payload.get("metrics", {})
    if not isinstance(nested, dict):
        raise ComparisonResultError("comparison metrics must be an object")
    # Keep the original file untouched; extract only observed, usable diagnostics.
    metrics = {}
    for key in (
        "error_elements",
        "error_ratio",
        "matched_ratio",
        "max_abs_error",
        "mean_abs_error",
        "rmse",
        "max_rel_error",
        "mean_rel_error",
        "special_value_mismatch_count",
        "bitwise_mismatch_count",
        "signed_zero_mismatch_count",
        "nan_payload_mismatch_count",
        "nan_sign_mismatch_count",
        "numeric_bit_mismatch_count",
    ):
        value = nested.get(key, payload.get(key))
        metrics[key] = (
            value
            if not isinstance(value, bool)
            and isinstance(value, (int, float))
            and math.isfinite(value)
            and value >= 0
            else None
        )
    digest = nested.get("mismatch_index_sha256", payload.get("mismatch_index_sha256"))
    metrics["mismatch_index_sha256"] = (
        digest if isinstance(digest, str) and digest else None
    )
    return {
        "status": "PASS" if verdicts[0] else "MISMATCH",
        "metrics": metrics,
        "raw": payload,
    }


def read_result(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, ValueError) as exc:
        raise ComparisonResultError(
            f"cannot read comparison result {path}: {exc}"
        ) from exc
    return normalize_result(payload)


def validate_mismatch_exit_codes(codes: list[int]) -> None:
    if any(
        isinstance(code, bool) or not isinstance(code, int) or not 1 <= code <= 255
        for code in codes
    ):
        raise ComparisonResultError("mismatch exit codes must be integers in [1, 255]")


def execution_issue(
    metadata: Any,
    mismatch_exit_codes: list[int],
    comparison_status: str | None,
) -> str | None:
    """A declared numerical failure must still be a normal, completed execution."""
    if (
        not isinstance(metadata, dict)
        or metadata.get("outcome") != "completed"
        or metadata.get("timed_out")
    ):
        return "execution did not complete normally"
    code = metadata.get("child_returncode")
    # Compatibility with earlier metadata that only recorded the wrapper code.
    if code is None:
        code = metadata.get("wrapper_returncode")
    if isinstance(code, bool) or not isinstance(code, int) or code < 0:
        return "execution was terminated by a signal or lacks an exit status"
    if metadata.get("wrapper_returncode") != code:
        return "execution exit statuses disagree"
    if code == 0:
        return None
    if code not in mismatch_exit_codes:
        return f"exit code {code} was not declared as a numerical mismatch"
    if comparison_status != "MISMATCH":
        return "a declared mismatch exit requires a valid failing evaluation"
    return None


def evaluation_for_execution(
    path: Path | None, metadata: dict[str, Any]
) -> dict[str, Any] | None:
    """Read an already generated report and ensure its timestamp belongs to this run."""
    if path is None:
        return None
    path = path.expanduser().resolve(strict=True)
    if not path.is_file():
        raise ComparisonResultError("evaluation report must be a regular file")
    try:
        start = datetime.fromisoformat(metadata["started_at"].replace("Z", "+00:00"))
        finish = datetime.fromisoformat(metadata["finished_at"].replace("Z", "+00:00"))
        if start.tzinfo is None or finish.tzinfo is None or finish < start:
            raise ValueError("invalid execution time range")
        start_ns = metadata.get("started_at_ns")
        finish_ns = metadata.get("finished_at_ns")
        modified_ns = path.stat().st_mtime_ns
        if isinstance(start_ns, int) and isinstance(finish_ns, int):
            fresh = start_ns <= modified_ns <= finish_ns
        else:
            # Older second-resolution records cannot bind files written in the
            # same boundary second; retain that uncertainty instead of guessing.
            fresh = start.timestamp() + 1 <= modified_ns / 1e9 <= finish.timestamp()
        if not fresh:
            raise ValueError(
                "evaluation report was not generated during this execution"
            )
    except (KeyError, TypeError, ValueError) as exc:
        raise ComparisonResultError(
            f"cannot bind evaluation to execution: {exc}"
        ) from exc
    content = path.read_bytes()
    result = normalize_result(json.loads(content))
    return {
        "path": str(path),
        "sha256": hashlib.sha256(content).hexdigest(),
        "status": result["status"],
    }
