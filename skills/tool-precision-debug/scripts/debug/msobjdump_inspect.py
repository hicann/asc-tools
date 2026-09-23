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

"""用途：直接调用指定 msobjdump，记录 ELF 结构、工具身份和失败；probe 绑定可选，不解释精度影响。
使用方法（在 Skill 根目录执行）：
    python3 scripts/debug/msobjdump_inspect.py --help

参数和示例见 scripts/usage/msobjdump-inspect.md。
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
import signal
import stat
import subprocess
import sys
import tempfile
import time
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


SCHEMA_VERSION = "1.0.0"
DEFAULT_TIMEOUT_SECONDS = 30
DEFAULT_MAX_INPUT_BYTES = 256 * 1024 * 1024
DEFAULT_MAX_OUTPUT_BYTES = 4 * 1024 * 1024
MAX_PROBE_REPORT_BYTES = 4 * 1024 * 1024
MAX_INPUT_BYTES = 4 * 1024 * 1024 * 1024
MAX_OUTPUT_BYTES = 64 * 1024 * 1024
MAX_TIMEOUT_SECONDS = 300
FUNCTION_META_PATTERN = re.compile(r"^\.ascend\.meta\.\s*\[(\d+)\]:\s*(.*)$")
ELF_LIST_PATTERN = re.compile(r"^ELF file\s+(\d+):\s*(.*)$", re.IGNORECASE)
BRACKET_FIELD_PATTERN = re.compile(r"^\[([A-Z][A-Z0-9 _.-]*)\]:\s*(.*)$")
PLAIN_FIELD_PATTERN = re.compile(r"^([A-Z][A-Z0-9_]*)\s*:\s*(.*)$")
BINARY_META_HEADER_PATTERN = re.compile(r"^\.ascend\.meta\s+META INFO$")


class InspectorContractError(ValueError):
    """A user-correctable inspection contract error."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--tool", required=True, type=Path, help="explicit msobjdump executable"
    )
    parser.add_argument(
        "--probe-report",
        type=Path,
        help="Optional asc_tools_probe.py report; when supplied, verify version and executable binding",
    )
    parser.add_argument(
        "--input", required=True, type=Path, help="ELF object or container"
    )
    parser.add_argument("--output", required=True, type=Path, help="JSON report path")
    parser.add_argument("--operation", required=True, choices=("dump", "list"))
    parser.add_argument("--cann", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--max-input-bytes", type=int, default=DEFAULT_MAX_INPUT_BYTES)
    parser.add_argument(
        "--max-output-bytes", type=int, default=DEFAULT_MAX_OUTPUT_BYTES
    )
    return parser.parse_args()


def validate_limits(
    timeout_seconds: int, max_input_bytes: int, max_output_bytes: int
) -> None:
    if timeout_seconds < 1 or timeout_seconds > MAX_TIMEOUT_SECONDS:
        raise InspectorContractError(
            f"--timeout-seconds must be between 1 and {MAX_TIMEOUT_SECONDS}"
        )
    if max_input_bytes < 1 or max_input_bytes > MAX_INPUT_BYTES:
        raise InspectorContractError(
            f"--max-input-bytes must be between 1 and {MAX_INPUT_BYTES}"
        )
    if max_output_bytes < 1 or max_output_bytes > MAX_OUTPUT_BYTES:
        raise InspectorContractError(
            f"--max-output-bytes must be between 1 and {MAX_OUTPUT_BYTES}"
        )


def regular_file_snapshot(path: Path, limit: int, label: str) -> dict[str, Any]:
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as exc:
        raise InspectorContractError(
            f"cannot open {label} as a regular file: {exc}"
        ) from exc
    try:
        with os.fdopen(descriptor, "rb") as stream:
            before = os.fstat(stream.fileno())
            if not stat.S_ISREG(before.st_mode):
                raise InspectorContractError(f"{label} is not a regular file: {path}")
            if before.st_size > limit:
                raise InspectorContractError(
                    f"{label} exceeds byte limit {limit}: {path} ({before.st_size} bytes)"
                )
            digest = hashlib.sha256()
            bytes_read = 0
            while True:
                chunk = stream.read(1024 * 1024)
                if not chunk:
                    break
                bytes_read += len(chunk)
                if bytes_read > limit:
                    raise InspectorContractError(
                        f"{label} grew beyond byte limit {limit}: {path}"
                    )
                digest.update(chunk)
            after = os.fstat(stream.fileno())
    except OSError as exc:
        raise InspectorContractError(f"cannot read {label}: {exc}") from exc
    before_identity = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
    after_identity = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
    if before_identity != after_identity or bytes_read != after.st_size:
        raise InspectorContractError(f"{label} changed while being hashed: {path}")
    return {
        "path": str(path),
        "bytes": bytes_read,
        "sha256": digest.hexdigest(),
        "identity": list(after_identity),
    }


def resolve_input(path: Path, limit: int) -> tuple[Path, dict[str, Any]]:
    expanded = path.expanduser()
    if expanded.is_symlink():
        raise InspectorContractError(f"symbolic-link input is not allowed: {expanded}")
    try:
        resolved = expanded.resolve(strict=True)
    except OSError as exc:
        raise InspectorContractError(f"cannot resolve input: {exc}") from exc
    snapshot = regular_file_snapshot(resolved, limit, "input")
    snapshot.pop("identity")
    return resolved, snapshot


def resolve_tool(path: Path) -> tuple[Path, dict[str, Any]]:
    try:
        resolved = path.expanduser().resolve(strict=True)
    except OSError as exc:
        raise InspectorContractError(
            f"cannot resolve msobjdump executable: {exc}"
        ) from exc
    if not os.access(resolved, os.X_OK):
        raise InspectorContractError(f"msobjdump is not executable: {resolved}")
    snapshot = regular_file_snapshot(resolved, MAX_OUTPUT_BYTES, "msobjdump executable")
    snapshot.pop("identity")
    snapshot["requested_path"] = str(path.expanduser().absolute())
    return resolved, snapshot


def load_probe_binding(
    path: Path, declared_cann: str, tool_path: Path
) -> tuple[Path, dict[str, Any]]:
    expanded = path.expanduser()
    if expanded.is_symlink():
        raise InspectorContractError(
            f"symbolic-link probe report is not allowed: {expanded}"
        )
    try:
        resolved = expanded.resolve(strict=True)
    except OSError as exc:
        raise InspectorContractError(f"cannot resolve probe report: {exc}") from exc
    snapshot = regular_file_snapshot(resolved, MAX_PROBE_REPORT_BYTES, "probe report")
    snapshot.pop("identity")
    try:
        report = json.loads(resolved.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise InspectorContractError(f"cannot load probe report JSON: {exc}") from exc
    if not isinstance(report, dict) or report.get("schema_version") != "1.1.0":
        raise InspectorContractError(
            "probe report must be an asc-tools probe schema 1.1.0 object"
        )
    if report.get("mode") != "probe" or report.get("expected_cann") != declared_cann:
        raise InspectorContractError(
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
        or "msobjdump" not in requested
        or requirements.get("satisfied") is not True
    ):
        raise InspectorContractError(
            "probe report must satisfy an explicit msobjdump requirement"
        )
    matches: list[dict[str, Any]] = []
    for installation in report.get("installations", []):
        if not isinstance(installation, dict):
            continue
        version = installation.get("version")
        if not isinstance(version, dict) or version.get("value") != declared_cann:
            continue
        for capability in installation.get("capabilities", []):
            if (
                isinstance(capability, dict)
                and capability.get("id") == "msobjdump"
                and capability.get("status") == "available"
            ):
                matches.append(capability)
    bound = False
    for capability in matches:
        entrypoint = capability.get("entrypoint")
        if not isinstance(entrypoint, str) or not entrypoint:
            continue
        try:
            bound = Path(entrypoint).expanduser().resolve(strict=True) == tool_path
        except OSError:
            bound = False
        if bound:
            break
    if not bound:
        raise InspectorContractError(
            "probe report does not bind the requested msobjdump executable as available"
        )
    return resolved, snapshot


def set_output_limit(max_output_bytes: int) -> None:
    resource.setrlimit(resource.RLIMIT_FSIZE, (max_output_bytes, max_output_bytes))


def stop_process_group(process: subprocess.Popen[bytes]) -> dict[str, Any]:
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


def read_capture(path: Path, max_output_bytes: int) -> dict[str, Any]:
    size = path.stat().st_size
    digest = hashlib.sha256()
    chunks: list[bytes] = []
    captured = 0
    with path.open("rb") as stream:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            if captured < max_output_bytes:
                kept = chunk[: max_output_bytes - captured]
                chunks.append(kept)
                captured += len(kept)
    data = b"".join(chunks)
    try:
        text = data.decode("utf-8")
        decode_errors = False
    except UnicodeDecodeError:
        text = data.decode("utf-8", errors="replace")
        decode_errors = True
    return {
        "bytes": size,
        "sha256": digest.hexdigest(),
        "decode_errors": decode_errors,
        "truncated": size >= max_output_bytes,
        "text": text,
    }


def run_msobjdump(
    tool: Path,
    input_path: Path,
    operation: str,
    timeout_seconds: int,
    max_output_bytes: int,
) -> dict[str, Any]:
    option = "--dump-elf" if operation == "dump" else "--list-elf"
    argv = [str(tool), option, str(input_path)]
    started = time.monotonic()
    timed_out = False
    cleanup = None
    with tempfile.TemporaryDirectory(prefix="asc-msobjdump-") as temporary_name:
        temporary = Path(temporary_name)
        stdout_path = temporary / "stdout.log"
        stderr_path = temporary / "stderr.log"
        try:
            with (
                stdout_path.open("wb") as stdout_stream,
                stderr_path.open("wb") as stderr_stream,
            ):
                process = subprocess.Popen(
                    argv,
                    cwd=temporary,
                    stdin=subprocess.DEVNULL,
                    stdout=stdout_stream,
                    stderr=stderr_stream,
                    start_new_session=True,
                    preexec_fn=lambda: set_output_limit(max_output_bytes),
                )
                try:
                    returncode = process.wait(timeout=timeout_seconds)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    cleanup = stop_process_group(process)
                    returncode = process.returncode
        except OSError as exc:
            raise InspectorContractError(f"cannot launch msobjdump: {exc}") from exc
        stdout = read_capture(stdout_path, max_output_bytes)
        stderr = read_capture(stderr_path, max_output_bytes)
    output_limit_exceeded = (
        stdout["truncated"]
        or stderr["truncated"]
        or returncode in {-signal.SIGXFSZ, 128 + signal.SIGXFSZ}
    )
    return {
        "argv": argv,
        "returncode": returncode,
        "timed_out": timed_out,
        "output_limit_exceeded": output_limit_exceeded,
        "duration_seconds": round(time.monotonic() - started, 6),
        "timeout_cleanup": cleanup,
        "stdout": stdout,
        "stderr": stderr,
    }


def parse_observations(text: str) -> list[dict[str, Any]]:
    observations: list[dict[str, Any]] = []
    for line_number, line in enumerate(text.splitlines(), 1):
        match = FUNCTION_META_PATTERN.match(line)
        if match:
            observations.append(
                {
                    "observation_id": "",
                    "kind": "function-meta",
                    "line_number": line_number,
                    "raw_line": line,
                    "name": ".ascend.meta.",
                    "value": match.group(2),
                    "index": int(match.group(1)),
                }
            )
            continue
        match = ELF_LIST_PATTERN.match(line)
        if match:
            observations.append(
                {
                    "observation_id": "",
                    "kind": "elf-list-item",
                    "line_number": line_number,
                    "raw_line": line,
                    "name": "ELF FILE",
                    "value": match.group(2),
                    "index": int(match.group(1)),
                }
            )
            continue
        match = BRACKET_FIELD_PATTERN.match(line)
        if match:
            name = match.group(1)
            if name in {"ERROR", "WARNING"}:
                continue
            index_match = re.fullmatch(r"ELF FILE\s+(\d+)", name)
            observations.append(
                {
                    "observation_id": "",
                    "kind": "elf-file" if index_match else "container-field",
                    "line_number": line_number,
                    "raw_line": line,
                    "name": "ELF FILE" if index_match else name,
                    "value": match.group(2),
                    "index": int(index_match.group(1)) if index_match else None,
                }
            )
            continue
        match = PLAIN_FIELD_PATTERN.match(line)
        if match:
            observations.append(
                {
                    "observation_id": "",
                    "kind": "metadata-field",
                    "line_number": line_number,
                    "raw_line": line,
                    "name": match.group(1),
                    "value": match.group(2),
                    "index": None,
                }
            )
            continue
        if BINARY_META_HEADER_PATTERN.match(line):
            observations.append(
                {
                    "observation_id": "",
                    "kind": "binary-meta-header",
                    "line_number": line_number,
                    "raw_line": line,
                    "name": ".ascend.meta",
                    "value": "META INFO",
                    "index": None,
                }
            )
    for index, observation in enumerate(observations, 1):
        observation["observation_id"] = f"MSOBJ-{index:06d}"
    return observations


def parse_diagnostics(stdout: str, stderr: str) -> list[dict[str, Any]]:
    diagnostics: list[dict[str, Any]] = []
    for stream_name, text in (("stdout", stdout), ("stderr", stderr)):
        for line_number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith("[ERROR]"):
                level = "error"
            elif stripped.startswith("[WARNING]"):
                level = "warning"
            elif "kernel meta information cannot be found" in stripped.lower():
                level = "notice"
            else:
                continue
            diagnostics.append(
                {
                    "stream": stream_name,
                    "line_number": line_number,
                    "level": level,
                    "raw_line": line,
                }
            )
    return diagnostics


def build_report(args: argparse.Namespace) -> tuple[dict[str, Any], int]:
    validate_limits(args.timeout_seconds, args.max_input_bytes, args.max_output_bytes)
    input_path, input_record = resolve_input(args.input, args.max_input_bytes)
    tool_path, tool_record = resolve_tool(args.tool)
    probe_path, probe_record = (
        load_probe_binding(args.probe_report, args.cann, tool_path)
        if args.probe_report is not None
        else (None, None)
    )
    output_path = args.output.expanduser().resolve()
    if output_path in {input_path, tool_path, probe_path}:
        raise InspectorContractError(
            "output path cannot overwrite the input, msobjdump or probe report"
        )

    input_before = regular_file_snapshot(input_path, args.max_input_bytes, "input")
    tool_before = regular_file_snapshot(
        tool_path, MAX_OUTPUT_BYTES, "msobjdump executable"
    )
    execution = run_msobjdump(
        tool_path,
        input_path,
        args.operation,
        args.timeout_seconds,
        args.max_output_bytes,
    )
    input_after = regular_file_snapshot(input_path, args.max_input_bytes, "input")
    tool_after = regular_file_snapshot(
        tool_path, MAX_OUTPUT_BYTES, "msobjdump executable"
    )
    if input_before != input_after:
        raise InspectorContractError("input changed during msobjdump execution")
    if tool_before != tool_after:
        raise InspectorContractError("msobjdump executable changed during execution")

    observations = parse_observations(execution["stdout"]["text"])
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
        else ("OBSERVATIONS" if observations else "NO_OBSERVATIONS")
    )
    kind_counts = Counter(item["kind"] for item in observations)
    report = {
        "schema_version": SCHEMA_VERSION,
        "mode": "inspect-elf",
        "status": status,
        "request": {
            "operation": args.operation,
            "declared_cann": args.cann,
            "timeout_seconds": args.timeout_seconds,
            "max_input_bytes": args.max_input_bytes,
            "max_output_bytes_per_stream": args.max_output_bytes,
        },
        "input": input_record,
        "tool": tool_record,
        "probe_report": probe_record,
        "execution": execution,
        "observations": observations,
        "diagnostics": diagnostics,
        "statistics": {
            "observation_count": len(observations),
            "diagnostic_count": len(diagnostics),
            "explicit_error_count": explicit_errors,
            "observation_kind_counts": [
                {"kind": kind, "count": count}
                for kind, count in sorted(kind_counts.items())
            ],
            "field_names": sorted({item["name"] for item in observations}),
        },
        "policy": {
            "read_only": True,
            "extracts_files": False,
            "supports_verbose": False,
            "trusts_exit_code_alone": False,
            "requires_probe_binding": probe_record is not None,
            "infers_root_cause": False,
            "maps_precision_impact": False,
            "no_observations_means": "no-supported-msobjdump-field-marker-observed",
        },
    }
    errors = validate_report(report)
    if errors:
        raise InspectorContractError(
            "internal report contract failed: " + "; ".join(errors)
        )
    return report, 1 if tool_error else 0


def validate_report(report: object) -> list[str]:
    if not isinstance(report, dict):
        return ["report must be an object"]
    observations = report.get("observations")
    diagnostics = report.get("diagnostics")
    statistics = report.get("statistics")
    execution = report.get("execution")
    if not all(
        isinstance(item, expected)
        for item, expected in (
            (observations, list),
            (diagnostics, list),
            (statistics, dict),
            (execution, dict),
        )
    ):
        return ["observations, diagnostics, statistics or execution has invalid type"]
    errors: list[str] = []
    expected_ids = [f"MSOBJ-{index:06d}" for index in range(1, len(observations) + 1)]
    if [
        item.get("observation_id") for item in observations if isinstance(item, dict)
    ] != expected_ids:
        errors.append("observation IDs are not sequential")
    kind_counts = Counter(
        item.get("kind") for item in observations if isinstance(item, dict)
    )
    explicit_errors = sum(
        isinstance(item, dict) and item.get("level") == "error" for item in diagnostics
    )
    expected_statistics = {
        "observation_count": len(observations),
        "diagnostic_count": len(diagnostics),
        "explicit_error_count": explicit_errors,
        "observation_kind_counts": [
            {"kind": kind, "count": count}
            for kind, count in sorted(kind_counts.items())
        ],
        "field_names": sorted(
            {
                item.get("name")
                for item in observations
                if isinstance(item, dict) and isinstance(item.get("name"), str)
            }
        ),
    }
    if statistics != expected_statistics:
        errors.append("statistics do not match observations and diagnostics")
    tool_error = (
        execution.get("timed_out") is True
        or execution.get("output_limit_exceeded") is True
        or execution.get("returncode") != 0
        or explicit_errors > 0
    )
    expected_status = (
        "TOOL_ERROR"
        if tool_error
        else ("OBSERVATIONS" if observations else "NO_OBSERVATIONS")
    )
    if report.get("status") != expected_status:
        errors.append("status does not match execution and observations")
    if report.get("probe_report") is not None and not isinstance(
        report["probe_report"], dict
    ):
        errors.append("probe_report must be an object or null")
    expected_policy = {
        "read_only": True,
        "extracts_files": False,
        "supports_verbose": False,
        "trusts_exit_code_alone": False,
        "requires_probe_binding": report.get("probe_report") is not None,
        "infers_root_cause": False,
        "maps_precision_impact": False,
        "no_observations_means": "no-supported-msobjdump-field-marker-observed",
    }
    if report.get("policy") != expected_policy:
        errors.append("inspection policy is invalid")
    return errors


def write_report(path: Path, report: dict[str, Any]) -> None:
    output = path.expanduser().resolve()
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=output.name + ".", suffix=".tmp", dir=output.parent
        )
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
    except OSError as exc:
        raise InspectorContractError(f"cannot write output report: {exc}") from exc
    finally:
        if "temporary" in locals() and temporary.exists():
            temporary.unlink()


def main() -> int:
    args = parse_args()
    try:
        report, returncode = build_report(args)
        write_report(args.output, report)
    except (InspectorContractError, OSError, KeyError, TypeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(
        f"WROTE status={report['status']} observations={len(report['observations'])} "
        f"output={args.output.expanduser().resolve()}"
    )
    return returncode


if __name__ == "__main__":
    raise SystemExit(main())
