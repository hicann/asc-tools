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

"""用途：只读提取已有 npu check 日志的观察、原始行和完整性限制，不推断根因。
使用方法（在 Skill 根目录执行）：
    python3 scripts/debug/npuchk_parser.py --help

参数和示例见 scripts/usage/npuchk-parser.md。
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


SCHEMA_VERSION = "1.0.0"
LOG_PATTERN = "*_npuchk.log"
DEFAULT_MAX_FILES = 256
DEFAULT_MAX_FILE_BYTES = 16 * 1024 * 1024
DEFAULT_MAX_TOTAL_BYTES = 64 * 1024 * 1024
MAX_FILES_LIMIT = 10_000
MAX_BYTES_LIMIT = 4 * 1024 * 1024 * 1024
ERROR_PATTERN = re.compile(r"\[(Error[A-Za-z0-9_]+)\]")
CORE_PATTERN = re.compile(r"_(\d+)_(\d+)_(?:aiv|vec)_npuchk\.log$", re.IGNORECASE)
DOCUMENTED_ERROR_CODES = frozenset(
    {
        "ErrorRead1",
        "ErrorRead2",
        "ErrorRead3",
        "ErrorRead4",
        "ErrorWrite1",
        "ErrorWrite2",
        "ErrorWrite3",
        "ErrorWrite4",
        "ErrorSync1",
        "ErrorSync2",
        "ErrorSync3",
        "ErrorSync4",
        "ErrorLeak",
        "ErrorFree",
        "ErrorBuffer0",
        "ErrorBuffer1",
        "ErrorBuffer2",
        "ErrorBuffer3",
        "ErrorBuffer4",
    }
)


class ParserContractError(ValueError):
    """A user-correctable input or report contract error."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input", required=True, type=Path, help="npu check log or directory"
    )
    parser.add_argument("--output", required=True, type=Path, help="JSON report path")
    parser.add_argument(
        "--recursive", action="store_true", help="scan nested directories"
    )
    parser.add_argument("--cann", help="declared source environment")
    parser.add_argument("--max-files", type=int, default=DEFAULT_MAX_FILES)
    parser.add_argument("--max-file-bytes", type=int, default=DEFAULT_MAX_FILE_BYTES)
    parser.add_argument("--max-total-bytes", type=int, default=DEFAULT_MAX_TOTAL_BYTES)
    return parser.parse_args()


def validate_limits(max_files: int, max_file_bytes: int, max_total_bytes: int) -> None:
    if max_files < 1 or max_files > MAX_FILES_LIMIT:
        raise ParserContractError(
            f"--max-files must be between 1 and {MAX_FILES_LIMIT}"
        )
    for name, value in (
        ("--max-file-bytes", max_file_bytes),
        ("--max-total-bytes", max_total_bytes),
    ):
        if value < 1 or value > MAX_BYTES_LIMIT:
            raise ParserContractError(f"{name} must be between 1 and {MAX_BYTES_LIMIT}")
    if max_total_bytes < max_file_bytes:
        raise ParserContractError(
            "--max-total-bytes cannot be smaller than --max-file-bytes"
        )


def collect_files(
    input_path: Path, recursive: bool, max_files: int
) -> tuple[Path, str, list[Path]]:
    if input_path.is_symlink():
        raise ParserContractError(f"symbolic-link input is not allowed: {input_path}")
    try:
        resolved = input_path.expanduser().resolve(strict=True)
    except OSError as exc:
        raise ParserContractError(f"cannot resolve input: {exc}") from exc
    if resolved.is_file():
        return resolved.parent, "file", [resolved]
    if not resolved.is_dir():
        raise ParserContractError(
            f"input must be a regular file or directory: {resolved}"
        )

    iterator = resolved.rglob(LOG_PATTERN) if recursive else resolved.glob(LOG_PATTERN)
    files: list[Path] = []
    try:
        for candidate in iterator:
            if candidate.is_symlink():
                raise ParserContractError(
                    f"symbolic-link log is not allowed: {candidate.relative_to(resolved)}"
                )
            if not candidate.is_file():
                continue
            canonical = candidate.resolve(strict=True)
            if not canonical.is_relative_to(resolved):
                raise ParserContractError(f"log escapes input directory: {candidate}")
            files.append(canonical)
            if len(files) > max_files:
                raise ParserContractError(f"log count exceeds --max-files={max_files}")
    except OSError as exc:
        raise ParserContractError(f"cannot enumerate input directory: {exc}") from exc
    files.sort(key=lambda path: path.relative_to(resolved).as_posix())
    if not files:
        scope = "recursively" if recursive else "at directory top level"
        raise ParserContractError(f"no {LOG_PATTERN} files found {scope}: {resolved}")
    return resolved, "directory", files


def relative_name(path: Path, root: Path, input_kind: str) -> str:
    return path.name if input_kind == "file" else path.relative_to(root).as_posix()


def core_identity(filename: str) -> tuple[int | None, str]:
    match = CORE_PATTERN.search(filename)
    if match is None:
        return None, "unresolved"
    return int(match.group(1)), "official-filename"


def parse_text(text: str, file_name: str, core_id: int | None) -> list[dict[str, Any]]:
    observations: list[dict[str, Any]] = []
    last_intrinsic: dict[str, Any] | None = None
    active_group: list[dict[str, Any]] = []
    in_backtrace = False

    for line_number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        matches = list(ERROR_PATTERN.finditer(line))
        if matches:
            in_backtrace = False
            for match in matches:
                error_code = match.group(1)
                observation = {
                    "observation_id": "",
                    "file": file_name,
                    "core_id": core_id,
                    "line_number": line_number,
                    "error_code": error_code,
                    "code_status": (
                        "documented"
                        if error_code in DOCUMENTED_ERROR_CODES
                        else "unknown"
                    ),
                    "raw_line": line,
                    "intrinsic": dict(last_intrinsic) if last_intrinsic else None,
                    "backtrace": [],
                }
                observations.append(observation)
                active_group.append(observation)
            continue

        if stripped.startswith("### "):
            last_intrinsic = {"line_number": line_number, "raw_line": line}
            active_group = []
            in_backtrace = False
            continue

        if "# BackTrace #" in line:
            in_backtrace = True
            continue

        if in_backtrace:
            if line.startswith((" ", "\t")) and stripped:
                frame = {"line_number": line_number, "raw_line": line}
                for observation in active_group:
                    observation["backtrace"].append(dict(frame))
                continue
            in_backtrace = False
            active_group = []

    return observations


def read_snapshot(
    path: Path, max_file_bytes: int, max_total_bytes: int, total_read_bytes: int
) -> tuple[bytes, os.stat_result]:
    max_total_remaining = max_total_bytes - total_read_bytes
    try:
        flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
        descriptor = os.open(path, flags)
    except OSError as exc:
        raise ParserContractError(f"cannot open regular log {path}: {exc}") from exc
    try:
        with os.fdopen(descriptor, "rb") as stream:
            before = os.fstat(stream.fileno())
            if not stat.S_ISREG(before.st_mode):
                raise ParserContractError(f"input log is not a regular file: {path}")
            if before.st_size > max_file_bytes:
                raise ParserContractError(
                    f"log exceeds --max-file-bytes={max_file_bytes}: "
                    f"{path} ({before.st_size} bytes)"
                )
            if before.st_size > max_total_remaining:
                raise ParserContractError(
                    f"log bytes exceed --max-total-bytes={max_total_bytes}: "
                    f"cumulative size would be {total_read_bytes + before.st_size} "
                    f"at {path}"
                )
            read_limit = min(max_file_bytes, max_total_remaining)
            chunks: list[bytes] = []
            bytes_read = 0
            while bytes_read <= read_limit:
                chunk = stream.read(min(1024 * 1024, read_limit + 1 - bytes_read))
                if not chunk:
                    break
                chunks.append(chunk)
                bytes_read += len(chunk)
            data = b"".join(chunks)
            after = os.fstat(stream.fileno())
    except OSError as exc:
        raise ParserContractError(f"cannot read log {path}: {exc}") from exc
    if len(data) > max_file_bytes:
        raise ParserContractError(
            f"log grew beyond --max-file-bytes={max_file_bytes}: {path}"
        )
    if len(data) > max_total_remaining:
        raise ParserContractError(
            f"log bytes grew beyond --max-total-bytes={max_total_bytes}: "
            f"cumulative size exceeds the limit at {path}"
        )
    identity_before = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
    identity_after = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
    if identity_before != identity_after or len(data) != after.st_size:
        raise ParserContractError(f"log changed while being read: {path}")
    return data, after


def decode_utf8(data: bytes) -> tuple[str, bool]:
    try:
        return data.decode("utf-8"), False
    except UnicodeDecodeError:
        return data.decode("utf-8", errors="replace"), True


def build_report(
    input_path: Path,
    *,
    recursive: bool,
    declared_cann: str | None,
    max_files: int,
    max_file_bytes: int,
    max_total_bytes: int,
) -> dict[str, Any]:
    validate_limits(max_files, max_file_bytes, max_total_bytes)
    root, input_kind, paths = collect_files(input_path, recursive, max_files)

    file_records: list[dict[str, Any]] = []
    observations: list[dict[str, Any]] = []
    total_read_bytes = 0
    for path in paths:
        name = relative_name(path, root, input_kind)
        data, _ = read_snapshot(path, max_file_bytes, max_total_bytes, total_read_bytes)
        total_read_bytes += len(data)
        text, decode_errors = decode_utf8(data)
        core_id, core_source = core_identity(path.name)
        parsed = parse_text(text, name, core_id)
        observations.extend(parsed)
        file_records.append(
            {
                "path": name,
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
                "line_count": len(text.splitlines()),
                "decode_errors": decode_errors,
                "filename_contract": bool(CORE_PATTERN.search(path.name)),
                "core_id": core_id,
                "core_id_source": core_source,
                "observation_count": len(parsed),
            }
        )

    for index, observation in enumerate(observations, 1):
        observation["observation_id"] = f"NPUCHK-{index:06d}"
    counts = Counter(observation["error_code"] for observation in observations)
    documented_count = sum(
        1 for observation in observations if observation["code_status"] == "documented"
    )
    report = {
        "schema_version": SCHEMA_VERSION,
        "mode": "parse-existing-logs",
        "status": "OBSERVATIONS" if observations else "NO_OBSERVATIONS",
        "input": {
            "path": str(input_path.expanduser().resolve()),
            "kind": input_kind,
            "recursive": recursive,
            "pattern": LOG_PATTERN,
            "declared_cann": declared_cann,
            "max_files": max_files,
            "max_file_bytes": max_file_bytes,
            "max_total_bytes": max_total_bytes,
        },
        "files": file_records,
        "observations": observations,
        "statistics": {
            "file_count": len(file_records),
            "files_with_observations": sum(
                1 for record in file_records if record["observation_count"] > 0
            ),
            "total_bytes": total_read_bytes,
            "observation_count": len(observations),
            "documented_observation_count": documented_count,
            "unknown_observation_count": len(observations) - documented_count,
            "error_code_counts": [
                {"error_code": code, "count": count}
                for code, count in sorted(counts.items())
            ],
            "affected_core_ids": sorted(
                {
                    observation["core_id"]
                    for observation in observations
                    if observation["core_id"] is not None
                }
            ),
            "unresolved_core_file_count": sum(
                1 for record in file_records if record["core_id"] is None
            ),
        },
        "completeness": {
            "status": "unknown",
            "reason": (
                "asc-tools documents that npu check output may be incomplete when CPU Debug "
                "does not exit normally or hits an ASSERT; the text format has no verified "
                "completion marker."
            ),
        },
        "policy": {
            "read_only": True,
            "executes_target_workload": False,
            "infers_root_cause": False,
            "maps_precision_impact": False,
            "resolves_symbols": False,
            "no_observations_means": "no-supported-error-marker-observed",
        },
    }
    errors = validate_report(report)
    if errors:
        raise ParserContractError(
            "internal report contract failed: " + "; ".join(errors)
        )
    return report


def validate_report(report: object) -> list[str]:
    if not isinstance(report, dict):
        return ["report must be an object"]
    errors: list[str] = []
    files = report.get("files")
    observations = report.get("observations")
    statistics = report.get("statistics")
    if (
        not isinstance(files, list)
        or not isinstance(observations, list)
        or not isinstance(statistics, dict)
    ):
        return ["files, observations and statistics have invalid types"]
    file_by_path: dict[str, dict[str, Any]] = {}
    for record in files:
        if not isinstance(record, dict) or not isinstance(record.get("path"), str):
            errors.append("file record is invalid")
            continue
        if record["path"] in file_by_path:
            errors.append(f"duplicate file path: {record['path']}")
        file_by_path[record["path"]] = record
    expected_ids = [f"NPUCHK-{index:06d}" for index in range(1, len(observations) + 1)]
    actual_ids: list[object] = []
    observed_by_file: Counter[str] = Counter()
    code_counts: Counter[str] = Counter()
    documented_count = 0
    affected_cores: set[int] = set()
    for observation in observations:
        if not isinstance(observation, dict):
            errors.append("observation is not an object")
            continue
        actual_ids.append(observation.get("observation_id"))
        file_name = observation.get("file")
        if file_name not in file_by_path:
            errors.append(f"observation references unknown file: {file_name}")
            continue
        line_number = observation.get("line_number")
        if (
            not isinstance(line_number, int)
            or isinstance(line_number, bool)
            or line_number < 1
            or line_number > file_by_path[file_name].get("line_count", 0)
        ):
            errors.append(
                f"observation line is invalid: {observation.get('observation_id')}"
            )
        code = observation.get("error_code")
        if not isinstance(code, str) or ERROR_PATTERN.fullmatch(f"[{code}]") is None:
            errors.append(f"observation error code is invalid: {code!r}")
            continue
        expected_status = "documented" if code in DOCUMENTED_ERROR_CODES else "unknown"
        if observation.get("code_status") != expected_status:
            errors.append(
                f"observation code status is stale: {observation.get('observation_id')}"
            )
        observed_by_file[file_name] += 1
        code_counts[code] += 1
        documented_count += int(expected_status == "documented")
        core_id = observation.get("core_id")
        if core_id is not None:
            if not isinstance(core_id, int) or isinstance(core_id, bool) or core_id < 0:
                errors.append(
                    f"observation core ID is invalid: {observation.get('observation_id')}"
                )
            else:
                affected_cores.add(core_id)
    if actual_ids != expected_ids:
        errors.append("observation IDs are not sequential")
    for path, record in file_by_path.items():
        if record.get("observation_count") != observed_by_file[path]:
            errors.append(f"file observation count is stale: {path}")
    expected_counts = [
        {"error_code": code, "count": count}
        for code, count in sorted(code_counts.items())
    ]
    expected_statistics = {
        "file_count": len(files),
        "files_with_observations": sum(
            1 for value in observed_by_file.values() if value > 0
        ),
        "total_bytes": sum(
            record.get("bytes", 0) for record in files if isinstance(record, dict)
        ),
        "observation_count": len(observations),
        "documented_observation_count": documented_count,
        "unknown_observation_count": len(observations) - documented_count,
        "error_code_counts": expected_counts,
        "affected_core_ids": sorted(affected_cores),
        "unresolved_core_file_count": sum(
            1
            for record in files
            if isinstance(record, dict) and record.get("core_id") is None
        ),
    }
    if statistics != expected_statistics:
        errors.append("statistics do not match files and observations")
    expected_status = "OBSERVATIONS" if observations else "NO_OBSERVATIONS"
    if report.get("status") != expected_status:
        errors.append("report status does not match observations")
    completeness = report.get("completeness")
    if not isinstance(completeness, dict) or completeness.get("status") != "unknown":
        errors.append("log completeness must remain unknown")
    policy = report.get("policy")
    expected_policy = {
        "read_only": True,
        "executes_target_workload": False,
        "infers_root_cause": False,
        "maps_precision_impact": False,
        "resolves_symbols": False,
        "no_observations_means": "no-supported-error-marker-observed",
    }
    if policy != expected_policy:
        errors.append("parser policy is invalid")
    return errors


def write_report(path: Path, report: dict[str, Any], input_files: list[Path]) -> None:
    output = path.expanduser().resolve()
    if output in {item.resolve() for item in input_files}:
        raise ParserContractError("output path cannot overwrite an input log")
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
        raise ParserContractError(f"cannot write output report: {exc}") from exc
    finally:
        if "temporary" in locals() and temporary.exists():
            temporary.unlink()


def main() -> int:
    args = parse_args()
    try:
        report = build_report(
            args.input,
            recursive=args.recursive,
            declared_cann=args.cann,
            max_files=args.max_files,
            max_file_bytes=args.max_file_bytes,
            max_total_bytes=args.max_total_bytes,
        )
        input_root = Path(report["input"]["path"])
        input_files = (
            [input_root]
            if report["input"]["kind"] == "file"
            else [input_root / record["path"] for record in report["files"]]
        )
        write_report(args.output, report, input_files)
    except (ParserContractError, OSError, ValueError, TypeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(
        f"WROTE status={report['status']} files={report['statistics']['file_count']} "
        f"observations={report['statistics']['observation_count']} "
        f"output={args.output.expanduser().resolve()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
