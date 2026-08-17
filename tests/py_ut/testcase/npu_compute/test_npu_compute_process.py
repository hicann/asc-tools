# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest


BUILD_DIR = Path(
    os.environ.get("NPU_COMPUTE_BUILD_DIR", "/tmp/asc_tools_npu_compute_integration")
)
BIN_DIR = Path(
    os.environ.get("NPU_COMPUTE_TEST_BIN_DIR", str(BUILD_DIR / "npu_compute/bin"))
)
CLI = BIN_DIR / "npu-compute"
BASE_COMMAND = [str(CLI), "--section", "Memory"]
HARDWARE_INFO_STATEMENT = (
    "pathlib.Path(os.environ['NPU_COMPUTE_OUTPUT'], 'HardwareInfo.jsonl')"
    ".write_text('{}\\n', encoding='utf-8'); "
)


def run_cli(*program_and_arguments, env=None):
    assert CLI.is_file(), f"npu-compute was not built: {CLI}"
    return subprocess.run(
        [*BASE_COMMAND, *program_and_arguments],
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )


def wait_for_file(path, process, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.is_file() and path.stat().st_size > 0:
            return
        if process.poll() is not None:
            raise AssertionError(f"npu-compute exited early with {process.returncode}")
        time.sleep(0.01)
    raise AssertionError(f"timed out waiting for {path}")


def process_identity_program(identity_path, sleep_seconds):
    code = (
        "import os, pathlib, sys, time; "
        + HARDWARE_INFO_STATEMENT
        + "pathlib.Path(sys.argv[1]).write_text("
        "f'{os.getpid()} {os.getppid()}', encoding='utf-8'); "
        "time.sleep(float(sys.argv[2]))"
    )
    return [sys.executable, "-c", code, str(identity_path), str(sleep_seconds)]


def test_cli_remains_parent_until_app_exits(tmp_path):
    identity_path = tmp_path / "identity.txt"
    process = subprocess.Popen(
        [*BASE_COMMAND, *process_identity_program(identity_path, 0.3)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        wait_for_file(identity_path, process)
        app_pid, app_parent_pid = map(int, identity_path.read_text().split())

        assert process.poll() is None
        assert app_pid != process.pid
        assert app_parent_pid == process.pid
        assert process.wait(timeout=5) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)


def test_app_exit_status_is_preserved():
    result = run_cli("/bin/sh", "-c", "exit 7")

    assert result.returncode == 7
    assert "APP exited with status 7" in result.stderr


def test_program_not_found_returns_127():
    result = run_cli("/definitely/not/a/real/npu-compute-program")

    assert result.returncode == 127
    assert "execvpe" in result.stderr
    assert "No such file or directory" in result.stderr


def test_program_not_executable_returns_126(tmp_path):
    program = tmp_path / "not-executable"
    program.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    program.chmod(0o644)

    result = run_cli(str(program))

    assert result.returncode == 126
    assert "execvpe" in result.stderr
    assert "Permission denied" in result.stderr


def test_child_environment_is_overridden_without_changing_parent(monkeypatch):
    monkeypatch.setenv("NPU_COMPUTE_SECTIONS", "parent-value")
    monkeypatch.setenv("NPU_COMPUTE_REPLAY_MODE", "parent-value")
    code = (
        "import json, os, pathlib; " + HARDWARE_INFO_STATEMENT + "print(json.dumps({"
        "'sections': os.environ['NPU_COMPUTE_SECTIONS'], "
        "'replay': os.environ['NPU_COMPUTE_REPLAY_MODE'], "
        "'injection': os.environ['ACL_API_INJECTION']}))"
    )

    result = run_cli(sys.executable, "-c", code, env=os.environ.copy())

    assert result.returncode == 0
    child_environment = json.loads(result.stdout)
    assert child_environment["sections"] == "Memory"
    assert child_environment["replay"] == "kernel"
    assert Path(child_environment["injection"]).is_absolute()
    assert os.environ["NPU_COMPUTE_SECTIONS"] == "parent-value"
    assert os.environ["NPU_COMPUTE_REPLAY_MODE"] == "parent-value"


def test_program_is_found_through_path(tmp_path):
    program = tmp_path / "npu-compute-path-probe"
    program.write_text(
        "#!/bin/sh\nprintf '{}\\n' > \"$NPU_COMPUTE_OUTPUT/HardwareInfo.jsonl\"\n",
        encoding="utf-8",
    )
    program.chmod(0o755)
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join((str(tmp_path), environment.get("PATH", "")))

    result = run_cli(program.name, env=environment)

    assert result.returncode == 0


def test_sigterm_is_forwarded_and_child_is_reaped(tmp_path):
    identity_path = tmp_path / "signal-identity.txt"
    process = subprocess.Popen(
        [*BASE_COMMAND, *process_identity_program(identity_path, 30)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    app_pid = None
    try:
        wait_for_file(identity_path, process)
        app_pid = int(identity_path.read_text().split()[0])
        process.send_signal(signal.SIGTERM)

        stdout, stderr = process.communicate(timeout=5)
        assert stdout == ""
        assert process.returncode == 128 + signal.SIGTERM
        assert "APP terminated by signal 15" in stderr
        with pytest.raises(ProcessLookupError):
            os.kill(app_pid, 0)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if app_pid is not None:
            try:
                os.kill(app_pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
