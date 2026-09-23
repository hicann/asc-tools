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

"""repeatability_analyzer 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_repeatability_analyzer.py -q
"""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "analysis" / "repeatability_analyzer.py"


@pytest.fixture
def canary(tmp_path: Path) -> dict[str, Path]:
    generator = tmp_path / "generate.py"
    generator.write_text(
        """from pathlib import Path
import sys
import numpy as np

mode, output, state_path = sys.argv[1:]
state = Path(state_path)
iteration = int(state.read_text()) + 1 if state.exists() else 1
state.write_text(str(iteration))
if mode == 'stable':
    values = [1.0, 0.0]
elif mode == 'position':
    values = [1.0, 0.0] if iteration % 2 else [0.0, 1.0]
elif mode == 'amplitude':
    values = [float(iteration), 0.0]
elif mode == 'amplitude-positions':
    values = [1.0, 2.0] if iteration % 2 else [2.0, 1.0]
elif mode == 'intermittent':
    values = [0.0, 0.0] if iteration % 2 else [1.0, 0.0]
else:
    raise SystemExit(2)
np.save(output, np.array(values, dtype=np.float32))
""",
        encoding="utf-8",
    )
    golden = tmp_path / "golden.npy"
    np.save(golden, np.zeros(2, dtype=np.float32))
    fixed_input = tmp_path / "input.bin"
    fixed_input.write_bytes(b"fixed-input")
    return {"generator": generator, "golden": golden, "input": fixed_input}


def run_canary(tmp_path: Path, canary: dict[str, Path], mode: str) -> dict:
    actual = tmp_path / f"{mode}-actual.npy"
    state = tmp_path / f"{mode}-state.txt"
    output = tmp_path / f"{mode}-report.json"
    result = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--runs",
            "4",
            "--work-dir",
            str(tmp_path / f"{mode}-runs"),
            "--output",
            str(output),
            "--input",
            str(canary["input"]),
            "--actual",
            str(actual),
            "--golden",
            str(canary["golden"]),
            "--rtol",
            "0",
            "--atol",
            "0",
            "--",
            sys.executable,
            str(canary["generator"]),
            mode,
            str(actual),
            str(state),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    report = json.loads(output.read_text(encoding="utf-8"))
    assert report["classification"] in {
        "STABLE_MISMATCH",
        "POSITION_DRIFT",
        "AMPLITUDE_DRIFT",
        "INTERMITTENT_FAILURE",
        "NO_FAILURE_OBSERVED",
        "INCONCLUSIVE",
    }
    assert len(report["runs"]) == 4
    return report


@pytest.mark.parametrize(
    ("mode", "classification"),
    [
        ("stable", "STABLE_MISMATCH"),
        ("position", "POSITION_DRIFT"),
        ("amplitude", "AMPLITUDE_DRIFT"),
        ("amplitude-positions", "AMPLITUDE_DRIFT"),
        ("intermittent", "INTERMITTENT_FAILURE"),
    ],
)
def test_failure_distribution_canaries(
    tmp_path: Path, canary: dict[str, Path], mode: str, classification: str
):
    report = run_canary(tmp_path, canary, mode)
    assert report["classification"] == classification
    assert report["statistics"]["valid_runs"] == 4


def test_fixed_input_mutation_is_rejected(tmp_path: Path, canary: dict[str, Path]):
    mutator = tmp_path / "mutate.py"
    mutator.write_text(
        "from pathlib import Path\nimport sys\nimport numpy as np\n"
        "Path(sys.argv[2]).write_bytes(b'changed')\n"
        "np.save(sys.argv[1], np.ones(2, dtype=np.float32))\n",
        encoding="utf-8",
    )
    actual = tmp_path / "actual.npy"
    result = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--runs",
            "2",
            "--work-dir",
            str(tmp_path / "runs"),
            "--output",
            str(tmp_path / "report.json"),
            "--input",
            str(canary["input"]),
            "--actual",
            str(actual),
            "--golden",
            str(canary["golden"]),
            "--",
            sys.executable,
            str(mutator),
            str(actual),
            str(canary["input"]),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 2
    assert "fixed input or golden changed" in result.stderr


def external_run(
    tmp_path, mode, *, exit_code=0, declared=(), actual_values=None, golden_values=None
):
    generator = tmp_path / "external.py"
    generator.write_text("""import json, os, signal, sys, time
from pathlib import Path
mode, report_path, state_path, exit_code = sys.argv[1:]
state = Path(state_path)
iteration = int(state.read_text()) + 1 if state.exists() else 1
state.write_text(str(iteration))
payload = {'is_pass': mode == 'pass' or (mode == 'alternating' and iteration % 2 == 0),
           'project_specific': {'version': 'test-rule'}}
if mode == 'invalid': payload['is_pass'] = 'false'
if mode == 'conflict': payload['status'] = 'PASS'
if mode != 'stale': Path(report_path).write_text(json.dumps(payload))
if mode == 'signal': os.kill(os.getpid(), signal.SIGTERM)
if mode == 'timeout': time.sleep(1)
raise SystemExit(int(exit_code))
""")
    report = tmp_path / "evaluation.json"
    if mode == "stale":
        report.write_text(json.dumps({"is_pass": True}))
    output = tmp_path / "summary.json"
    args = [
        sys.executable,
        str(TOOL),
        "--runs",
        "2",
        "--work-dir",
        str(tmp_path / "runs"),
        "--output",
        str(output),
        "--evaluation-report",
        str(report),
    ]
    if mode == "timeout":
        args += ["--timeout", "0.1"]
    for code in declared:
        args += ["--mismatch-exit-code", str(code)]
    if actual_values is not None:
        # Deliberately different from the selected evaluator's verdict.
        # Rewrite the actual each run so freshness is independently established.
        actual = tmp_path / "actual.npy"
        golden = tmp_path / "golden.npy"
        np.save(golden, np.array(golden_values, dtype=np.float32))
        original = generator.read_text()
        prefix = f"import numpy as np\nnp.save({str(actual)!r}, np.array({actual_values!r}, dtype=np.float32))\n"
        generator.write_text(prefix + original)
        args += [
            "--actual",
            str(actual),
            "--golden",
            str(golden),
            "--rtol",
            "0",
            "--atol",
            "0",
        ]
    args += [
        "--",
        sys.executable,
        str(generator),
        mode,
        str(report),
        str(tmp_path / "state"),
        str(exit_code),
    ]
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    assert output.exists(), result.stderr
    return result, json.loads(output.read_text())


@pytest.mark.parametrize(
    "mode,classification",
    [
        ("pass", "NO_FAILURE_OBSERVED"),
        ("fail", "ALL_RUNS_FAILED"),
        ("alternating", "INTERMITTENT_FAILURE"),
    ],
)
def test_external_verdict_only_classification(tmp_path, mode, classification):
    result, report = external_run(tmp_path, mode)
    assert result.returncode == 0, result.stderr
    assert report["classification"] == classification
    assert report["statistics"]["valid_runs"] == 2
    archived = json.loads(Path(report["runs"][0]["comparison_report"]).read_text())
    assert archived["project_specific"] == {"version": "test-rule"}


@pytest.mark.parametrize(
    "mode,actual,golden,classification",
    [
        ("pass", [1.005], [1.0], "NO_FAILURE_OBSERVED"),
        ("fail", [1.0], [1.0], "ALL_RUNS_FAILED"),
    ],
)
def test_external_result_is_not_overridden_by_auxiliary_comparison(
    tmp_path, mode, actual, golden, classification
):
    result, report = external_run(
        tmp_path, mode, actual_values=actual, golden_values=golden
    )
    assert result.returncode == 0, result.stderr
    assert report["classification"] == classification
    assert report["runs"][0]["actual_snapshot"] is not None


@pytest.mark.parametrize(
    "mode,code,declared,valid",
    [
        ("fail", 1, (1,), True),
        ("fail", 1, (), False),
        ("pass", 1, (1,), False),
        ("stale", 0, (), False),
        ("invalid", 1, (1,), False),
        ("conflict", 0, (), False),
        ("signal", 0, (143,), False),
        ("timeout", 0, (124,), False),
    ],
)
def test_external_execution_failures(tmp_path, mode, code, declared, valid):
    result, report = external_run(tmp_path, mode, exit_code=code, declared=declared)
    assert result.returncode == (0 if valid else 1), result.stderr
    assert report["classification"] == ("ALL_RUNS_FAILED" if valid else "INCONCLUSIVE")
    assert report["statistics"]["valid_runs"] == (2 if valid else 0)
