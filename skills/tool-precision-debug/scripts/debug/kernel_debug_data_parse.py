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

"""用途：直接调用指定解析器并隔离原始 Kernel dump，保留产物和失败；probe 绑定可选。
使用方法（在 Skill 根目录执行）：
    python3 scripts/debug/kernel_debug_data_parse.py --help

参数和示例见 scripts/usage/kernel-debug-data-parse.md。
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import signal
import stat
import subprocess
import tempfile
import threading
import time
from typing import Any

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


SCHEMA_VERSION = "1.0.0"
DEFAULT_TIMEOUT_SECONDS = 300
DEFAULT_MAX_INPUT_FILES = 256
DEFAULT_MAX_INPUT_BYTES = 1024 * 1024 * 1024
DEFAULT_MAX_FILE_BYTES = 256 * 1024 * 1024
DEFAULT_MAX_OUTPUT_FILES = 4096
DEFAULT_MAX_OUTPUT_BYTES = 1024 * 1024 * 1024
DEFAULT_MAX_STREAM_BYTES = 4 * 1024 * 1024
MAX_PROBE_REPORT_BYTES = 4 * 1024 * 1024
MAX_TOOL_BYTES = 64 * 1024 * 1024
MAX_TIMEOUT_SECONDS = 1800
MAX_INPUT_FILES = 4096
MAX_INPUT_BYTES = 8 * 1024 * 1024 * 1024
MAX_FILE_BYTES = 4 * 1024 * 1024 * 1024
MAX_OUTPUT_FILES = 65536
MAX_OUTPUT_BYTES = 8 * 1024 * 1024 * 1024
MAX_STREAM_BYTES = 64 * 1024 * 1024
BLOCK_BOUNDARY = re.compile(r"^=+\s*block\.([^ ]+)\s+(begin|end)\s*=+$")
ERROR_MARKERS = (
    "parse dump workspace bin occur exception",
    "traceback (most recent call last)",
    "parameters invalid, please check tool introduction",
    "file does not exist or permission denied",
    "does not contain any .bin file",
    "unknown block magic",
)


class ParserContractError(ValueError):
    """A user-correctable parser contract error."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument(
        "--probe-report",
        type=Path,
        help="Optional asc_tools_probe.py report; when supplied, verify version and executable binding",
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--artifact-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cann", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--max-input-files", type=int, default=DEFAULT_MAX_INPUT_FILES)
    parser.add_argument("--max-input-bytes", type=int, default=DEFAULT_MAX_INPUT_BYTES)
    parser.add_argument("--max-file-bytes", type=int, default=DEFAULT_MAX_FILE_BYTES)
    parser.add_argument(
        "--max-output-files", type=int, default=DEFAULT_MAX_OUTPUT_FILES
    )
    parser.add_argument(
        "--max-output-bytes", type=int, default=DEFAULT_MAX_OUTPUT_BYTES
    )
    parser.add_argument(
        "--max-stream-bytes", type=int, default=DEFAULT_MAX_STREAM_BYTES
    )
    return parser.parse_args()


def validate_limits(args: argparse.Namespace) -> None:
    limits = (
        ("--timeout-seconds", args.timeout_seconds, 1, MAX_TIMEOUT_SECONDS),
        ("--max-input-files", args.max_input_files, 1, MAX_INPUT_FILES),
        ("--max-input-bytes", args.max_input_bytes, 1, MAX_INPUT_BYTES),
        ("--max-file-bytes", args.max_file_bytes, 1, MAX_FILE_BYTES),
        ("--max-output-files", args.max_output_files, 1, MAX_OUTPUT_FILES),
        ("--max-output-bytes", args.max_output_bytes, 1, MAX_OUTPUT_BYTES),
        ("--max-stream-bytes", args.max_stream_bytes, 1, MAX_STREAM_BYTES),
    )
    for name, value, minimum, maximum in limits:
        if value < minimum or value > maximum:
            raise ParserContractError(f"{name} must be between {minimum} and {maximum}")


def hash_regular_file(path: Path, limit: int, label: str) -> dict[str, Any]:
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as exc:
        raise ParserContractError(
            f"cannot open {label} as a regular file: {exc}"
        ) from exc
    try:
        with os.fdopen(descriptor, "rb") as stream:
            before = os.fstat(stream.fileno())
            if not stat.S_ISREG(before.st_mode):
                raise ParserContractError(f"{label} is not a regular file: {path}")
            if before.st_size > limit:
                raise ParserContractError(f"{label} exceeds byte limit {limit}: {path}")
            digest = hashlib.sha256()
            size = 0
            while True:
                chunk = stream.read(1024 * 1024)
                if not chunk:
                    break
                size += len(chunk)
                if size > limit:
                    raise ParserContractError(
                        f"{label} grew beyond byte limit {limit}: {path}"
                    )
                digest.update(chunk)
            after = os.fstat(stream.fileno())
    except OSError as exc:
        raise ParserContractError(f"cannot read {label}: {exc}") from exc
    identity = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
    if identity != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
        raise ParserContractError(f"{label} changed while being hashed: {path}")
    return {
        "bytes": size,
        "sha256": digest.hexdigest(),
        "identity": list(identity),
    }


def collect_input(
    path: Path, max_files: int, max_total: int, max_file: int
) -> tuple[Path, dict[str, Any]]:
    expanded = path.expanduser()
    if expanded.is_symlink():
        raise ParserContractError(f"symbolic-link input is not allowed: {expanded}")
    try:
        resolved = expanded.resolve(strict=True)
    except OSError as exc:
        raise ParserContractError(f"cannot resolve input: {exc}") from exc
    files: list[tuple[Path, str]] = []
    if resolved.is_file():
        if resolved.suffix.lower() != ".bin":
            raise ParserContractError("single-file input must have a .bin suffix")
        files.append((resolved, resolved.name))
        kind = "file"
    elif resolved.is_dir():
        kind = "directory"
        for root, directories, names in os.walk(resolved, followlinks=False):
            root_path = Path(root)
            for name in list(directories):
                candidate = root_path / name
                if candidate.is_symlink():
                    raise ParserContractError(
                        f"symbolic link inside input is not allowed: {candidate}"
                    )
            for name in names:
                candidate = root_path / name
                if candidate.is_symlink():
                    raise ParserContractError(
                        f"symbolic link inside input is not allowed: {candidate}"
                    )
                if candidate.suffix.lower() == ".bin":
                    files.append(
                        (candidate, candidate.relative_to(resolved).as_posix())
                    )
    else:
        raise ParserContractError(
            f"input is neither a file nor a directory: {resolved}"
        )
    files.sort(key=lambda item: item[1])
    if not files:
        raise ParserContractError("input does not contain any .bin file")
    if len(files) > max_files:
        raise ParserContractError(f"input exceeds file limit {max_files}")
    records = []
    total = 0
    for source, relative in files:
        snapshot = hash_regular_file(source, max_file, f"input file {relative}")
        total += snapshot["bytes"]
        if total > max_total:
            raise ParserContractError(f"input exceeds total byte limit {max_total}")
        snapshot.pop("identity")
        records.append({"relative_path": relative, **snapshot})
    return resolved, {
        "path": str(resolved),
        "kind": kind,
        "file_count": len(records),
        "total_bytes": total,
        "files": records,
    }


def resolve_tool(path: Path) -> tuple[Path, dict[str, Any]]:
    try:
        resolved = path.expanduser().resolve(strict=True)
    except OSError as exc:
        raise ParserContractError(
            f"cannot resolve show_kernel_debug_data: {exc}"
        ) from exc
    if not os.access(resolved, os.X_OK):
        raise ParserContractError(
            f"show_kernel_debug_data is not executable: {resolved}"
        )
    snapshot = hash_regular_file(resolved, MAX_TOOL_BYTES, "show_kernel_debug_data")
    snapshot.pop("identity")
    return resolved, {"path": str(resolved), **snapshot}


def load_probe_binding(
    path: Path, cann: str, tool: Path
) -> tuple[Path, dict[str, Any]]:
    expanded = path.expanduser()
    if expanded.is_symlink():
        raise ParserContractError(
            f"symbolic-link probe report is not allowed: {expanded}"
        )
    try:
        resolved = expanded.resolve(strict=True)
    except OSError as exc:
        raise ParserContractError(f"cannot resolve probe report: {exc}") from exc
    snapshot = hash_regular_file(resolved, MAX_PROBE_REPORT_BYTES, "probe report")
    snapshot.pop("identity")
    try:
        report = json.loads(resolved.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ParserContractError(f"cannot load probe report JSON: {exc}") from exc
    if not isinstance(report, dict) or report.get("schema_version") != "1.1.0":
        raise ParserContractError("probe report must use asc-tools probe schema 1.1.0")
    if report.get("mode") != "probe" or report.get("expected_cann") != cann:
        raise ParserContractError(
            "probe report is not bound to the declared CANN version"
        )
    requirements = report.get("requirements")
    requested = (
        requirements.get("requested") if isinstance(requirements, dict) else None
    )
    if (
        not isinstance(requirements, dict)
        or not isinstance(requested, list)
        or not all(isinstance(item, str) for item in requested)
        or "show-kernel-debug-data" not in requested
        or requirements.get("satisfied") is not True
    ):
        raise ParserContractError(
            "probe report must satisfy an explicit show-kernel-debug-data requirement"
        )
    bound = False
    for installation in report.get("installations", []):
        if not isinstance(installation, dict):
            continue
        version = installation.get("version")
        if not isinstance(version, dict) or version.get("value") != cann:
            continue
        for capability in installation.get("capabilities", []):
            if not isinstance(capability, dict):
                continue
            if (
                capability.get("id") != "show-kernel-debug-data"
                or capability.get("status") != "available"
            ):
                continue
            entrypoint = capability.get("entrypoint")
            try:
                bound = (
                    isinstance(entrypoint, str)
                    and Path(entrypoint).expanduser().resolve(strict=True) == tool
                )
            except OSError:
                bound = False
            if bound:
                break
    if not bound:
        raise ParserContractError(
            "probe report does not bind the requested show_kernel_debug_data executable"
        )
    return resolved, {"path": str(resolved), **snapshot}


def copy_input(source: Path, record: dict[str, Any], destination: Path) -> Path:
    input_root = destination / "input"
    if record["kind"] == "file":
        input_root.mkdir()
        target = input_root / record["files"][0]["relative_path"]
        shutil.copyfile(source, target)
        return target
    input_root.mkdir()
    for item in record["files"]:
        target = input_root / item["relative_path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / item["relative_path"], target)
    return input_root


def stop_process_group(process: subprocess.Popen[bytes]) -> dict[str, bool]:
    result = {"sigterm_sent": False, "sigkill_sent": False, "cleanup_complete": False}
    if process.poll() is not None:
        result["cleanup_complete"] = True
        return result
    try:
        os.killpg(process.pid, signal.SIGTERM)
        result["sigterm_sent"] = True
    except ProcessLookupError:
        pass
    except PermissionError:
        # macOS can return EPERM if the child exited after the initial poll.
        if process.poll() is None:
            raise
    try:
        process.wait(timeout=1)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
            result["sigkill_sent"] = True
        except ProcessLookupError:
            pass
        except PermissionError:
            if process.poll() is None:
                raise
        process.wait()
    result["cleanup_complete"] = process.poll() is not None
    return result


def set_output_file_limit(max_bytes: int) -> None:
    resource.setrlimit(resource.RLIMIT_FSIZE, (max_bytes, max_bytes))


def capture_pipe(
    stream: Any, max_bytes: int, result: dict[str, Any], exceeded: threading.Event
) -> None:
    digest = hashlib.sha256()
    chunks: list[bytes] = []
    kept = 0
    size = 0
    while True:
        chunk = stream.read(64 * 1024)
        if not chunk:
            break
        size += len(chunk)
        digest.update(chunk)
        if kept < max_bytes:
            selected = chunk[: max_bytes - kept]
            chunks.append(selected)
            kept += len(selected)
        if size > max_bytes:
            exceeded.set()
    stream.close()
    data = b"".join(chunks)
    try:
        decoded = data.decode("utf-8")
        decode_errors = False
    except UnicodeDecodeError:
        decoded = data.decode("utf-8", errors="replace")
        decode_errors = True
    result.update(
        {
            "bytes": size,
            "sha256": digest.hexdigest(),
            "decode_errors": decode_errors,
            "truncated": size > max_bytes,
            "text": decoded,
        }
    )


def run_parser(
    tool: Path, staged_input: Path, workspace: Path, args: argparse.Namespace
) -> dict[str, Any]:
    parsed = workspace / "parsed"
    parsed.mkdir()
    argv = [str(tool), str(staged_input), str(parsed)]
    started = time.monotonic()
    timed_out = False
    cleanup = None
    stream_limit_exceeded = threading.Event()
    stdout: dict[str, Any] = {}
    stderr: dict[str, Any] = {}
    try:
        process = subprocess.Popen(
            argv,
            cwd=workspace,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
            preexec_fn=lambda: set_output_file_limit(args.max_file_bytes),
        )
    except OSError as exc:
        raise ParserContractError(
            f"cannot launch show_kernel_debug_data: {exc}"
        ) from exc
    assert process.stdout is not None and process.stderr is not None
    threads = [
        threading.Thread(
            target=capture_pipe,
            args=(process.stdout, args.max_stream_bytes, stdout, stream_limit_exceeded),
            daemon=True,
        ),
        threading.Thread(
            target=capture_pipe,
            args=(process.stderr, args.max_stream_bytes, stderr, stream_limit_exceeded),
            daemon=True,
        ),
    ]
    for thread in threads:
        thread.start()
    deadline = time.monotonic() + args.timeout_seconds
    while process.poll() is None:
        if stream_limit_exceeded.is_set():
            cleanup = stop_process_group(process)
            break
        if time.monotonic() >= deadline:
            timed_out = True
            cleanup = stop_process_group(process)
            break
        time.sleep(0.02)
    returncode = process.wait()
    for thread in threads:
        thread.join()
    output_limit_exceeded = stream_limit_exceeded.is_set() or returncode in {
        -signal.SIGXFSZ,
        128 + signal.SIGXFSZ,
    }
    return {
        "argv": [str(tool), "<private-input-copy>", "<private-output-dir>"],
        "returncode": returncode,
        "timed_out": timed_out,
        "output_limit_exceeded": output_limit_exceeded,
        "duration_seconds": round(time.monotonic() - started, 6),
        "timeout_cleanup": cleanup,
        "stdout": stdout,
        "stderr": stderr,
    }


def classify_artifact(relative: str) -> str:
    name = Path(relative).name
    if name == "parser.log" or name == "dump_parser.log":
        return "parser-log"
    if name == "index_dtype.json":
        return "index-dtype-map"
    if name.startswith("time_stamp_core_") and name.endswith(".csv"):
        return "timestamp-csv"
    if name.endswith(".txt"):
        return "tensor-text"
    if name.endswith(".bin"):
        return "tensor-binary"
    return "other"


def inventory_outputs(
    root: Path, max_files: int, max_total: int, max_file: int
) -> list[dict[str, Any]]:
    paths: list[Path] = []
    for current, directories, names in os.walk(root, followlinks=False):
        current_path = Path(current)
        for name in directories:
            candidate = current_path / name
            if candidate.is_symlink():
                raise ParserContractError(
                    f"parser output contains a symbolic link: {candidate}"
                )
        for name in names:
            candidate = current_path / name
            if candidate.is_symlink():
                raise ParserContractError(
                    f"parser output contains a symbolic link: {candidate}"
                )
            paths.append(candidate)
    paths.sort(key=lambda path: path.relative_to(root).as_posix())
    if len(paths) > max_files:
        raise ParserContractError(f"parser output exceeds file limit {max_files}")
    records = []
    total = 0
    for path in paths:
        relative = path.relative_to(root).as_posix()
        snapshot = hash_regular_file(path, max_file, f"parser output {relative}")
        total += snapshot["bytes"]
        if total > max_total:
            raise ParserContractError(
                f"parser output exceeds total byte limit {max_total}"
            )
        snapshot.pop("identity")
        records.append(
            {
                "artifact_id": f"KDBG-ART-{len(records) + 1:06d}",
                "relative_path": relative,
                "kind": classify_artifact(relative),
                **snapshot,
            }
        )
    return records


def parse_console_boundaries(stdout: str) -> list[dict[str, Any]]:
    observations = []
    for line_number, line in enumerate(stdout.splitlines(), 1):
        match = BLOCK_BOUNDARY.match(line.strip())
        if match:
            observations.append(
                {
                    "observation_id": f"KDBG-OBS-{len(observations) + 1:06d}",
                    "kind": "console-block-boundary",
                    "stream": "stdout",
                    "line_number": line_number,
                    "raw_line": line,
                    "block_id": match.group(1),
                    "boundary": match.group(2),
                }
            )
    return observations


def parse_diagnostics(stdout: str, stderr: str) -> list[dict[str, Any]]:
    diagnostics = []
    for stream, text in (("stdout", stdout), ("stderr", stderr)):
        for line_number, line in enumerate(text.splitlines(), 1):
            lowered = line.lower()
            marker = next((item for item in ERROR_MARKERS if item in lowered), None)
            if marker is not None:
                diagnostics.append(
                    {
                        "stream": stream,
                        "line_number": line_number,
                        "level": "error",
                        "marker": marker,
                        "raw_line": line,
                    }
                )
    return diagnostics


def ensure_destinations(
    args: argparse.Namespace, protected: set[Path]
) -> tuple[Path, Path]:
    report = args.output.expanduser().resolve()
    artifacts = args.artifact_dir.expanduser().resolve()
    if report in protected or artifacts in protected:
        raise ParserContractError(
            "output paths cannot overwrite an input, tool or probe report"
        )
    if report.exists():
        raise ParserContractError(f"output report already exists: {report}")
    if artifacts.exists():
        raise ParserContractError(f"artifact directory already exists: {artifacts}")
    if (
        report == artifacts
        or report.is_relative_to(artifacts)
        or artifacts.is_relative_to(report)
    ):
        raise ParserContractError(
            "output report and artifact directory must not overlap"
        )
    report.parent.mkdir(parents=True, exist_ok=True)
    artifacts.parent.mkdir(parents=True, exist_ok=True)
    return report, artifacts


def reject_input_overlap(input_path: Path, input_kind: str, *outputs: Path) -> None:
    if input_kind != "directory":
        return
    if any(
        output == input_path or output.is_relative_to(input_path) for output in outputs
    ):
        raise ParserContractError("output paths must not be inside the input directory")


def publish_artifacts(source: Path, target: Path) -> None:
    temporary = Path(tempfile.mkdtemp(prefix=target.name + ".", dir=target.parent))
    try:
        shutil.copytree(source, temporary, dirs_exist_ok=True)
        os.replace(temporary, target)
    except OSError as exc:
        raise ParserContractError(f"cannot publish parser artifacts: {exc}") from exc
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def build_report(args: argparse.Namespace) -> tuple[dict[str, Any], int]:
    validate_limits(args)
    input_path, input_record = collect_input(
        args.input, args.max_input_files, args.max_input_bytes, args.max_file_bytes
    )
    tool_path, tool_record = resolve_tool(args.tool)
    probe_path, probe_record = (
        load_probe_binding(args.probe_report, args.cann, tool_path)
        if args.probe_report is not None
        else (None, None)
    )
    protected = {input_path, tool_path}
    if probe_path is not None:
        protected.add(probe_path)
    report_path, artifact_path = ensure_destinations(args, protected)
    reject_input_overlap(input_path, input_record["kind"], report_path, artifact_path)
    input_before = collect_input(
        input_path, args.max_input_files, args.max_input_bytes, args.max_file_bytes
    )[1]
    tool_before = hash_regular_file(tool_path, MAX_TOOL_BYTES, "show_kernel_debug_data")
    with tempfile.TemporaryDirectory(prefix="asc-kernel-debug-") as temporary_name:
        workspace = Path(temporary_name)
        staged_input = copy_input(input_path, input_record, workspace)
        execution = run_parser(tool_path, staged_input, workspace, args)
        artifacts = inventory_outputs(
            workspace / "parsed",
            args.max_output_files,
            args.max_output_bytes,
            args.max_file_bytes,
        )
        observations = parse_console_boundaries(execution["stdout"]["text"])
        diagnostics = parse_diagnostics(
            execution["stdout"]["text"], execution["stderr"]["text"]
        )
        explicit_errors = sum(item["level"] == "error" for item in diagnostics)
        tool_error = (
            execution["timed_out"]
            or execution["output_limit_exceeded"]
            or execution["returncode"] != 0
            or explicit_errors > 0
        )
        status = (
            "TOOL_ERROR"
            if tool_error
            else ("OBSERVATIONS" if artifacts or observations else "NO_OBSERVATIONS")
        )
        input_after = collect_input(
            input_path, args.max_input_files, args.max_input_bytes, args.max_file_bytes
        )[1]
        tool_after = hash_regular_file(
            tool_path, MAX_TOOL_BYTES, "show_kernel_debug_data"
        )
        if input_before != input_after:
            raise ParserContractError("input changed during parser execution")
        if tool_before != tool_after:
            raise ParserContractError("show_kernel_debug_data changed during execution")
        publish_artifacts(workspace / "parsed", artifact_path)
    kind_counts = Counter(item["kind"] for item in artifacts)
    report = {
        "schema_version": SCHEMA_VERSION,
        "mode": "parse-kernel-debug-data",
        "status": status,
        "request": {
            "declared_cann": args.cann,
            "timeout_seconds": args.timeout_seconds,
            "max_input_files": args.max_input_files,
            "max_input_bytes": args.max_input_bytes,
            "max_file_bytes": args.max_file_bytes,
            "max_output_files": args.max_output_files,
            "max_output_bytes": args.max_output_bytes,
            "max_stream_bytes": args.max_stream_bytes,
        },
        "input": input_record,
        "tool": tool_record,
        "probe_report": probe_record,
        "artifact_directory": str(artifact_path),
        "execution": execution,
        "artifacts": artifacts,
        "observations": observations,
        "diagnostics": diagnostics,
        "statistics": {
            "artifact_count": len(artifacts),
            "artifact_bytes": sum(item["bytes"] for item in artifacts),
            "artifact_kind_counts": [
                {"kind": kind, "count": count}
                for kind, count in sorted(kind_counts.items())
            ],
            "observation_count": len(observations),
            "diagnostic_count": len(diagnostics),
            "explicit_error_count": explicit_errors,
        },
        "policy": {
            "runs_target_workload": False,
            "generates_kernel_dump": False,
            "uses_private_input_copy": True,
            "trusts_exit_code_alone": False,
            "requires_probe_binding": probe_record is not None,
            "infers_tensor_correctness": False,
            "infers_root_cause": False,
            "maps_precision_impact": False,
            "no_observations_means": "no-output-artifact-or-supported-console-boundary-observed",
        },
    }
    errors = validate_report(report)
    if errors:
        raise ParserContractError(
            "internal report contract failed: " + "; ".join(errors)
        )
    return report, 1 if tool_error else 0


def validate_report(report: object) -> list[str]:
    if not isinstance(report, dict):
        return ["report must be an object"]
    artifacts = report.get("artifacts")
    observations = report.get("observations")
    diagnostics = report.get("diagnostics")
    statistics = report.get("statistics")
    execution = report.get("execution")
    if not all(
        isinstance(value, expected)
        for value, expected in (
            (artifacts, list),
            (observations, list),
            (diagnostics, list),
            (statistics, dict),
            (execution, dict),
        )
    ):
        return [
            "artifacts, observations, diagnostics, statistics or execution has invalid type"
        ]
    errors = []
    if [item.get("artifact_id") for item in artifacts] != [
        f"KDBG-ART-{index:06d}" for index in range(1, len(artifacts) + 1)
    ]:
        errors.append("artifact IDs are not sequential")
    if [item.get("observation_id") for item in observations] != [
        f"KDBG-OBS-{index:06d}" for index in range(1, len(observations) + 1)
    ]:
        errors.append("observation IDs are not sequential")
    explicit_errors = sum(item.get("level") == "error" for item in diagnostics)
    counts = Counter(item.get("kind") for item in artifacts)
    expected_statistics = {
        "artifact_count": len(artifacts),
        "artifact_bytes": sum(item.get("bytes", 0) for item in artifacts),
        "artifact_kind_counts": [
            {"kind": kind, "count": count} for kind, count in sorted(counts.items())
        ],
        "observation_count": len(observations),
        "diagnostic_count": len(diagnostics),
        "explicit_error_count": explicit_errors,
    }
    if statistics != expected_statistics:
        errors.append("statistics do not match artifacts, observations and diagnostics")
    tool_error = (
        execution.get("timed_out") is True
        or execution.get("output_limit_exceeded") is True
        or execution.get("returncode") != 0
        or explicit_errors > 0
    )
    expected_status = (
        "TOOL_ERROR"
        if tool_error
        else ("OBSERVATIONS" if artifacts or observations else "NO_OBSERVATIONS")
    )
    if report.get("status") != expected_status:
        errors.append("status does not match execution and observations")
    if report.get("probe_report") is not None and not isinstance(
        report["probe_report"], dict
    ):
        errors.append("probe_report must be an object or null")
    expected_policy = {
        "runs_target_workload": False,
        "generates_kernel_dump": False,
        "uses_private_input_copy": True,
        "trusts_exit_code_alone": False,
        "requires_probe_binding": report.get("probe_report") is not None,
        "infers_tensor_correctness": False,
        "infers_root_cause": False,
        "maps_precision_impact": False,
        "no_observations_means": "no-output-artifact-or-supported-console-boundary-observed",
    }
    if report.get("policy") != expected_policy:
        errors.append("parser policy is invalid")
    return errors


def write_report(path: Path, report: dict[str, Any]) -> None:
    output = path.expanduser().resolve()
    temporary: Path | None = None
    try:
        descriptor, name = tempfile.mkstemp(
            prefix=output.name + ".", suffix=".tmp", dir=output.parent
        )
        temporary = Path(name)
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
    except OSError as exc:
        raise ParserContractError(f"cannot write output report: {exc}") from exc
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main() -> int:
    args = parse_args()
    try:
        report, returncode = build_report(args)
        write_report(args.output, report)
    except (ParserContractError, OSError, KeyError, TypeError) as exc:
        print(f"error: {exc}", file=os.sys.stderr)
        return 2
    print(
        f"WROTE status={report['status']} artifacts={len(report['artifacts'])} "
        f"output={args.output.expanduser().resolve()}"
    )
    return returncode


if __name__ == "__main__":
    raise SystemExit(main())
