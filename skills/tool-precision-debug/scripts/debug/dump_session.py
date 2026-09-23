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

"""用途：复杂 DumpTensor 采集的可选记录关联；probe 可选，区分数值失败与运行失败及源码恢复。
使用方法（在 Skill 根目录执行）：
    python3 scripts/debug/dump_session.py --help

参数和示例见 scripts/usage/dump-session.md。
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import sys
import time
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from probe_binding import ProbeBindingError, bind_probe_report
from analysis.comparison_result import (
    evaluation_for_execution,
    execution_issue,
    validate_mismatch_exit_codes,
)


SCHEMA_VERSION = "1.0.0"


class DumpSessionError(ValueError):
    """The DumpTensor evidence session contract is invalid."""


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def snapshot_file(path_value: str | Path, label: str) -> dict[str, Any]:
    path = Path(path_value).expanduser()
    if path.is_symlink():
        raise DumpSessionError(f"{label} must not be a symbolic link: {path}")
    try:
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise DumpSessionError(f"cannot resolve {label}: {exc}") from exc
    if not resolved.is_file():
        raise DumpSessionError(f"{label} is not a regular file: {resolved}")
    return {
        "path": str(resolved),
        "size_bytes": resolved.stat().st_size,
        "sha256": sha256_file(resolved),
    }


def load_json_file(
    path_value: str | Path, label: str
) -> tuple[dict[str, Any], dict[str, Any]]:
    snapshot = snapshot_file(path_value, label)
    try:
        payload = json.loads(Path(snapshot["path"]).read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise DumpSessionError(f"cannot load {label} JSON: {exc}") from exc
    if not isinstance(payload, dict):
        raise DumpSessionError(f"{label} must be a JSON object")
    return payload, snapshot


def write_json_atomic(
    path: Path, payload: dict[str, Any], *, require_new: bool = False
) -> None:
    path = path.expanduser().resolve()
    if require_new and path.exists():
        raise DumpSessionError(f"output already exists: {path}")
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


def ensure_fresh_dump_directory(path_value: Path) -> Path:
    path = path_value.expanduser().resolve()
    if path.exists():
        if path.is_symlink() or not path.is_dir():
            raise DumpSessionError(f"dump path must be a real directory: {path}")
        if any(path.iterdir()):
            raise DumpSessionError(
                f"dump directory must be empty; choose a fresh directory instead of deleting data: {path}"
            )
    else:
        path.mkdir(parents=True)
    return path


def inventory_dump_files(root: Path) -> list[dict[str, Any]]:
    records = []
    for candidate in sorted(
        root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()
    ):
        if candidate.is_symlink():
            raise DumpSessionError(
                f"symbolic link inside dump directory is not allowed: {candidate}"
            )
        if candidate.is_file() and candidate.suffix.lower() == ".bin":
            snapshot = snapshot_file(candidate, "dump file")
            snapshot["relative_path"] = candidate.relative_to(root).as_posix()
            records.append(snapshot)
    return records


def validate_session(payload: dict[str, Any]) -> None:
    if payload.get("schema_version") != SCHEMA_VERSION:
        raise DumpSessionError(f"session schema_version must be {SCHEMA_VERSION}")
    if payload.get("status") not in {"PREPARED", "COLLECTED", "CLOSED"}:
        raise DumpSessionError("session status is invalid")
    if not isinstance(payload.get("sources"), list) or not payload["sources"]:
        raise DumpSessionError("session must freeze at least one source file")
    if not isinstance(payload.get("dump"), dict) or not payload["dump"].get(
        "directory"
    ):
        raise DumpSessionError("session dump contract is incomplete")


def prepare(args: argparse.Namespace) -> dict[str, Any]:
    session_path = args.session.expanduser().resolve()
    config_path = args.config.expanduser().resolve()
    if session_path == config_path or session_path.exists() or config_path.exists():
        raise DumpSessionError("session and config must be distinct new files")
    probe = (
        bind_probe_report(
            args.probe_report,
            expected_cann=args.cann,
            npu_arch=args.npu_arch,
            required_capabilities=["show-kernel-debug-data"],
        )
        if args.probe_report
        else None
    )
    dump_dir = ensure_fresh_dump_directory(args.dump_dir)
    if config_path.is_relative_to(dump_dir) or session_path.is_relative_to(dump_dir):
        raise DumpSessionError(
            "session and config must be outside the fresh dump directory"
        )
    sources = [
        snapshot_file(item, f"source {index}")
        for index, item in enumerate(args.source, 1)
    ]
    if len({item["path"] for item in sources}) != len(sources):
        raise DumpSessionError("--source paths must be unique")
    config = {
        "dump": {
            "dump_kernel_data": args.dump_kernel_data,
            "dump_path": str(dump_dir),
        }
    }
    write_json_atomic(config_path, config, require_new=True)
    prepared_ns = time.time_ns()
    session = {
        "schema_version": SCHEMA_VERSION,
        "status": "PREPARED",
        "profile": {"cann": args.cann, "npu_arch": str(args.npu_arch)},
        "prepared_at": now_iso(),
        "prepared_at_ns": prepared_ns,
        "probe_binding": probe,
        "sources": sources,
        "dump": {
            "directory": str(dump_dir),
            "config": snapshot_file(config_path, "dump config"),
            "kernel_data": args.dump_kernel_data,
            "fresh_at_prepare": True,
            "files": [],
        },
        "collection": None,
        "closure": None,
        "policy": {
            "runs_target_workload": False,
            "parses_dump": False,
            "modifies_instrumented_source": False,
            "requires_explicit_run_with_evidence": True,
            "infers_root_cause": False,
        },
    }
    write_json_atomic(session_path, session, require_new=True)
    return session


def expected_environment_hash(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def verify_probe_binding(
    expected: dict[str, Any],
    observed: Any,
    capability_id: str,
    label: str,
) -> None:
    if not isinstance(observed, dict):
        raise DumpSessionError(f"{label} lacks a capability-bound probe report")
    for key in (
        "path",
        "size_bytes",
        "sha256",
        "expected_cann",
        "npu_arch",
        "installation_root",
    ):
        if observed.get(key) != expected.get(key):
            raise DumpSessionError(
                f"{label} probe binding does not match the dump session: {key}"
            )
    capabilities = observed.get("capabilities")
    if not isinstance(capabilities, list) or not any(
        isinstance(item, dict)
        and item.get("id") == capability_id
        and item.get("status") == "available"
        for item in capabilities
    ):
        raise DumpSessionError(f"{label} probe binding lacks {capability_id}")


def collect(args: argparse.Namespace) -> dict[str, Any]:
    validate_mismatch_exit_codes(args.mismatch_exit_code)
    session, _ = load_json_file(args.session, "dump session")
    validate_session(session)
    if session["status"] != "PREPARED":
        raise DumpSessionError("collect requires a PREPARED session")
    metadata, metadata_record = load_json_file(args.run_metadata, "run metadata")
    if metadata.get("schema_version") != "1.0.0":
        raise DumpSessionError("run metadata must come from run_with_evidence.py")
    evaluation = evaluation_for_execution(args.evaluation_report, metadata)
    issue = execution_issue(
        metadata, args.mismatch_exit_code, evaluation["status"] if evaluation else None
    )
    if issue:
        raise DumpSessionError(issue)
    if metadata.get("started_at", "") < session["prepared_at"]:
        raise DumpSessionError("run metadata predates dump session preparation")
    if session.get("probe_binding") is not None:
        verify_probe_binding(
            session["probe_binding"],
            metadata.get("probe_binding"),
            "show-kernel-debug-data",
            "run metadata",
        )
    overrides = {
        item.get("name"): item.get("value_sha256")
        for item in metadata.get("environment_overrides", [])
        if isinstance(item, dict)
    }
    dump_dir = Path(session["dump"]["directory"])
    if overrides.get("ASCEND_DUMP_PATH") != expected_environment_hash(str(dump_dir)):
        raise DumpSessionError(
            "run metadata does not prove ASCEND_DUMP_PATH was bound to this fresh dump directory"
        )
    files = inventory_dump_files(dump_dir)
    if not files:
        raise DumpSessionError("explicit NPU command produced no fresh .bin dump files")
    session["status"] = "COLLECTED"
    session["collected_at"] = now_iso()
    session["dump"]["files"] = files
    session["collection"] = {
        "evaluation": archive_evaluation(evaluation, args.session, "collection"),
        "execution_returncode": metadata.get(
            "child_returncode", metadata.get("wrapper_returncode")
        ),
        "run_metadata": metadata_record,
        "command_argv": metadata.get("command_argv"),
        "cwd": metadata.get("cwd"),
        "dump_file_count": len(files),
        "dump_bytes": sum(item["size_bytes"] for item in files),
    }
    write_json_atomic(args.session, session)
    return session


def verify_parser_binding(session: dict[str, Any], report: dict[str, Any]) -> None:
    if (
        report.get("schema_version") != "1.0.0"
        or report.get("status") != "OBSERVATIONS"
    ):
        raise DumpSessionError("parser report must contain valid OBSERVATIONS")
    request = report.get("request")
    if (
        not isinstance(request, dict)
        or request.get("declared_cann") != session["profile"]["cann"]
    ):
        raise DumpSessionError(
            "parser report CANN version does not match the dump session"
        )
    parser_tool = report.get("tool")
    if (
        not isinstance(parser_tool, dict)
        or not isinstance(parser_tool.get("path"), str)
        or not parser_tool["path"]
    ):
        raise DumpSessionError("parser report lacks the actual tool path")
    if session.get("probe_binding") is not None:
        expected_probe = session["probe_binding"]
        parser_probe = report.get("probe_report")
        if not isinstance(parser_probe, dict) or any(
            parser_probe.get(observed_key) != expected_probe.get(expected_key)
            for observed_key, expected_key in (
                ("path", "path"),
                ("bytes", "size_bytes"),
                ("sha256", "sha256"),
            )
        ):
            raise DumpSessionError(
                "parser report is not bound to this session's probe report"
            )
        expected_entrypoint = next(
            (
                item.get("entrypoint")
                for item in expected_probe.get("capabilities", [])
                if isinstance(item, dict) and item.get("id") == "show-kernel-debug-data"
            ),
            None,
        )
        parser_tool = report.get("tool")
        if (
            not isinstance(expected_entrypoint, str)
            or not isinstance(parser_tool, dict)
            or parser_tool.get("path") != expected_entrypoint
        ):
            raise DumpSessionError(
                "parser report did not use the show_kernel_debug_data entrypoint bound by this session"
            )
    parser_input = report.get("input")
    parser_files = parser_input.get("files") if isinstance(parser_input, dict) else None
    if not isinstance(parser_files, list):
        raise DumpSessionError("parser report lacks frozen input files")
    expected = sorted(
        (item["relative_path"], item["size_bytes"], item["sha256"])
        for item in session["dump"]["files"]
    )
    observed = sorted(
        (item.get("relative_path"), item.get("bytes"), item.get("sha256"))
        for item in parser_files
        if isinstance(item, dict)
    )
    if observed != expected:
        raise DumpSessionError(
            "parser report is not bound to this session's fresh dump files"
        )


def close(args: argparse.Namespace) -> dict[str, Any]:
    validate_mismatch_exit_codes(args.mismatch_exit_code)
    session, _ = load_json_file(args.session, "dump session")
    validate_session(session)
    if session["status"] != "COLLECTED":
        raise DumpSessionError("close requires a COLLECTED session")
    parser_report, parser_record = load_json_file(args.parser_report, "parser report")
    verify_parser_binding(session, parser_report)
    checkpoint_report, checkpoint_record = load_json_file(
        args.checkpoint_report, "checkpoint report"
    )
    if (
        checkpoint_report.get("schema_version") != "1.0.0"
        or checkpoint_report.get("status") != "VALID"
        or checkpoint_report.get("policy", {}).get("infers_root_cause") is not False
    ):
        raise DumpSessionError(
            "checkpoint report is not a valid checkpoint_analyzer result"
        )
    clean_build, clean_build_record = load_json_file(
        args.clean_build_metadata, "clean build metadata"
    )
    if (
        clean_build.get("wrapper_returncode") != 0
        or clean_build.get("outcome") != "completed"
    ):
        raise DumpSessionError(
            "clean build after instrumentation removal did not succeed"
        )
    if clean_build.get("started_at", "") < session.get("collected_at", ""):
        raise DumpSessionError("clean build predates dump collection")
    replay, replay_record = load_json_file(
        args.clean_replay_metadata, "clean replay metadata"
    )
    evaluation = evaluation_for_execution(args.evaluation_report, replay)
    issue = execution_issue(
        replay, args.mismatch_exit_code, evaluation["status"] if evaluation else None
    )
    if issue:
        raise DumpSessionError(issue)
    if replay.get("started_at", "") < clean_build.get("finished_at", ""):
        raise DumpSessionError("clean replay predates the clean build")
    current_sources = [
        snapshot_file(item["path"], "restored source") for item in session["sources"]
    ]
    if current_sources != session["sources"]:
        raise DumpSessionError(
            "instrumented sources do not match the pre-instrumentation snapshots"
        )
    session["status"] = "CLOSED"
    session["closed_at"] = now_iso()
    session["closure"] = {
        "evaluation": archive_evaluation(evaluation, args.session, "replay"),
        "replay_returncode": replay.get(
            "child_returncode", replay.get("wrapper_returncode")
        ),
        "parser_tool": parser_report["tool"],
        "parser_report": parser_record,
        "checkpoint_report": checkpoint_record,
        "clean_build_metadata": clean_build_record,
        "clean_replay_metadata": replay_record,
        "instrumentation_removed": True,
        "source_snapshots_restored": current_sources,
    }
    write_json_atomic(args.session, session)
    return session


def archive_evaluation(
    record: dict[str, Any] | None, session: Path, step: str
) -> dict[str, Any] | None:
    if record is None:
        return None
    source = Path(record["path"])
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest() != record["sha256"]:
        raise DumpSessionError("evaluation report changed while recording the session")
    target = (
        session.expanduser()
        .resolve()
        .with_name(f"{session.stem}.{step}.evaluation.json")
    )
    if target.exists() and target.read_bytes() != content:
        raise DumpSessionError(
            f"evaluation archive already contains different data: {target}"
        )
    if target.is_symlink():
        raise DumpSessionError("evaluation archive must not be a symbolic link")
    target.write_bytes(content)
    return {**record, "path": str(target), "original_path": str(source)}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    prepare_parser = subparsers.add_parser("prepare")
    prepare_parser.add_argument("--session", required=True, type=Path)
    prepare_parser.add_argument("--config", required=True, type=Path)
    prepare_parser.add_argument("--dump-dir", required=True, type=Path)
    prepare_parser.add_argument("--probe-report", type=Path)
    prepare_parser.add_argument("--cann", required=True)
    prepare_parser.add_argument("--npu-arch", required=True)
    prepare_parser.add_argument("--source", required=True, action="append")
    prepare_parser.add_argument(
        "--dump-kernel-data",
        default="tensor",
        choices=("all", "printf", "tensor", "assert", "timestamp"),
    )
    collect_parser = subparsers.add_parser("collect")
    collect_parser.add_argument("--session", required=True, type=Path)
    collect_parser.add_argument("--run-metadata", required=True, type=Path)
    close_parser = subparsers.add_parser("close")
    close_parser.add_argument("--session", required=True, type=Path)
    close_parser.add_argument("--parser-report", required=True, type=Path)
    close_parser.add_argument("--checkpoint-report", required=True, type=Path)
    close_parser.add_argument("--clean-build-metadata", required=True, type=Path)
    close_parser.add_argument("--clean-replay-metadata", required=True, type=Path)
    for stage in (collect_parser, close_parser):
        stage.add_argument(
            "--evaluation-report",
            type=Path,
            help="对应执行生成的验收报告；非零数值失败时需要",
        )
        stage.add_argument(
            "--mismatch-exit-code",
            type=int,
            action="append",
            default=[],
            help="明确表示数值失败的退出码；可重复",
        )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        if args.command == "prepare":
            report = prepare(args)
        elif args.command == "collect":
            report = collect(args)
        else:
            report = close(args)
        print(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False))
        return 0
    except (DumpSessionError, ProbeBindingError, OSError, ValueError, TypeError) as exc:
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
