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

"""run_with_evidence 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_run_with_evidence.py -q
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest


ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "scripts" / "debug" / "run_with_evidence.py"


def invoke(
    *arguments: str, timeout: float = 10, cwd: Path | None = None
) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return subprocess.run(
        [sys.executable, str(RUNNER), *arguments],
        cwd=cwd or ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        env=environment,
        check=False,
    )


def test_preserves_failure_code_and_freezes_combined_log(tmp_path: Path):
    log = tmp_path / "failure.log"
    result = invoke(
        "--log",
        str(log),
        "--label",
        "original-failure",
        "--tail-lines",
        "2",
        "--",
        sys.executable,
        "-u",
        "-c",
        "import sys; print('stdout-line'); print('stderr-line', file=sys.stderr); sys.exit(7)",
    )
    assert result.returncode == 7
    assert log.read_text(encoding="utf-8").splitlines() == [
        "stdout-line",
        "stderr-line",
    ]
    assert result.stdout.splitlines() == ["stdout-line", "stderr-line"]

    metadata_path = Path(f"{log.resolve()}.meta.json")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    assert metadata["outcome"] == "completed"
    assert metadata["child_returncode"] == 7
    assert metadata["wrapper_returncode"] == 7
    assert metadata["label"] == "original-failure"
    assert metadata["command_argv"][0] == sys.executable
    assert metadata["executable"]["path"] == str(Path(sys.executable).resolve())
    assert (
        metadata["executable"]["sha256_before_execution"]
        == hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()
    )
    assert metadata["log"]["sha256"] == hashlib.sha256(log.read_bytes()).hexdigest()


def test_rejects_overwrite_and_naked_shell_pipeline(tmp_path: Path):
    existing = tmp_path / "existing.log"
    existing.write_text("frozen\n", encoding="utf-8")
    overwrite = invoke("--log", str(existing), "--", sys.executable, "-c", "print(1)")
    assert overwrite.returncode == 2
    assert "已存在" in overwrite.stderr
    assert existing.read_text(encoding="utf-8") == "frozen\n"

    pipeline_log = tmp_path / "pipeline.log"
    pipeline = invoke(
        "--log",
        str(pipeline_log),
        "--",
        "bash",
        "-c",
        "printf x | grep x",
    )
    assert pipeline.returncode == 2
    assert "pipefail" in pipeline.stderr
    assert not pipeline_log.exists()

    pipefail_log = tmp_path / "pipefail.log"
    pipefail = invoke(
        "--log",
        str(pipefail_log),
        "--",
        "bash",
        "-c",
        "set -o pipefail; false | cat",
    )
    assert pipefail.returncode == 1
    assert pipefail_log.is_file()


def test_heartbeat_can_be_disabled_for_a_no_timeout_command(tmp_path: Path):
    log = tmp_path / "no-heartbeat.log"
    result = invoke(
        "--log",
        str(log),
        "--heartbeat-interval",
        "0",
        "--",
        sys.executable,
        "-c",
        "import time; time.sleep(0.05)",
    )
    assert result.returncode == 0, result.stderr
    assert "elapsed_seconds=" not in result.stderr


def test_rejects_invalid_heartbeat_intervals_without_starting_a_command(tmp_path: Path):
    for index, value in enumerate(("-1", "nan", "inf")):
        log = tmp_path / f"invalid-heartbeat-{index}.log"
        result = invoke(
            "--log",
            str(log),
            "--heartbeat-interval",
            value,
            "--",
            sys.executable,
            "-c",
            "raise SystemExit(99)",
        )
        assert result.returncode == 2
        assert "--heartbeat-interval" in result.stderr
        assert not log.exists()


def test_heartbeat_is_visible_on_stderr_and_not_in_evidence_log(tmp_path: Path):
    log = tmp_path / "heartbeat.log"
    result = invoke(
        "--log",
        str(log),
        "--heartbeat-interval",
        "0.02",
        "--",
        sys.executable,
        "-u",
        "-c",
        "import time; print('child-start'); time.sleep(0.12); print('child-done')",
    )
    assert result.returncode == 0, result.stderr
    heartbeat_lines = [
        line
        for line in result.stderr.splitlines()
        if "elapsed_seconds=" in line and "log_size_bytes=" in line
    ]
    assert heartbeat_lines
    assert log.read_text(encoding="utf-8").splitlines() == [
        "child-start",
        "child-done",
    ]
    assert "heartbeat" not in log.read_text(encoding="utf-8")


def test_timeout_returns_124_and_records_cleanup(tmp_path: Path):
    log = tmp_path / "timeout.log"
    result = invoke(
        "--log",
        str(log),
        "--timeout",
        "0.1",
        "--heartbeat-interval",
        "0.02",
        "--terminate-grace",
        "0.1",
        "--",
        sys.executable,
        "-c",
        "import time; time.sleep(5)",
    )
    assert result.returncode == 124
    metadata = json.loads(
        Path(f"{log.resolve()}.meta.json").read_text(encoding="utf-8")
    )
    assert metadata["outcome"] == "timeout"
    assert metadata["timed_out"] is True
    assert metadata["wrapper_returncode"] == 124
    assert metadata["timeout_cleanup"]["cleanup_complete"] is True
    assert "elapsed_seconds=" in result.stderr


def test_missing_executable_returns_127_with_auditable_metadata(tmp_path: Path):
    log = tmp_path / "missing.log"
    result = invoke(
        "--log", str(log), "--", "asc-precision-command-that-does-not-exist"
    )
    assert result.returncode == 127
    metadata = json.loads(
        Path(f"{log.resolve()}.meta.json").read_text(encoding="utf-8")
    )
    assert metadata["outcome"] == "launch_failed"
    assert metadata["child_returncode"] is None
    assert metadata["wrapper_returncode"] == 127
    assert log.is_file()


def test_capability_bound_execution_records_probe_and_redacted_environment(
    tmp_path: Path,
):
    probe = tmp_path / "probe.json"
    probe.write_text(
        json.dumps(
            {
                "schema_version": "1.1.0",
                "mode": "probe",
                "expected_cann": "9.0.0",
                "npu_arch": "2201",
                "requirements": {
                    "requested": ["cpu-debug"],
                    "satisfied": True,
                    "missing": [],
                },
                "installations": [
                    {
                        "root": str(tmp_path / "cann-9.0.0"),
                        "version": {"value": "9.0.0", "match": "match"},
                        "capabilities": [
                            {
                                "id": "cpu-debug",
                                "status": "available",
                                "entrypoint": None,
                                "target_probe": {
                                    "attempted": True,
                                    "returncode": 0,
                                    "npu_arch": "2201",
                                    "cmake_target": "tikicpulib::ascend910B1",
                                },
                            }
                        ],
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    log = tmp_path / "cpu.log"
    metadata = tmp_path / "cpu.json"
    result = invoke(
        "--log",
        str(log),
        "--metadata",
        str(metadata),
        "--probe-report",
        str(probe),
        "--require-capability",
        "cpu-debug",
        "--expected-cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--env",
        "ASCENDC_CPU_DEBUG_OUTPUT=2",
        "--",
        sys.executable,
        "-c",
        "import os; assert os.environ['ASCENDC_CPU_DEBUG_OUTPUT'] == '2'",
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(metadata.read_text(encoding="utf-8"))
    assert payload["probe_binding"]["capabilities"] == [
        {
            "id": "cpu-debug",
            "status": "available",
            "entrypoint": None,
            "cmake_target": "tikicpulib::ascend910B1",
        }
    ]
    override = payload["environment_overrides"][0]
    assert override["name"] == "ASCENDC_CPU_DEBUG_OUTPUT"
    assert override["value_sha256"] != "2"


def target_metadata(log: Path) -> dict:
    payload = json.loads(Path(f"{log.resolve()}.meta.json").read_text(encoding="utf-8"))
    return payload["validation_target"]


def test_validation_target_direct_executable_and_preexecution_identity(tmp_path: Path):
    target = tmp_path / "runner.py"
    target.write_text(f"#!{sys.executable}\n", encoding="utf-8")
    target.chmod(0o755)
    log = tmp_path / "direct.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(log),
        "--validation-target",
        "runner.py",
        "--",
        "./runner.py",
        cwd=tmp_path,
    )
    assert result.returncode == 0
    target.write_text("changed", encoding="utf-8")
    frozen = target_metadata(log)
    assert frozen["adopted"] is True
    assert frozen["adoption_method"] == "resolved-executable"
    assert frozen["sha256"] != hashlib.sha256(target.read_bytes()).hexdigest()


def test_validation_target_interpreter_script_and_not_adopted(tmp_path: Path):
    script = tmp_path / "input.py"
    script.write_text("print('ok')\n", encoding="utf-8")
    other = tmp_path / "other.py"
    other.write_text("print('other')\n", encoding="utf-8")
    log = tmp_path / "argv.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(log),
        "--validation-target",
        "input.py",
        "--",
        sys.executable,
        "input.py",
        cwd=tmp_path,
    )
    assert result.returncode == 0
    assert target_metadata(log)["adoption_method"] == "interpreter-script"
    log = tmp_path / "other.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(log),
        "--validation-target",
        "other.py",
        "--",
        sys.executable,
        "input.py",
        cwd=tmp_path,
    )
    assert result.returncode == 0
    assert target_metadata(log) == {
        "path": "other.py",
        "sha256": hashlib.sha256(other.read_bytes()).hexdigest(),
        "size_bytes": other.stat().st_size,
        "adoption_method": "not-in-executed-argv",
        "adopted": False,
        "reason": "not-adopted",
    }


def test_validation_target_rejects_unsafe_and_records_unavailable(tmp_path: Path):
    log = tmp_path / "bad.log"
    for target in ("../outside", str(tmp_path / "absolute")):
        result = invoke(
            "--log",
            str(log),
            "--validation-target",
            target,
            "--",
            sys.executable,
            "-c",
            "pass",
        )
        assert result.returncode == 2
    missing_log = tmp_path / "missing.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(missing_log),
        "--validation-target",
        "missing",
        "--",
        sys.executable,
        "-c",
        "pass",
        cwd=tmp_path,
    )
    assert result.returncode == 0
    assert target_metadata(missing_log)["reason"] == "missing"
    link = tmp_path / "link"
    link.symlink_to(sys.executable)
    link_log = tmp_path / "link.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(link_log),
        "--validation-target",
        "link",
        "--",
        sys.executable,
        "-c",
        "pass",
        cwd=tmp_path,
    )
    assert result.returncode == 0
    assert target_metadata(link_log)["reason"] == "symlink-not-allowed"


@pytest.mark.parametrize("relative_path", [False, True])
@pytest.mark.parametrize("target_dir", ["original-bin", "effective-bin"])
def test_identity_uses_child_path_and_cwd(
    tmp_path: Path, monkeypatch, relative_path: bool, target_dir: str
):
    for directory, message in (
        ("original-bin", "original"),
        ("effective-bin", "effective"),
    ):
        binary = tmp_path / directory / "review-kernel"
        binary.parent.mkdir()
        binary.write_text(f"#!/bin/sh\nprintf '{message}\\n'\n")
        binary.chmod(0o755)
    monkeypatch.setenv("PATH", str(tmp_path / "original-bin") + os.pathsep + os.defpath)
    child_path = "effective-bin" if relative_path else str(tmp_path / "effective-bin")
    log = tmp_path / "execution.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(log),
        "--env",
        f"PATH={child_path}",
        "--validation-target",
        f"{target_dir}/review-kernel",
        "--",
        "review-kernel",
    )
    assert result.returncode == 0, result.stderr
    assert log.read_text() == "effective\n"
    metadata = json.loads(Path(f"{log}.meta.json").read_text())
    assert metadata["executable"]["path"] == str(
        (tmp_path / "effective-bin/review-kernel").resolve()
    )
    assert metadata["validation_target"]["adopted"] is (target_dir == "effective-bin")


def test_launch_failure_does_not_claim_target_adoption(tmp_path: Path):
    executable = tmp_path / "bad-shebang"
    executable.write_text("#!/missing-review-interpreter\n")
    executable.chmod(0o755)
    log = tmp_path / "failed-launch.log"
    result = invoke(
        "--cwd",
        str(tmp_path),
        "--log",
        str(log),
        "--validation-target",
        executable.name,
        "--",
        "./bad-shebang",
    )
    assert result.returncode == 127
    assert target_metadata(log)["adopted"] is False
    assert target_metadata(log)["reason"] == "launch-failed"
