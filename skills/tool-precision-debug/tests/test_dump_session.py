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

"""dump_session 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_dump_session.py -q
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "debug" / "dump_session.py"
RUNNER = ROOT / "scripts" / "debug" / "run_with_evidence.py"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fake_probe(path: Path, cann: str = "9.0.0") -> None:
    path.write_text(
        json.dumps(
            {
                "schema_version": "1.1.0",
                "mode": "probe",
                "expected_cann": cann,
                "npu_arch": "2201",
                "requirements": {
                    "requested": ["show-kernel-debug-data"],
                    "satisfied": True,
                    "missing": [],
                },
                "installations": [
                    {
                        "root": str(path.parent / f"cann-{cann}"),
                        "version": {"value": cann, "match": "match"},
                        "capabilities": [
                            {
                                "id": "show-kernel-debug-data",
                                "status": "available",
                                "entrypoint": str(
                                    path.parent
                                    / f"cann-{cann}"
                                    / "show_kernel_debug_data"
                                ),
                                "target_probe": None,
                            }
                        ],
                    }
                ],
            }
        ),
        encoding="utf-8",
    )


def run(*arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(TOOL), *arguments],
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.mark.parametrize(
    "bound,mismatch", [(True, False), (False, False), (True, True), (False, True)]
)
def test_dump_session_closes_only_after_fresh_collection_and_source_restore(
    tmp_path: Path, bound: bool, mismatch: bool
):
    probe = tmp_path / "probe.json"
    fake_probe(probe)
    source = tmp_path / "kernel.cpp"
    source.write_text("clean-source\n", encoding="utf-8")
    session = tmp_path / "dump-session.json"
    config = tmp_path / "dump-config.json"
    dump_dir = tmp_path / "fresh-dump"
    prepared = run(
        "prepare",
        "--session",
        str(session),
        "--config",
        str(config),
        "--dump-dir",
        str(dump_dir),
        *(["--probe-report", str(probe)] if bound else []),
        "--cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--source",
        str(source),
    )
    assert prepared.returncode == 0, prepared.stderr
    prepared_session = json.loads(session.read_text(encoding="utf-8"))
    assert prepared_session["status"] == "PREPARED"
    assert prepared_session["sources"] == [
        {
            "path": str(source.resolve()),
            "size_bytes": source.stat().st_size,
            "sha256": sha256(source),
        }
    ]

    source.write_text("instrumented-source\n", encoding="utf-8")
    run_metadata = tmp_path / "npu.meta.json"
    npu = subprocess.run(
        [
            sys.executable,
            str(RUNNER),
            "--log",
            str(tmp_path / "npu.log"),
            "--metadata",
            str(run_metadata),
            "--env",
            f"ASCEND_DUMP_PATH={dump_dir}",
            *(
                [
                    "--probe-report",
                    str(probe),
                    "--require-capability",
                    "show-kernel-debug-data",
                    "--expected-cann",
                    "9.0.0",
                    "--npu-arch",
                    "2201",
                ]
                if bound
                else []
            ),
            "--",
            sys.executable,
            "-c",
            "from pathlib import Path; import sys,json; Path(sys.argv[1]).write_bytes(b'dump'); Path(sys.argv[2]).write_text(json.dumps({'is_pass': False})); sys.exit(int(sys.argv[3]))",
            str(dump_dir / "asc_kernel_data_0.bin"),
            str(tmp_path / "npu-evaluation.json"),
            str(int(mismatch)),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert npu.returncode == int(mismatch), npu.stderr
    options = (
        [
            "--mismatch-exit-code",
            "1",
            "--evaluation-report",
            str(tmp_path / "npu-evaluation.json"),
        ]
        if mismatch
        else []
    )
    if mismatch:
        rejected = run(
            "collect", "--session", str(session), "--run-metadata", str(run_metadata)
        )
        assert rejected.returncode == 2
    collected = run(
        "collect",
        "--session",
        str(session),
        "--run-metadata",
        str(run_metadata),
        *options,
    )
    assert collected.returncode == 0, collected.stderr
    assert json.loads(session.read_text(encoding="utf-8"))["status"] == "COLLECTED"

    dump_file = dump_dir / "asc_kernel_data_0.bin"
    parser_report = tmp_path / "parser.json"
    parser_report.write_text(
        json.dumps(
            {
                "schema_version": "1.0.0",
                "status": "OBSERVATIONS",
                "request": {"declared_cann": "9.0.0"},
                "probe_report": {
                    "path": str(probe.resolve()),
                    "bytes": probe.stat().st_size,
                    "sha256": sha256(probe),
                },
                "tool": {
                    "path": str(tmp_path / "cann-9.0.0" / "show_kernel_debug_data")
                },
                "input": {
                    "files": [
                        {
                            "relative_path": dump_file.name,
                            "bytes": dump_file.stat().st_size,
                            "sha256": sha256(dump_file),
                        }
                    ]
                },
            }
        ),
        encoding="utf-8",
    )
    parser_payload = json.loads(parser_report.read_text(encoding="utf-8"))
    if bound:
        parser_payload["probe_report"]["sha256"] = "0" * 64
        parser_report.write_text(json.dumps(parser_payload), encoding="utf-8")
        mismatched_parser = run(
            "close",
            "--session",
            str(session),
            "--parser-report",
            str(parser_report),
            "--checkpoint-report",
            str(tmp_path / "not-needed-checkpoint.json"),
            "--clean-build-metadata",
            str(tmp_path / "not-needed-build.json"),
            "--clean-replay-metadata",
            str(tmp_path / "not-needed-replay.json"),
        )
        assert mismatched_parser.returncode == 2
        assert "not bound to this session's probe report" in mismatched_parser.stderr
        parser_payload["probe_report"]["sha256"] = sha256(probe)
        parser_report.write_text(json.dumps(parser_payload), encoding="utf-8")
    else:
        parser_payload["probe_report"] = None
        parser_report.write_text(json.dumps(parser_payload))
    checkpoint_report = tmp_path / "checkpoint.json"
    checkpoint_report.write_text(
        json.dumps(
            {
                "schema_version": "1.0.0",
                "status": "VALID",
                "policy": {"infers_root_cause": False},
            }
        ),
        encoding="utf-8",
    )
    source.write_text("clean-source\n", encoding="utf-8")
    build_metadata = tmp_path / "build.meta.json"
    build = subprocess.run(
        [
            sys.executable,
            str(RUNNER),
            "--log",
            str(tmp_path / "build.log"),
            "--metadata",
            str(build_metadata),
            "--",
            sys.executable,
            "-c",
            "pass",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert build.returncode == 0, build.stderr
    replay_metadata = tmp_path / "replay.meta.json"
    replay = subprocess.run(
        [
            sys.executable,
            str(RUNNER),
            "--log",
            str(tmp_path / "replay.log"),
            "--metadata",
            str(replay_metadata),
            "--",
            sys.executable,
            "-c",
            "from pathlib import Path; import json,sys; Path(sys.argv[1]).write_text(json.dumps({'is_pass': False})); sys.exit(int(sys.argv[2]))",
            str(tmp_path / "replay-evaluation.json"),
            str(int(mismatch)),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert replay.returncode == int(mismatch), replay.stderr
    closed = run(
        "close",
        "--session",
        str(session),
        "--parser-report",
        str(parser_report),
        "--checkpoint-report",
        str(checkpoint_report),
        "--clean-build-metadata",
        str(build_metadata),
        "--clean-replay-metadata",
        str(replay_metadata),
        *(
            [
                "--mismatch-exit-code",
                "1",
                "--evaluation-report",
                str(tmp_path / "replay-evaluation.json"),
            ]
            if mismatch
            else []
        ),
    )
    assert closed.returncode == 0, closed.stderr
    payload = json.loads(session.read_text(encoding="utf-8"))
    assert payload["status"] == "CLOSED"
    assert payload["closure"]["instrumentation_removed"] is True
    assert payload["closure"]["replay_returncode"] == int(mismatch)
    assert payload["closure"]["parser_tool"]["path"] == str(
        tmp_path / "cann-9.0.0" / "show_kernel_debug_data"
    )
    if not bound:
        assert payload["probe_binding"] is None
    if mismatch:
        assert payload["closure"]["evaluation"]["status"] == "MISMATCH"
        assert json.loads(
            Path(payload["collection"]["evaluation"]["path"]).read_text()
        ) == {"is_pass": False}


def test_collect_rejects_an_unbound_workload(tmp_path: Path):
    probe = tmp_path / "probe.json"
    fake_probe(probe)
    source = tmp_path / "kernel.cpp"
    source.write_text("clean-source\n", encoding="utf-8")
    session = tmp_path / "dump-session.json"
    dump_dir = tmp_path / "fresh-dump"
    prepared = run(
        "prepare",
        "--session",
        str(session),
        "--config",
        str(tmp_path / "config.json"),
        "--dump-dir",
        str(dump_dir),
        "--probe-report",
        str(probe),
        "--cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--source",
        str(source),
    )
    assert prepared.returncode == 0, prepared.stderr
    metadata = tmp_path / "unbound.meta.json"
    workload = subprocess.run(
        [
            sys.executable,
            str(RUNNER),
            "--log",
            str(tmp_path / "unbound.log"),
            "--metadata",
            str(metadata),
            "--env",
            f"ASCEND_DUMP_PATH={dump_dir}",
            "--",
            sys.executable,
            "-c",
            "from pathlib import Path; import sys; Path(sys.argv[1]).write_bytes(b'dump')",
            str(dump_dir / "asc_kernel_data_0.bin"),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert workload.returncode == 0, workload.stderr
    collected = run(
        "collect", "--session", str(session), "--run-metadata", str(metadata)
    )
    assert collected.returncode == 2
    assert "lacks a capability-bound probe report" in collected.stderr


def test_prepare_refuses_nonempty_dump_directory(tmp_path: Path):
    probe = tmp_path / "probe.json"
    fake_probe(probe)
    source = tmp_path / "kernel.cpp"
    source.write_text("clean", encoding="utf-8")
    dump_dir = tmp_path / "dump"
    dump_dir.mkdir()
    (dump_dir / "old.bin").write_bytes(b"old")
    result = run(
        "prepare",
        "--session",
        str(tmp_path / "session.json"),
        "--config",
        str(tmp_path / "config.json"),
        "--dump-dir",
        str(dump_dir),
        "--probe-report",
        str(probe),
        "--cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--source",
        str(source),
    )
    assert result.returncode == 2
    assert "choose a fresh directory" in result.stderr


def test_unlisted_cann_version_can_prepare_a_dump_session(tmp_path):
    probe = tmp_path / "probe.json"
    fake_probe(probe, cann="9.2.0")
    source = tmp_path / "kernel.cpp"
    source.write_text("clean-source\n", encoding="utf-8")
    session = tmp_path / "session.json"
    result = run(
        "prepare",
        "--session",
        str(session),
        "--config",
        str(tmp_path / "config.json"),
        "--dump-dir",
        str(tmp_path / "fresh"),
        "--probe-report",
        str(probe),
        "--cann",
        "9.2.0",
        "--npu-arch",
        "2201",
        "--source",
        str(source),
    )
    assert result.returncode == 0, result.stderr
    assert json.loads(session.read_text())["profile"]["cann"] == "9.2.0"
