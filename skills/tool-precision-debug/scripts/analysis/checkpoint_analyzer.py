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

"""用途：整理已有验收结果；在比较依据充分时建议二分，区分首次非零误差与首次验收失败。
使用方法（在 Skill 根目录执行）：
    python3 scripts/analysis/checkpoint_analyzer.py --help

参数和示例见 scripts/usage/checkpoint-analyzer.md。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import sys
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analysis.comparison_result import read_result


SCHEMA_VERSION = "1.0.0"


class CheckpointError(ValueError):
    """The checkpoint plan or referenced comparison is invalid."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise CheckpointError(f"{label} must be an object")
    return value


def require_text(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise CheckpointError(f"{label} must be non-empty text")
    return value


def require_nonnegative_number(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise CheckpointError(f"{label} must be a number")
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise CheckpointError(f"{label} must be finite and non-negative")
    return number


def validate_plan(plan: Any) -> list[dict[str, Any]]:
    plan = require_object(plan, "plan")
    if plan.get("schema_version", SCHEMA_VERSION) != SCHEMA_VERSION:
        raise CheckpointError(f"plan schema_version must be {SCHEMA_VERSION}")
    checkpoints = plan.get("checkpoints")
    if not isinstance(checkpoints, list) or not checkpoints:
        raise CheckpointError("checkpoints must contain at least one entry")
    identifiers: set[str] = set()
    orders: set[int] = set()
    normalized = []
    for index, value in enumerate(checkpoints):
        label = f"checkpoints[{index}]"
        checkpoint = dict(require_object(value, label))
        identifier = require_text(
            checkpoint.get("checkpoint_id"), f"{label}.checkpoint_id"
        )
        if identifier in identifiers:
            raise CheckpointError(f"{label}.checkpoint_id must be unique")
        identifiers.add(identifier)
        require_text(checkpoint.get("semantic"), f"{label}.semantic")
        order = checkpoint.setdefault("order", index)
        if isinstance(order, bool) or not isinstance(order, int) or order in orders:
            raise CheckpointError(f"{label}.order must be a unique integer")
        orders.add(order)
        for name in ("tensor", "oracle", "tolerance", "collection"):
            if name in checkpoint:
                require_object(checkpoint[name], f"{label}.{name}")
        tensor = checkpoint.get("tensor", {})
        for name in ("logical_shape", "physical_shape"):
            if name in tensor:
                shape = tensor[name]
                if not isinstance(shape, list) or any(
                    isinstance(item, bool) or not isinstance(item, int) or item < 0
                    for item in shape
                ):
                    raise CheckpointError(
                        f"{label}.tensor.{name} must contain non-negative dimensions"
                    )
        if "valid_region" in tensor:
            region = require_object(
                tensor["valid_region"], f"{label}.tensor.valid_region"
            )
            if region.get("mode") not in {"all", "flat-prefix", "boolean-mask"}:
                raise CheckpointError(f"{label}.tensor.valid_region.mode is invalid")
            if "element_count" in region:
                count = region["element_count"]
                if isinstance(count, bool) or not isinstance(count, int) or count < 0:
                    raise CheckpointError(
                        f"{label}.tensor.valid_region.element_count is invalid"
                    )
            if region["mode"] == "boolean-mask":
                require_text(region.get("mask"), f"{label}.tensor.valid_region.mask")
        tolerance = checkpoint.get("tolerance", {})
        if "mode" in tolerance and tolerance["mode"] not in {
            "auto",
            "exact",
            "tolerance",
            "bitwise",
        }:
            raise CheckpointError(f"{label}.tolerance.mode is invalid")
        for name in ("rtol", "atol"):
            if name in tolerance:
                require_nonnegative_number(tolerance[name], f"{label}.tolerance.{name}")
        collection = checkpoint.get("collection", {})
        if "plane" in collection and collection["plane"] not in {
            "REFERENCE",
            "CPU",
            "NPU",
            "CROSS_PLANE",
        }:
            raise CheckpointError(f"{label}.collection.plane is invalid")
        if "perturbed" in collection and not isinstance(collection["perturbed"], bool):
            raise CheckpointError(f"{label}.collection.perturbed must be boolean")
        if "evidence_refs" in collection:
            refs = collection["evidence_refs"]
            if not isinstance(refs, list) or any(
                not isinstance(ref, str) or not ref for ref in refs
            ):
                raise CheckpointError(
                    f"{label}.collection.evidence_refs must contain non-empty strings"
                )
        if "comparison_basis" in checkpoint:
            require_text(checkpoint["comparison_basis"], f"{label}.comparison_basis")
        if checkpoint.get("comparison_report") and checkpoint.get("ambiguous_reason"):
            raise CheckpointError(
                f"{label} cannot have both comparison_report and ambiguous_reason"
            )
        normalized.append(checkpoint)
    return sorted(normalized, key=lambda item: item["order"])


def load_comparison(
    path_value: str, plan_dir: Path, checkpoint: dict[str, Any]
) -> tuple[dict[str, Any], dict[str, Any]]:
    path = Path(path_value)
    path = plan_dir / path if not path.is_absolute() else path
    if path.is_symlink() or not path.is_file():
        raise CheckpointError(f"comparison report is not a regular file: {path}")
    resolved = path.resolve()
    result = read_result(resolved)
    report = result["raw"]
    tensor = checkpoint.get("tensor", {})
    tolerance = checkpoint.get("tolerance", {})
    actual = report.get("actual", {})
    golden = report.get("golden", {})
    selection = report.get("selection", {})
    for expected, observed, label in (
        (tolerance.get("mode"), report.get("requested_mode"), "mode"),
        (tolerance.get("rtol"), report.get("rtol"), "rtol"),
        (tolerance.get("atol"), report.get("atol"), "atol"),
        (
            tensor.get("actual_dtype"),
            actual.get("dtype") if isinstance(actual, dict) else None,
            "actual dtype",
        ),
        (
            tensor.get("oracle_dtype"),
            golden.get("dtype") if isinstance(golden, dict) else None,
            "golden dtype",
        ),
        (
            tensor.get("logical_shape"),
            actual.get("shape") if isinstance(actual, dict) else None,
            "actual shape",
        ),
        (
            tensor.get("logical_shape"),
            golden.get("shape") if isinstance(golden, dict) else None,
            "golden shape",
        ),
        (
            tensor.get("valid_region", {}).get("mode"),
            selection.get("mode") if isinstance(selection, dict) else None,
            "valid region",
        ),
        (
            tensor.get("valid_region", {}).get("element_count"),
            selection.get("compared_elements") if isinstance(selection, dict) else None,
            "element count",
        ),
    ):
        if expected is not None and observed is not None and expected != observed:
            raise CheckpointError(
                f"comparison {label} does not match checkpoint {checkpoint['checkpoint_id']}"
            )
    declared_mask = tensor.get("valid_region", {}).get("mask")
    observed_mask = selection.get("mask") if isinstance(selection, dict) else None
    if declared_mask is not None and observed_mask is not None:

        def resolve_mask(value: str) -> Path:
            item = Path(value)
            return (
                (plan_dir / item).resolve()
                if not item.is_absolute()
                else item.resolve()
            )

        if resolve_mask(declared_mask) != resolve_mask(observed_mask):
            raise CheckpointError("comparison mask does not match checkpoint")
    return result, {
        "path": str(resolved),
        "size_bytes": resolved.stat().st_size,
        "sha256": sha256_file(resolved),
    }


def comparison_basis(checkpoint: dict[str, Any], report: dict[str, Any]) -> Any:
    declared = checkpoint.get("comparison_basis", report.get("comparison_basis"))
    if declared is not None:
        require_text(declared, "comparison_basis")
    observed = None
    if "is_pass" not in report and report.get("requested_mode") in {
        "auto",
        "exact",
        "tolerance",
        "bitwise",
    }:
        observed = {key: report.get(key) for key in ("requested_mode", "rtol", "atol")}
    keys = ("rtol", "atol", "required_matched_ratio", "max_abs_error_limit")
    if (
        all(
            not isinstance(report.get(key), bool)
            and isinstance(report.get(key), (int, float))
            and math.isfinite(report[key])
            and report[key] >= 0
            for key in keys
        )
        and report["required_matched_ratio"] <= 1
    ):
        observed = {key: report[key] for key in keys}
    # A shared description cannot hide parameters known to differ.
    return (
        {"description": declared, "rule": observed}
        if declared is not None
        else observed
    )


def bisection_advice(results: list[dict[str, Any]]) -> dict[str, Any]:
    tested = [
        item
        for item in results
        if item["status"] in {"PASS", "FAIL"} and not item["perturbed"]
    ]
    statuses = [item["status"] for item in tested]
    seen_fail = False
    inversion = False
    for status in statuses:
        if status == "FAIL":
            seen_fail = True
        elif seen_fail:
            inversion = True
    if inversion:
        return {
            "status": "INELIGIBLE",
            "reason": "tested checkpoints contain PASS after FAIL",
            "last_normal_checkpoint": None,
            "first_abnormal_checkpoint": None,
            "recommended_checkpoint": None,
        }
    passes = [item for item in tested if item["status"] == "PASS"]
    failures = [item for item in tested if item["status"] == "FAIL"]
    if not passes or not failures:
        return {
            "status": "INSUFFICIENT",
            "reason": "a credible PASS boundary and a later FAIL boundary are both required",
            "last_normal_checkpoint": passes[-1]["checkpoint_id"] if passes else None,
            "first_abnormal_checkpoint": failures[0]["checkpoint_id"]
            if failures
            else None,
            "recommended_checkpoint": None,
        }
    last_pass = passes[-1]
    first_fail = failures[0]
    if last_pass["order"] >= first_fail["order"]:
        return {
            "status": "INELIGIBLE",
            "reason": "the PASS boundary does not precede the FAIL boundary",
            "last_normal_checkpoint": last_pass["checkpoint_id"],
            "first_abnormal_checkpoint": first_fail["checkpoint_id"],
            "recommended_checkpoint": None,
        }
    interval = [
        item
        for item in results
        if last_pass["order"] < item["order"] < first_fail["order"]
    ]
    obstructed = [
        item["checkpoint_id"]
        for item in interval
        if (
            item["status"] == "AMBIGUOUS"
            or item["perturbed"] is True
            or (item["status"] != "UNTESTED" and item["perturbed"] is not False)
        )
    ]
    if obstructed:
        return {
            "status": "INELIGIBLE",
            "reason": "ambiguous or perturbed checkpoints obstruct the current boundary",
            "obstructed_checkpoints": obstructed,
            "last_normal_checkpoint": last_pass["checkpoint_id"],
            "first_abnormal_checkpoint": first_fail["checkpoint_id"],
            "recommended_checkpoint": None,
        }
    candidates = [item for item in interval if item["status"] == "UNTESTED"]
    recommended = (
        candidates[len(candidates) // 2]["checkpoint_id"] if candidates else None
    )
    return {
        "status": "ELIGIBLE" if recommended else "ADJACENT_BOUNDARY",
        "reason": (
            "an untested semantic midpoint can reduce the current interval"
            if recommended
            else "the latest PASS and earliest FAIL checkpoints are adjacent"
        ),
        "last_normal_checkpoint": last_pass["checkpoint_id"],
        "first_abnormal_checkpoint": first_fail["checkpoint_id"],
        "recommended_checkpoint": recommended,
    }


def progressive_trend(results: list[dict[str, Any]]) -> dict[str, Any]:
    sequence = []
    for item in results:
        metrics = item.get("metrics")
        if not isinstance(metrics, dict):
            continue
        sequence.append(
            {
                "checkpoint_id": item["checkpoint_id"],
                "order": item["order"],
                "status": item["status"],
                "perturbed": item["perturbed"],
                "error_elements": metrics.get("error_elements"),
                "error_ratio": metrics.get("error_ratio"),
                "max_abs_error": metrics.get("max_abs_error"),
                "max_rel_error": metrics.get("max_rel_error"),
            }
        )
    changes = []
    for previous, current in zip(sequence, sequence[1:]):
        before = previous["max_abs_error"]
        after = current["max_abs_error"]
        delta = None if before is None or after is None else after - before
        factor = None if before in {None, 0} or after is None else after / before
        changes.append(
            {
                "from_checkpoint": previous["checkpoint_id"],
                "to_checkpoint": current["checkpoint_id"],
                "max_abs_error_delta": delta,
                "max_abs_error_factor": factor,
                "factor_undefined_from_zero": before == 0 and after not in {None, 0},
            }
        )
    positive = [
        item
        for item in changes
        if item["max_abs_error_delta"] is not None and item["max_abs_error_delta"] > 0
    ]
    largest = (
        max(positive, key=lambda item: item["max_abs_error_delta"])
        if positive
        else None
    )
    first_measurable = next(
        (
            item["checkpoint_id"]
            for item in sequence
            if item["max_abs_error"] is not None and item["max_abs_error"] > 0
        ),
        None,
    )
    return {
        "sequence": sequence,
        "adjacent_changes": changes,
        "first_measurable_error_checkpoint": first_measurable,
        "first_failed_checkpoint": next(
            (item["checkpoint_id"] for item in results if item["status"] == "FAIL"),
            None,
        ),
        "largest_observed_max_abs_increase": largest,
        "interpretation": (
            "These are observed metric changes only; significance and root cause require an external contract and causal experiment."
        ),
    }


def execute(plan_path: Path) -> dict[str, Any]:
    if plan_path.is_symlink():
        raise CheckpointError(f"symbolic-link plan is not allowed: {plan_path}")
    resolved = plan_path.expanduser().resolve(strict=True)
    try:
        plan = json.loads(resolved.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise CheckpointError(f"cannot load checkpoint plan: {exc}") from exc
    checkpoints = validate_plan(plan)
    results = []
    for checkpoint in checkpoints:
        comparison_record = None
        metrics = None
        basis = None
        if checkpoint.get("comparison_report"):
            comparison, comparison_record = load_comparison(
                checkpoint["comparison_report"], resolved.parent, checkpoint
            )
            status = "PASS" if comparison["status"] == "PASS" else "FAIL"
            metrics = comparison["metrics"]
            basis = comparison_basis(checkpoint, comparison["raw"])
        elif checkpoint.get("ambiguous_reason"):
            require_text(
                checkpoint["ambiguous_reason"],
                f"{checkpoint['checkpoint_id']}.ambiguous_reason",
            )
            status = "AMBIGUOUS"
        else:
            status = "UNTESTED"
        collection = checkpoint.get("collection", {})
        results.append(
            {
                "checkpoint_id": checkpoint["checkpoint_id"],
                "node_id": checkpoint.get("node_id"),
                "order": checkpoint["order"],
                "semantic": checkpoint["semantic"],
                "source_location": checkpoint.get("source_location"),
                "status": status,
                "plane": collection.get("plane"),
                "oracle_contract_id": checkpoint.get("oracle", {}).get("contract_id"),
                "comparison_basis": basis,
                "perturbed": collection.get("perturbed"),
                "evidence_refs": collection.get("evidence_refs", []),
                "comparison_report": comparison_record,
                "metrics": metrics,
                "ambiguous_reason": checkpoint.get("ambiguous_reason"),
            }
        )
    tested = [item for item in results if item["status"] in {"PASS", "FAIL"}]
    missing_context = any(
        not item["plane"]
        or not item["oracle_contract_id"]
        or item["comparison_basis"] is None
        or item["perturbed"] is None
        for item in tested
    )
    comparable_keys = {
        (
            item["plane"],
            item["oracle_contract_id"],
            json.dumps(item["comparison_basis"], sort_keys=True),
        )
        for item in tested
    }
    if missing_context or len(comparable_keys) > 1:
        advice = {
            "status": "INELIGIBLE",
            "reason": (
                "observation plane, reference/comparison basis or perturbation information is missing"
                if missing_context
                else "tested checkpoints do not share one observation plane and reference/comparison basis"
            ),
            "last_normal_checkpoint": None,
            "first_abnormal_checkpoint": None,
            "recommended_checkpoint": None,
        }
    else:
        advice = bisection_advice(results)
    return {
        "schema_version": SCHEMA_VERSION,
        "status": "VALID",
        "plan": {
            "path": str(resolved),
            "size_bytes": resolved.stat().st_size,
            "sha256": sha256_file(resolved),
            "diagnostic_thread_id": plan.get("diagnostic_thread_id"),
            "comparison_id": plan.get("comparison_id"),
        },
        "checkpoint_results": results,
        "bisection_advice": advice,
        "progressive_error_trend": progressive_trend(results),
        "policy": {
            "performs_numeric_comparison": False,
            "consumes_precision_compare_reports": True,
            "consumes_external_evaluation_results": True,
            "infers_root_cause": False,
            "earliest_failure_is_root_cause": False,
        },
    }


def write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        temporary.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        output = args.output.expanduser().resolve()
        plan = args.plan.expanduser().resolve()
        if output == plan:
            raise CheckpointError("--output must not overwrite the checkpoint plan")
        report = execute(args.plan)
        write_json_atomic(output, report)
        print(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False))
        return 0
    except (
        CheckpointError,
        OSError,
        ValueError,
        TypeError,
        json.JSONDecodeError,
    ) as exc:
        print(
            json.dumps(
                {
                    "schema_version": SCHEMA_VERSION,
                    "status": "INVALID",
                    "error": str(exc),
                },
                ensure_ascii=False,
                indent=2,
            ),
            file=sys.stderr,
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
