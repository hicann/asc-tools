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

"""kernel_debug_data_parse 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_kernel_debug_data_parse.py -q
"""

from __future__ import annotations

from copy import deepcopy
import errno
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import Mock, sentinel

import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "debug" / "kernel_debug_data_parse.py"
sys.path.insert(0, str(TOOL.parent))

import kernel_debug_data_parse  # noqa: E402
import msobjdump_inspect  # noqa: E402


def fake_tool(path: Path, body: str) -> Path:
    path.write_text(f"#!{sys.executable}\n{body}\n", encoding="utf-8")
    path.chmod(0o700)
    return path


def probe_report(path: Path, tool: Path, cann: str = "9.0.0") -> Path:
    payload = {
        "schema_version": "1.1.0",
        "mode": "probe",
        "expected_cann": cann,
        "requirements": {
            "requested": ["show-kernel-debug-data"],
            "missing": [],
            "satisfied": True,
        },
        "installations": [
            {
                "version": {"value": cann},
                "capabilities": [
                    {
                        "id": "show-kernel-debug-data",
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
    source: Path,
    output: Path,
    *arguments: str,
    cann: str = "9.0.0",
    expected_returncode: int = 0,
    with_probe: bool = True,
) -> dict:
    probe_arguments = []
    if with_probe:
        probe = probe_report(output.with_name(output.stem + "-probe.json"), tool, cann)
        probe_arguments = ["--probe-report", str(probe)]
    artifacts = output.with_name(output.stem + "-artifacts")
    result = invoke(
        "--tool",
        str(tool),
        *probe_arguments,
        "--input",
        str(source),
        "--artifact-dir",
        str(artifacts),
        "--output",
        str(output),
        "--cann",
        cann,
        *arguments,
    )
    assert result.returncode == expected_returncode, result.stderr
    assert result.stdout.startswith("WROTE status=")
    report = json.loads(output.read_text(encoding="utf-8"))
    assert kernel_debug_data_parse.validate_report(report) == []
    assert (report["probe_report"] is not None) is with_probe
    assert report["policy"]["requires_probe_binding"] is with_probe
    assert Path(report["artifact_directory"]) == artifacts.resolve()
    return report


@pytest.mark.parametrize("with_probe", [False, True])
def test_inventories_outputs_and_console_boundaries_without_interpretation(
    tmp_path, with_probe
):
    parser = fake_tool(
        tmp_path / "show_kernel_debug_data",
        "from pathlib import Path\n"
        "import sys\n"
        "source, output = map(Path, sys.argv[1:])\n"
        "target = output / 'PARSER_1' / 'dump_data' / '0'\n"
        "target.mkdir(parents=True)\n"
        "(target / 'asc_kernel_data_aiv_0_index_2_loop_3.bin').write_bytes(b'raw')\n"
        "(target / 'asc_kernel_data_aiv_0_index_2_loop_3.txt').write_text('1.0\\n')\n"
        "(output / 'PARSER_1' / 'dump_data' / 'index_dtype.json').write_text('{\"2\": \"float\"}')\n"
        "print('================ block.0 begin ==============')\n"
        "print('arbitrary printf payload')\n"
        "print('================ block.0 end ================')",
    )
    source = tmp_path / "dump.bin"
    source.write_bytes(b"dump")
    report = run_report(parser, source, tmp_path / "report.json", with_probe=with_probe)

    assert report["status"] == "OBSERVATIONS"
    assert [item["kind"] for item in report["artifacts"]] == [
        "tensor-binary",
        "tensor-text",
        "index-dtype-map",
    ]
    assert [item["boundary"] for item in report["observations"]] == ["begin", "end"]
    assert "arbitrary printf payload" in report["execution"]["stdout"]["text"]


def test_directory_input_is_recursively_copied_to_private_ascii_workspace(tmp_path):
    parser = fake_tool(
        tmp_path / "show_kernel_debug_data",
        "from pathlib import Path\n"
        "import sys\n"
        "source, output = map(Path, sys.argv[1:])\n"
        "assert '输入 路径' not in str(source)\n"
        "assert sorted(p.relative_to(source).as_posix() for p in source.rglob('*.bin')) == ['a.bin', 'nested/b.bin']",
    )
    source = tmp_path / "输入 路径"
    (source / "nested").mkdir(parents=True)
    (source / "a.bin").write_bytes(b"a")
    (source / "nested" / "b.bin").write_bytes(b"b")
    (source / "ignored.txt").write_text("ignored")
    report = run_report(parser, source, tmp_path / "report.json")

    assert report["status"] == "NO_OBSERVATIONS"
    assert [item["relative_path"] for item in report["input"]["files"]] == [
        "a.bin",
        "nested/b.bin",
    ]
    assert report["execution"]["argv"][1:] == [
        "<private-input-copy>",
        "<private-output-dir>",
    ]


@pytest.mark.parametrize("with_probe", [False, True])
def test_90_zero_exit_exception_marker_is_tool_error_and_partial_outputs_survive(
    tmp_path, with_probe
):
    parser = fake_tool(
        tmp_path / "show_kernel_debug_data",
        "from pathlib import Path\n"
        "import sys\n"
        "output = Path(sys.argv[2])\n"
        "(output / 'dump_data').mkdir()\n"
        "(output / 'dump_data' / 'partial.txt').write_text('partial')\n"
        "print('parse dump workspace bin occur exception.')\n"
        "print('Traceback (most recent call last):', file=sys.stderr)",
    )
    source = tmp_path / "dump.bin"
    source.write_bytes(b"bad")
    report = run_report(
        parser,
        source,
        tmp_path / "report.json",
        expected_returncode=1,
        with_probe=with_probe,
    )

    assert report["status"] == "TOOL_ERROR"
    assert report["execution"]["returncode"] == 0
    assert report["statistics"]["explicit_error_count"] == 2
    assert (Path(report["artifact_directory"]) / "dump_data" / "partial.txt").is_file()


def test_91_nonzero_exit_and_empty_success_have_distinct_statuses(tmp_path):
    failing = fake_tool(tmp_path / "failing", "raise SystemExit(255)")
    source = tmp_path / "dump.bin"
    source.write_bytes(b"bad")
    failed = run_report(
        failing, source, tmp_path / "failed.json", cann="9.1.0", expected_returncode=1
    )
    assert failed["status"] == "TOOL_ERROR"
    assert failed["execution"]["returncode"] == 255

    empty = fake_tool(tmp_path / "empty", "raise SystemExit(0)")
    clean = run_report(empty, source, tmp_path / "empty.json", cann="9.1.0")
    assert clean["status"] == "NO_OBSERVATIONS"


def test_timeout_and_stream_limit_return_bounded_tool_error_reports(tmp_path):
    source = tmp_path / "dump.bin"
    source.write_bytes(b"dump")
    sleeping = fake_tool(tmp_path / "sleeping", "import time\ntime.sleep(30)")
    timed = run_report(
        sleeping,
        source,
        tmp_path / "timed.json",
        "--timeout-seconds",
        "1",
        expected_returncode=1,
    )
    assert timed["execution"]["timed_out"] is True
    assert timed["execution"]["timeout_cleanup"]["cleanup_complete"] is True

    noisy = fake_tool(tmp_path / "noisy", "import os\nos.write(1, b'x' * 1048576)")
    limited = run_report(
        noisy,
        source,
        tmp_path / "limited.json",
        "--max-stream-bytes",
        "1024",
        expected_returncode=1,
    )
    assert limited["execution"]["output_limit_exceeded"] is True
    assert limited["execution"]["stdout"]["bytes"] > 1024
    assert len(limited["execution"]["stdout"]["text"].encode()) <= 1024
    assert limited["execution"]["stdout"]["truncated"] is True

    artifact_writer = fake_tool(
        tmp_path / "artifact-writer",
        "from pathlib import Path\nimport sys\n(Path(sys.argv[2]) / 'tensor.bin').write_bytes(b'x' * 2048)",
    )
    independent = run_report(
        artifact_writer,
        source,
        tmp_path / "independent.json",
        "--max-stream-bytes",
        "1024",
        "--max-file-bytes",
        "4096",
    )
    assert independent["status"] == "OBSERVATIONS"
    assert independent["artifacts"][0]["bytes"] == 2048


def test_input_output_limits_symlinks_and_overwrite_are_rejected(tmp_path):
    parser = fake_tool(tmp_path / "parser", "raise SystemExit(0)")
    source = tmp_path / "dump.bin"
    source.write_bytes(b"12345")
    probe = probe_report(tmp_path / "probe.json", parser)
    result = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--artifact-dir",
        str(tmp_path / "artifacts"),
        "--output",
        str(tmp_path / "report.json"),
        "--cann",
        "9.0.0",
        "--max-file-bytes",
        "4",
    )
    assert result.returncode == 2
    assert "exceeds byte limit 4" in result.stderr

    alias = tmp_path / "alias.bin"
    try:
        alias.symlink_to(source)
    except OSError:
        pytest.skip("symbolic links are unavailable")
    linked = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(probe),
        "--input",
        str(alias),
        "--artifact-dir",
        str(tmp_path / "linked-artifacts"),
        "--output",
        str(tmp_path / "linked.json"),
        "--cann",
        "9.0.0",
    )
    assert linked.returncode == 2
    assert "symbolic-link input is not allowed" in linked.stderr

    overwrite = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(probe),
        "--input",
        str(source),
        "--artifact-dir",
        str(tmp_path / "overwrite-artifacts"),
        "--output",
        str(source),
        "--cann",
        "9.0.0",
    )
    assert overwrite.returncode == 2
    assert source.read_bytes() == b"12345"

    directory = tmp_path / "input-directory"
    directory.mkdir()
    (directory / "dump.bin").write_bytes(b"dump")
    overlap = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(probe),
        "--input",
        str(directory),
        "--artifact-dir",
        str(directory / "artifacts"),
        "--output",
        str(tmp_path / "overlap.json"),
        "--cann",
        "9.0.0",
    )
    assert overlap.returncode == 2
    assert "must not be inside the input directory" in overlap.stderr
    assert not (directory / "artifacts").exists()


def test_probe_binding_rejects_version_requirement_and_entrypoint_mismatches(tmp_path):
    parser = fake_tool(tmp_path / "parser", "raise SystemExit(0)")
    other = fake_tool(tmp_path / "other", "raise SystemExit(0)")
    source = tmp_path / "dump.bin"
    source.write_bytes(b"dump")
    wrong_version = probe_report(tmp_path / "wrong-version.json", parser, "9.1.0")
    result = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(wrong_version),
        "--input",
        str(source),
        "--artifact-dir",
        str(tmp_path / "a"),
        "--output",
        str(tmp_path / "r.json"),
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "declared CANN version" in result.stderr

    wrong_entry = probe_report(tmp_path / "wrong-entry.json", other)
    result = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(wrong_entry),
        "--input",
        str(source),
        "--artifact-dir",
        str(tmp_path / "b"),
        "--output",
        str(tmp_path / "s.json"),
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "does not bind" in result.stderr

    malformed = probe_report(tmp_path / "malformed-requested.json", parser)
    payload = json.loads(malformed.read_text())
    payload["requirements"]["requested"] = "show-kernel-debug-data"
    malformed.write_text(json.dumps(payload))
    result = invoke(
        "--tool",
        str(parser),
        "--probe-report",
        str(malformed),
        "--input",
        str(source),
        "--artifact-dir",
        str(tmp_path / "c"),
        "--output",
        str(tmp_path / "t.json"),
        "--cann",
        "9.0.0",
    )
    assert result.returncode == 2
    assert "must satisfy an explicit" in result.stderr


def test_semantic_validation_rejects_stale_derived_claims(tmp_path):
    parser = fake_tool(
        tmp_path / "parser",
        "from pathlib import Path\nimport sys\n(Path(sys.argv[2]) / 'x.txt').write_text('x')",
    )
    source = tmp_path / "dump.bin"
    source.write_bytes(b"dump")
    report = run_report(parser, source, tmp_path / "report.json")

    stale = deepcopy(report)
    stale["statistics"]["artifact_count"] = 0
    assert "statistics do not match artifacts, observations and diagnostics" in (
        kernel_debug_data_parse.validate_report(stale)
    )

    wrong_status = deepcopy(report)
    wrong_status["status"] = "NO_OBSERVATIONS"
    assert "status does not match execution and observations" in (
        kernel_debug_data_parse.validate_report(wrong_status)
    )


def test_unlisted_cann_version_uses_the_bound_parser(tmp_path):
    parser = fake_tool(
        tmp_path / "show_kernel_debug_data",
        "print('================ block.0 begin ==============')\n"
        "print('================ block.0 end ================')",
    )
    source = tmp_path / "dump.bin"
    source.write_bytes(b"dump")
    report = run_report(parser, source, tmp_path / "report.json", cann="9.2.0")
    assert report["request"]["declared_cann"] == "9.2.0"
    assert [item["boundary"] for item in report["observations"]] == ["begin", "end"]


@pytest.mark.parametrize("module", [kernel_debug_data_parse, msobjdump_inspect])
@pytest.mark.parametrize("has_exited", [False, True])
def test_process_cleanup_distinguishes_exit_race_from_live_permission_error(
    monkeypatch, module, has_exited
):
    process = Mock(pid=sentinel.owned_child, returncode=None)
    process.poll.side_effect = lambda: process.returncode
    process.wait.return_value = 0

    def denied(group, _signal):
        assert group is process.pid
        if has_exited:
            process.returncode = 0
        raise PermissionError(errno.EPERM, "simulated signal permission error")

    monkeypatch.setattr(module.os, "killpg", denied)
    if has_exited:
        result = module.stop_process_group(process)
        assert result["cleanup_complete"] is True
        assert result["sigterm_sent"] is False
        assert result["sigkill_sent"] is False
    else:
        with pytest.raises(PermissionError):
            module.stop_process_group(process)
        process.wait.assert_not_called()
