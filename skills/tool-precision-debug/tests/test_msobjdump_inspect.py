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

"""msobjdump_inspect 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_msobjdump_inspect.py -q
"""

from __future__ import annotations

from copy import deepcopy
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "debug" / "msobjdump_inspect.py"
sys.path.insert(0, str(TOOL.parent))

import msobjdump_inspect  # noqa: E402


def fake_tool(path: Path, body: str) -> Path:
    path.write_text(f"#!{sys.executable}\n{body}\n", encoding="utf-8")
    path.chmod(0o700)
    return path


def elf(path: Path, size: int = 16) -> Path:
    path.write_bytes(b"\x7fELF" + b"\x00" * (size - 4))
    return path


def probe_report(path: Path, tool: Path, cann: str = "9.0.0") -> Path:
    payload = {
        "schema_version": "1.1.0",
        "mode": "probe",
        "expected_cann": cann,
        "requirements": {
            "requested": ["msobjdump"],
            "missing": [],
            "satisfied": True,
        },
        "installations": [
            {
                "version": {"value": cann},
                "capabilities": [
                    {
                        "id": "msobjdump",
                        "status": "available",
                        "entrypoint": str(tool.resolve()),
                    }
                ],
            }
        ],
    }
    path.write_text(json.dumps(payload), encoding="utf-8")
    return path


def invoke(*arguments: str) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return subprocess.run(
        [sys.executable, str(TOOL), *arguments],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
        env=environment,
        check=False,
    )


def run_report(
    tool: Path,
    input_path: Path,
    output: Path,
    *arguments: str,
    expected_returncode: int = 0,
    cann: str = "9.0.0",
    with_probe: bool = True,
) -> dict:
    probe_arguments = []
    if with_probe:
        probe = probe_report(output.with_name(f"{output.stem}-probe.json"), tool, cann)
        probe_arguments = ["--probe-report", str(probe)]
    result = invoke(
        "--tool",
        str(tool),
        *probe_arguments,
        "--input",
        str(input_path),
        "--output",
        str(output),
        "--operation",
        "dump",
        "--cann",
        cann,
        *arguments,
    )
    assert result.returncode == expected_returncode, result.stderr
    assert result.stdout.startswith("WROTE status=")
    report = json.loads(output.read_text(encoding="utf-8"))
    assert msobjdump_inspect.validate_report(report) == []
    assert (report["probe_report"] is not None) is with_probe
    assert report["policy"]["requires_probe_binding"] is with_probe
    return report


@pytest.mark.parametrize("with_probe", [False, True])
def test_dump_preserves_90_and_91_structural_fields_without_interpretation(
    tmp_path, with_probe
):
    executable = fake_tool(
        tmp_path / "msobjdump",
        "print('===========================')\n"
        "print('[VERSION]: 1')\n"
        "print('[TYPE COUNT]: 1')\n"
        "print('[ELF FILE 0]: ascend910b_kernel_0_mix.o')\n"
        "print('[KERNEL TYPE]: mix')\n"
        "print('[KERNEL LEN]: 511560')\n"
        "print('[ASCEND META]: None')\n"
        "print('.ascend.meta META INFO')\n"
        "print('DEBUG: debugBufSize=4096, debugOptions=1')\n"
        "print('DYNAMIC_PARAM: dynamicParamMode=0')\n"
        "print('.ascend.meta. [3]: add_custom')\n"
        "print('KERNEL_TYPE: AIV')\n"
        "print('FUNCTION_ENTRY: 1010')",
    )
    source = elf(tmp_path / "kernel.o")
    report = run_report(
        executable, source, tmp_path / "report.json", with_probe=with_probe
    )

    assert report["status"] == "OBSERVATIONS"
    assert report["execution"]["returncode"] == 0
    assert report["execution"]["argv"] == [
        str(executable.resolve()),
        "--dump-elf",
        str(source.resolve()),
    ]
    assert report["input"]["path"] == str(source.resolve())
    assert report["tool"]["path"] == str(executable.resolve())
    assert [item["kind"] for item in report["observations"]] == [
        "container-field",
        "container-field",
        "elf-file",
        "container-field",
        "container-field",
        "container-field",
        "binary-meta-header",
        "metadata-field",
        "metadata-field",
        "function-meta",
        "metadata-field",
        "metadata-field",
    ]
    function = next(
        item for item in report["observations"] if item["kind"] == "function-meta"
    )
    assert function == {
        "observation_id": "MSOBJ-000010",
        "kind": "function-meta",
        "line_number": 11,
        "raw_line": ".ascend.meta. [3]: add_custom",
        "name": ".ascend.meta.",
        "value": "add_custom",
        "index": 3,
    }
    assert "field_semantics" not in report


def test_list_operation_extracts_names_without_writing_elf_files(tmp_path):
    executable = fake_tool(
        tmp_path / "msobjdump",
        "print('ELF file    0: first.o')\nprint('ELF file    1: second.o')",
    )
    source = elf(tmp_path / "container.so")
    output = tmp_path / "report.json"
    probe = probe_report(tmp_path / "probe.json", executable, "9.1.0")
    result = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--output",
        str(output),
        "--operation",
        "list",
        "--cann",
        "9.1.0",
    )
    assert result.returncode == 0, result.stderr
    report = json.loads(output.read_text(encoding="utf-8"))
    assert [item["value"] for item in report["observations"]] == ["first.o", "second.o"]
    assert report["request"]["operation"] == "list"
    assert set(tmp_path.iterdir()) == {executable, source, probe, output}


@pytest.mark.parametrize("with_probe", [False, True])
def test_explicit_error_marker_overrides_zero_tool_exit(tmp_path, with_probe):
    executable = fake_tool(
        tmp_path / "msobjdump",
        "print('[ERROR]: The input file or directory does not exist!!!')\n"
        "print('The kernel meta information cannot be found.')",
    )
    report = run_report(
        executable,
        elf(tmp_path / "kernel.asc.o"),
        tmp_path / "report.json",
        expected_returncode=1,
        with_probe=with_probe,
    )
    assert report["status"] == "TOOL_ERROR"
    assert report["execution"]["returncode"] == 0
    assert report["statistics"]["explicit_error_count"] == 1
    assert [item["level"] for item in report["diagnostics"]] == ["error", "notice"]


def test_warning_or_unrecognized_output_is_honest_no_observations(tmp_path):
    executable = fake_tool(
        tmp_path / "msobjdump",
        "print('[WARNING]: nothing to list in single op elf file')",
    )
    report = run_report(
        executable, elf(tmp_path / "kernel.o"), tmp_path / "report.json"
    )
    assert report["status"] == "NO_OBSERVATIONS"
    assert report["observations"] == []
    assert report["diagnostics"][0]["level"] == "warning"


def test_timeout_returns_bounded_tool_error_report_and_cleans_process(tmp_path):
    executable = fake_tool(tmp_path / "msobjdump", "import time\ntime.sleep(30)")
    report = run_report(
        executable,
        elf(tmp_path / "kernel.o"),
        tmp_path / "report.json",
        "--timeout-seconds",
        "1",
        expected_returncode=1,
    )
    assert report["status"] == "TOOL_ERROR"
    assert report["execution"]["timed_out"] is True
    assert report["execution"]["timeout_cleanup"]["sigterm_sent"] is True
    assert report["execution"]["timeout_cleanup"]["cleanup_complete"] is True


def test_output_and_input_limits_fail_closed(tmp_path):
    noisy = fake_tool(
        tmp_path / "noisy-msobjdump",
        "import os\nos.write(1, b'x' * 1048576)",
    )
    source = elf(tmp_path / "kernel.o")
    report = run_report(
        noisy,
        source,
        tmp_path / "limited.json",
        "--max-output-bytes",
        "1024",
        expected_returncode=1,
    )
    assert report["status"] == "TOOL_ERROR"
    assert report["execution"]["stdout"]["bytes"] <= 1024
    assert report["execution"]["output_limit_exceeded"] is True

    output = tmp_path / "oversized.json"
    probe = probe_report(tmp_path / "oversized-probe.json", noisy)
    oversized = invoke(
        "--tool",
        str(noisy),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--output",
        str(output),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
        "--max-input-bytes",
        "4",
    )
    assert oversized.returncode == 2
    assert "input exceeds byte limit 4" in oversized.stderr
    assert not output.exists()


def test_symlink_input_overwrite_and_extract_mode_are_rejected(tmp_path):
    executable = fake_tool(tmp_path / "msobjdump", "print('[VERSION]: 1')")
    source = elf(tmp_path / "kernel.o")
    alias = tmp_path / "alias.o"
    try:
        alias.symlink_to(source)
    except OSError:
        pytest.skip("symbolic links are unavailable")
    probe = probe_report(tmp_path / "probe.json", executable)

    linked = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(probe),
        "--input",
        str(alias),
        "--output",
        str(tmp_path / "linked.json"),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert linked.returncode == 2
    assert "symbolic-link input is not allowed" in linked.stderr

    overwrite = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--output",
        str(source),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert overwrite.returncode == 2
    assert source.read_bytes().startswith(b"\x7fELF")

    extract = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--output",
        str(tmp_path / "extract.json"),
        "--operation",
        "extract",
        "--cann",
        "9.0.0",
    )
    assert extract.returncode == 2
    assert "invalid choice" in extract.stderr


def test_semantic_gate_rejects_stale_derived_claims(tmp_path):
    executable = fake_tool(tmp_path / "msobjdump", "print('KERNEL_TYPE: AIV')")
    report = run_report(
        executable, elf(tmp_path / "kernel.o"), tmp_path / "report.json"
    )

    wrong_status = deepcopy(report)
    wrong_status["status"] = "NO_OBSERVATIONS"
    assert "status does not match execution and observations" in (
        msobjdump_inspect.validate_report(wrong_status)
    )

    stale = deepcopy(report)
    stale["statistics"]["observation_count"] = 0
    assert "statistics do not match observations and diagnostics" in (
        msobjdump_inspect.validate_report(stale)
    )


def test_probe_binding_rejects_version_requirement_and_entrypoint_mismatches(tmp_path):
    executable = fake_tool(tmp_path / "msobjdump", "print('[VERSION]: 1')")
    other = fake_tool(tmp_path / "other-msobjdump", "print('[VERSION]: 1')")
    source = elf(tmp_path / "kernel.o")

    wrong_version = probe_report(tmp_path / "wrong-version.json", executable, "9.1.0")
    result = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(wrong_version),
        "--input",
        str(source),
        "--output",
        str(tmp_path / "wrong-version-output.json"),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "not bound to the declared CANN version" in result.stderr

    unsatisfied = probe_report(tmp_path / "unsatisfied.json", executable)
    payload = json.loads(unsatisfied.read_text(encoding="utf-8"))
    payload["requirements"]["satisfied"] = False
    unsatisfied.write_text(json.dumps(payload), encoding="utf-8")
    result = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(unsatisfied),
        "--input",
        str(source),
        "--output",
        str(tmp_path / "unsatisfied-output.json"),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "must satisfy an explicit msobjdump requirement" in result.stderr

    not_requested = probe_report(tmp_path / "not-requested.json", executable)
    payload = json.loads(not_requested.read_text(encoding="utf-8"))
    payload["requirements"]["requested"] = []
    not_requested.write_text(json.dumps(payload), encoding="utf-8")
    result = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(not_requested),
        "--input",
        str(source),
        "--output",
        str(tmp_path / "not-requested-output.json"),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "must satisfy an explicit msobjdump requirement" in result.stderr

    mismatched = probe_report(tmp_path / "mismatched.json", other)
    result = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(mismatched),
        "--input",
        str(source),
        "--output",
        str(tmp_path / "mismatched-output.json"),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "does not bind the requested msobjdump executable" in result.stderr


def test_probe_symlink_and_probe_overwrite_are_rejected(tmp_path):
    executable = fake_tool(tmp_path / "msobjdump", "print('[VERSION]: 1')")
    source = elf(tmp_path / "kernel.o")
    probe = probe_report(tmp_path / "probe.json", executable)
    alias = tmp_path / "probe-link.json"
    try:
        alias.symlink_to(probe)
    except OSError:
        pytest.skip("symbolic links are unavailable")

    linked = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(alias),
        "--input",
        str(source),
        "--output",
        str(tmp_path / "linked-output.json"),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert linked.returncode == 2
    assert "symbolic-link probe report is not allowed" in linked.stderr

    original = probe.read_bytes()
    overwrite = invoke(
        "--tool",
        str(executable),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--output",
        str(probe),
        "--operation",
        "dump",
        "--cann",
        "9.0.0",
    )
    assert overwrite.returncode == 2
    assert "output path cannot overwrite" in overwrite.stderr
    assert probe.read_bytes() == original


def test_unlisted_cann_version_uses_the_bound_inspector(tmp_path):
    tool = fake_tool(tmp_path / "msobjdump", "print('[VERSION]: 1')")
    report = run_report(
        tool, elf(tmp_path / "kernel.o"), tmp_path / "report.json", cann="9.2.0"
    )
    assert report["request"]["declared_cann"] == "9.2.0"
    assert report["status"] == "OBSERVATIONS"
