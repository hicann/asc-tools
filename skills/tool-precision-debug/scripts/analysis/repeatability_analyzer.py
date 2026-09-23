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

"""用途：重复执行并复用外部验收结果，或按自有比较规则观察漂移；仅按已取得的信息分类。
使用方法（在 Skill 根目录执行）：
    python3 scripts/analysis/repeatability_analyzer.py --help

参数和示例见 scripts/usage/repeatability-analyzer.md。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analysis.comparison_result import (
    execution_issue,
    read_result,
    validate_mismatch_exit_codes,
)


SCHEMA_VERSION = "1.0.0"
RUNNER = Path(__file__).resolve().parents[1] / "debug" / "run_with_evidence.py"
COMPARATOR = Path(__file__).resolve().with_name("precision_compare.py")


class RepeatabilityError(ValueError):
    """The fixed-input repeatability contract is invalid."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def regular_file_snapshot(path_value: str | Path, label: str) -> dict[str, Any]:
    path = Path(path_value).expanduser()
    if path.is_symlink():
        raise RepeatabilityError(f"{label} must not be a symbolic link: {path}")
    try:
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise RepeatabilityError(f"cannot resolve {label}: {exc}") from exc
    if not resolved.is_file():
        raise RepeatabilityError(f"{label} is not a regular file: {resolved}")
    stat = resolved.stat()
    return {
        "path": str(resolved),
        "size_bytes": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
        "sha256": sha256_file(resolved),
    }


def optional_snapshot(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    return regular_file_snapshot(path, "actual output")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", required=True, type=int)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cwd", type=Path)
    parser.add_argument("--timeout", type=float)
    parser.add_argument(
        "--input", action="append", default=[], help="固定输入文件；可重复"
    )
    parser.add_argument(
        "--actual", type=Path, help="每次命令应新建或改写的输出；外部结果模式可省略"
    )
    parser.add_argument("--golden", type=Path)
    parser.add_argument(
        "--evaluation-report",
        type=Path,
        help="被测命令每轮生成的验收 JSON；不再调用自有比较器",
    )
    parser.add_argument(
        "--mismatch-exit-code",
        type=int,
        action="append",
        default=[],
        help="明确表示数值失败的退出码；可重复",
    )
    parser.add_argument("--dtype")
    parser.add_argument("--actual-dtype")
    parser.add_argument("--golden-dtype")
    parser.add_argument("--shape")
    parser.add_argument("--actual-shape")
    parser.add_argument("--golden-shape")
    parser.add_argument(
        "--mode", choices=("auto", "exact", "tolerance", "bitwise"), default="auto"
    )
    parser.add_argument("--rtol", type=float, default=1e-3)
    parser.add_argument("--atol", type=float, default=1e-5)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--valid-count", type=int)
    selection.add_argument("--mask", type=Path)
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--probe-report")
    parser.add_argument(
        "--require-capability",
        action="append",
        default=[],
        choices=("cpu-debug", "npu-check", "msobjdump", "show-kernel-debug-data"),
    )
    parser.add_argument("--expected-cann")
    parser.add_argument("--npu-arch")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    return parser


def validate_args(args: argparse.Namespace) -> None:
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        raise RepeatabilityError("provide the repeated command after --")
    if args.evaluation_report is None and (args.actual is None or args.golden is None):
        raise RepeatabilityError(
            "provide --actual and --golden, or use --evaluation-report"
        )
    validate_mismatch_exit_codes(args.mismatch_exit_code)
    if args.runs < 2 or args.runs > 1000:
        raise RepeatabilityError("--runs must be in [2, 1000]")
    if args.timeout is not None and (
        not math.isfinite(args.timeout) or args.timeout <= 0
    ):
        raise RepeatabilityError("--timeout must be a positive finite value")
    for name in ("rtol", "atol"):
        value = getattr(args, name)
        if not math.isfinite(value) or value < 0:
            raise RepeatabilityError(f"--{name} must be a finite non-negative value")
    cwd = (args.cwd or Path.cwd()).expanduser().resolve()
    if not cwd.is_dir():
        raise RepeatabilityError(f"--cwd is not a directory: {cwd}")
    args.cwd = cwd
    work_dir = args.work_dir.expanduser().resolve()
    output = args.output.expanduser().resolve()
    if work_dir.exists():
        raise RepeatabilityError(f"--work-dir must not already exist: {work_dir}")
    protected = {
        Path(value).expanduser().resolve()
        for value in [
            *args.input,
            args.actual,
            args.golden,
            args.mask,
            args.evaluation_report,
        ]
        if value is not None
    }
    if output in protected or output == work_dir or output.is_relative_to(work_dir):
        raise RepeatabilityError(
            "--output must be outside work-dir and must not overwrite an input"
        )
    immutable_paths = {
        Path(value).expanduser().resolve()
        for value in [*args.input, args.golden, args.mask]
        if value is not None
    }
    generated_paths = [
        p.expanduser().resolve()
        for p in (args.actual, args.evaluation_report)
        if p is not None
    ]
    if len(set(generated_paths)) != len(generated_paths) or any(
        p in immutable_paths for p in generated_paths
    ):
        raise RepeatabilityError(
            "actual/evaluation outputs must be distinct and must not overwrite fixed inputs"
        )
    if args.require_capability and not all(
        (args.probe_report, args.expected_cann, args.npu_arch)
    ):
        raise RepeatabilityError(
            "capability-bound runs require --probe-report, --expected-cann and --npu-arch"
        )


def comparison_command(
    args: argparse.Namespace, actual: Path, output: Path
) -> list[str]:
    command = [
        sys.executable,
        str(COMPARATOR),
        "--actual",
        str(actual),
        "--golden",
        str(args.golden.expanduser().resolve()),
        "--mode",
        args.mode,
        "--rtol",
        str(args.rtol),
        "--atol",
        str(args.atol),
        "--top-k",
        "1000",
        "--output",
        str(output),
    ]
    for option in (
        "dtype",
        "actual_dtype",
        "golden_dtype",
        "shape",
        "actual_shape",
        "golden_shape",
    ):
        value = getattr(args, option)
        if value:
            command.extend(["--" + option.replace("_", "-"), value])
    if args.valid_count is not None:
        command.extend(["--valid-count", str(args.valid_count)])
    if args.mask is not None:
        command.extend(["--mask", str(args.mask.expanduser().resolve())])
    return command


def error_metric_digest(report: dict[str, Any]) -> str:
    metrics = report["metrics"]
    selected = {
        key: metrics.get(key)
        for key in (
            "error_elements",
            "error_ratio",
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
            "top_mismatches",
        )
    }
    original = report.get("raw", report)
    original_metrics = original.get("metrics", {})
    selected["top_mismatches"] = original_metrics.get(
        "top_mismatches", original.get("top_mismatches")
    )
    return hashlib.sha256(
        json.dumps(selected, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def classify(runs: list[dict[str, Any]]) -> tuple[str, dict[str, Any]]:
    invalid = [item for item in runs if item["observation_status"] != "VALID"]
    if invalid:
        return "INCONCLUSIVE", {
            "reason": "one or more repetitions lacked a valid execution/output/comparison observation",
            "invalid_run_ids": [item["run_id"] for item in invalid],
        }
    pass_count = sum(item["comparison_status"] == "PASS" for item in runs)
    fail_count = len(runs) - pass_count
    if pass_count == len(runs):
        return "NO_FAILURE_OBSERVED", {"reason": "all observed repetitions passed"}
    if pass_count and fail_count:
        return "INTERMITTENT_FAILURE", {
            "reason": "the same frozen-input contract produced both PASS and MISMATCH",
        }
    if any(
        not item["mismatch_index_sha256"] or not item["error_metric_sha256"]
        for item in runs
    ):
        return "ALL_RUNS_FAILED", {
            "reason": "all repetitions failed the selected evaluation; complete position/error observations are unavailable",
        }
    position_digests = {item["mismatch_index_sha256"] for item in runs}
    if len(position_digests) > 1:
        return "POSITION_DRIFT", {
            "reason": "mismatch position-set digests differed across failing repetitions",
        }
    amplitude_digests = {item["error_metric_sha256"] for item in runs}
    if len(amplitude_digests) > 1:
        return "AMPLITUDE_DRIFT", {
            "reason": "mismatch positions were stable while error metrics differed",
        }
    return "STABLE_MISMATCH", {
        "reason": "all repetitions mismatched with identical position and error-metric digests",
    }


def execute(args: argparse.Namespace) -> dict[str, Any]:
    validate_args(args)
    inputs = [
        regular_file_snapshot(path, f"fixed input {index}")
        for index, path in enumerate(args.input, 1)
    ]
    golden = regular_file_snapshot(args.golden, "golden") if args.golden else None
    if args.mask is not None:
        inputs.append(regular_file_snapshot(args.mask, "selection mask"))
    work_dir = args.work_dir.expanduser().resolve()
    work_dir.mkdir(parents=True)
    actual_path = args.actual.expanduser().resolve() if args.actual else None
    evaluation_path = (
        args.evaluation_report.expanduser().resolve()
        if args.evaluation_report
        else None
    )
    runs: list[dict[str, Any]] = []
    immutable_files = [*inputs, *([golden] if golden else [])]

    def refreshed(
        path: Path, before: dict[str, Any] | None, label: str
    ) -> dict[str, Any]:
        after = optional_snapshot(path)
        if after is None:
            raise RepeatabilityError(f"{label} was not produced")
        if before == after:
            raise RepeatabilityError(f"{label} was not demonstrably refreshed")
        return after

    for index in range(1, args.runs + 1):
        run_id = f"RUN-{index:04d}"
        run_dir = work_dir / run_id.lower()
        run_dir.mkdir()
        before_actual = optional_snapshot(actual_path) if actual_path else None
        before_evaluation = (
            optional_snapshot(evaluation_path) if evaluation_path else None
        )
        metadata_path = run_dir / "execution.meta.json"
        runner_command = [
            sys.executable,
            str(RUNNER),
            "--log",
            str(run_dir / "execution.log"),
            "--metadata",
            str(metadata_path),
            "--cwd",
            str(args.cwd),
            "--label",
            run_id,
        ]
        if args.timeout is not None:
            runner_command.extend(["--timeout", str(args.timeout)])
        for item in args.env:
            runner_command.extend(["--env", item])
        if args.require_capability:
            runner_command.extend(
                [
                    "--probe-report",
                    str(Path(args.probe_report).expanduser().resolve()),
                    "--expected-cann",
                    args.expected_cann,
                    "--npu-arch",
                    args.npu_arch,
                ]
            )
            for capability in args.require_capability:
                runner_command.extend(["--require-capability", capability])
        runner_command.extend(["--", *args.command])
        completed = subprocess.run(
            runner_command, capture_output=True, text=True, check=False
        )
        record: dict[str, Any] = {
            "run_id": run_id,
            "observation_status": "INVALID",
            "execution_returncode": completed.returncode,
            "execution_metadata": str(metadata_path),
            "actual_snapshot": None,
            "comparison_report": None,
            "comparison_status": None,
            "mismatch_index_sha256": None,
            "error_metric_sha256": None,
            "limitations": [],
        }
        try:
            metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
            if (
                not isinstance(metadata, dict)
                or metadata.get("wrapper_returncode") != completed.returncode
            ):
                raise RepeatabilityError(
                    "execution metadata does not match the runner exit"
                )
            if (
                metadata.get("outcome") != "completed"
                or metadata.get("timed_out")
                or metadata.get("child_returncode", -1) < 0
            ):
                raise RepeatabilityError("execution did not complete normally")
            frozen_actual = None
            if actual_path is not None:
                refreshed(actual_path, before_actual, "actual output")
                frozen_actual = run_dir / ("actual" + actual_path.suffix.lower())
                shutil.copyfile(actual_path, frozen_actual)
                record["actual_snapshot"] = regular_file_snapshot(
                    frozen_actual, "frozen actual output"
                )
            compare_path = run_dir / "comparison.json"
            if evaluation_path is not None:
                refreshed(evaluation_path, before_evaluation, "evaluation report")
                shutil.copyfile(evaluation_path, compare_path)
            else:
                compared = subprocess.run(
                    comparison_command(args, frozen_actual, compare_path),
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if compared.returncode not in {0, 1} or not compare_path.is_file():
                    raise RepeatabilityError("precision comparison was invalid")
            record["comparison_report"] = str(compare_path)
            comparison = read_result(compare_path)
            issue = execution_issue(
                metadata, args.mismatch_exit_code, comparison["status"]
            )
            if issue:
                raise RepeatabilityError(issue)
            metrics = comparison["metrics"]
            record.update(
                {
                    "observation_status": "VALID",
                    "comparison_status": comparison["status"],
                    "mismatch_index_sha256": metrics.get("mismatch_index_sha256"),
                    "error_metric_sha256": (
                        error_metric_digest(comparison)
                        if metrics.get("max_abs_error") is not None
                        else None
                    ),
                }
            )
        except (OSError, ValueError, TypeError) as exc:
            record["limitations"].append(str(exc))
        current_inputs = [
            regular_file_snapshot(item["path"], f"immutable input after {run_id}")
            for item in immutable_files
        ]
        if current_inputs != immutable_files:
            raise RepeatabilityError(f"fixed input or golden changed during {run_id}")
        runs.append(record)

    classification, rationale = classify(runs)
    valid = [item for item in runs if item["observation_status"] == "VALID"]
    pass_count = sum(item["comparison_status"] == "PASS" for item in valid)
    mismatch_count = sum(item["comparison_status"] == "MISMATCH" for item in valid)
    comparison_rule = (
        {"source": "external", "evaluation_report": str(evaluation_path)}
        if evaluation_path
        else {
            "mode": args.mode,
            "rtol": args.rtol,
            "atol": args.atol,
            "valid_count": args.valid_count,
            "mask": str(args.mask.expanduser().resolve()) if args.mask else None,
        }
    )
    return {
        "schema_version": SCHEMA_VERSION,
        "status": "VALID" if classification != "INCONCLUSIVE" else "INCONCLUSIVE",
        "classification": classification,
        "classification_rationale": rationale,
        "contract": {
            "command_argv": args.command,
            "cwd": str(args.cwd),
            "requested_runs": args.runs,
            "fixed_inputs": inputs,
            "golden": golden,
            "actual_path": str(actual_path) if actual_path else None,
            "comparison": comparison_rule,
            "mismatch_exit_codes": args.mismatch_exit_code,
        },
        "statistics": {
            "observed_runs": len(runs),
            "valid_runs": len(valid),
            "pass_count": pass_count,
            "mismatch_count": mismatch_count,
            "failure_rate": mismatch_count / len(valid) if valid else None,
        },
        "runs": runs,
        "limitations": [
            "Classification describes only the observed repetitions; it does not prove future determinism.",
            "The selected evaluator determines PASS/MISMATCH; missing diagnostics do not imply stable outputs.",
            "The tool reports failure distribution and does not infer a root cause.",
        ],
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
    args = build_parser().parse_args()
    try:
        report = execute(args)
        write_json_atomic(args.output.expanduser().resolve(), report)
        print(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False))
        return 0 if report["status"] == "VALID" else 1
    except (
        RepeatabilityError,
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
