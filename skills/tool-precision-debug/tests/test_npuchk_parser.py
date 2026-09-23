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

"""npuchk_parser 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_npuchk_parser.py -q
"""

from __future__ import annotations

from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "debug" / "npuchk_parser.py"
sys.path.insert(0, str(TOOL.parent))

import npuchk_parser  # noqa: E402


DOCUMENTED_CODES = {
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


def run_report(input_path: Path, output: Path, *arguments: str) -> dict:
    result = invoke("--input", str(input_path), "--output", str(output), *arguments)
    assert result.returncode == 0, result.stderr
    assert result.stdout.startswith("WROTE status=")
    report = json.loads(output.read_text(encoding="utf-8"))
    assert npuchk_parser.validate_report(report) == []
    return report


def test_official_format_preserves_multicore_raw_observations(tmp_path):
    # Format is derived from asc-tools docs/02_npu_check.md and
    # npuchk/ascendc_npuchk_report.py at the frozen 9.0.0/9.1.0 commits.
    core0 = tmp_path / "add_custom_0_0_vec_npuchk.log"
    core0.write_text(
        "### vadd((__ubuf__ half*)0x1000);\n"
        "[V] [ErrorRead3] on read 0x1000 0x80B\n"
        "trace: # BackTrace #\n"
        "  kernel.o(+0x10)\n"
        "  libhelper.so(+0x20)\n"
        "END\n",
        encoding="utf-8",
    )
    core1 = tmp_path / "add_custom_1_0_aiv_npuchk.log"
    core1.write_text(
        "### data_copy();\n"
        "[V] [ErrorBuffer2] invalid tensor state\n"
        "[V] [ErrorFuture9] future tool observation\n",
        encoding="utf-8",
    )

    report = run_report(tmp_path, tmp_path / "report.json", "--cann", "9.1.0")

    assert report["status"] == "OBSERVATIONS"
    assert report["input"]["declared_cann"] == "9.1.0"
    assert [item["path"] for item in report["files"]] == [core0.name, core1.name]
    assert [item["core_id"] for item in report["files"]] == [0, 1]
    assert (
        report["files"][0]["sha256"] == hashlib.sha256(core0.read_bytes()).hexdigest()
    )
    assert [item["error_code"] for item in report["observations"]] == [
        "ErrorRead3",
        "ErrorBuffer2",
        "ErrorFuture9",
    ]
    first = report["observations"][0]
    assert first["observation_id"] == "NPUCHK-000001"
    assert first["intrinsic"] == {
        "line_number": 1,
        "raw_line": "### vadd((__ubuf__ half*)0x1000);",
    }
    assert first["backtrace"] == [
        {"line_number": 4, "raw_line": "  kernel.o(+0x10)"},
        {"line_number": 5, "raw_line": "  libhelper.so(+0x20)"},
    ]
    assert report["statistics"] == {
        "file_count": 2,
        "files_with_observations": 2,
        "total_bytes": len(core0.read_bytes()) + len(core1.read_bytes()),
        "observation_count": 3,
        "documented_observation_count": 2,
        "unknown_observation_count": 1,
        "error_code_counts": [
            {"error_code": "ErrorBuffer2", "count": 1},
            {"error_code": "ErrorFuture9", "count": 1},
            {"error_code": "ErrorRead3", "count": 1},
        ],
        "affected_core_ids": [0, 1],
        "unresolved_core_file_count": 0,
    }
    assert report["completeness"]["status"] == "unknown"


def test_all_documented_error_codes_are_classified_without_interpretation(tmp_path):
    log = tmp_path / "all_codes_3_0_vec_npuchk.log"
    log.write_text(
        "\n".join(f"[V] [{code}] raw" for code in sorted(DOCUMENTED_CODES)) + "\n",
        encoding="utf-8",
    )
    report = run_report(log, tmp_path / "report.json")
    assert {item["error_code"] for item in report["observations"]} == DOCUMENTED_CODES
    assert {item["code_status"] for item in report["observations"]} == {"documented"}
    assert report["statistics"]["documented_observation_count"] == 19
    assert report["statistics"]["unknown_observation_count"] == 0
    assert all(
        "severity" not in item and "route" not in item
        for item in report["observations"]
    )


def test_no_observations_and_decode_replacement_do_not_claim_clean_kernel(tmp_path):
    log = tmp_path / "renamed.log"
    log.write_bytes(b"normal execution\ninvalid:\xff\n")
    report = run_report(log, tmp_path / "report.json")
    assert report["status"] == "NO_OBSERVATIONS"
    assert report["observations"] == []
    assert report["files"][0]["decode_errors"] is True
    assert report["files"][0]["filename_contract"] is False
    assert report["files"][0]["core_id"] is None
    assert report["completeness"]["status"] == "unknown"


def test_directory_recursion_is_explicit(tmp_path):
    nested = tmp_path / "nested"
    nested.mkdir()
    (nested / "op_2_0_vec_npuchk.log").write_text(
        "[V] [ErrorSync1] raw\n", encoding="utf-8"
    )
    output = tmp_path / "report.json"
    shallow = invoke("--input", str(tmp_path), "--output", str(output))
    assert shallow.returncode == 2
    assert "no *_npuchk.log files found" in shallow.stderr
    assert not output.exists()

    report = run_report(tmp_path, output, "--recursive")
    assert report["files"][0]["path"] == "nested/op_2_0_vec_npuchk.log"
    assert report["input"]["recursive"] is True


def test_file_count_and_byte_limits_fail_without_partial_output(tmp_path):
    first = tmp_path / "op_0_0_vec_npuchk.log"
    second = tmp_path / "op_1_0_vec_npuchk.log"
    first.write_text("[ErrorRead1]\n", encoding="utf-8")
    second.write_text("[ErrorRead2]\n", encoding="utf-8")

    output = tmp_path / "report.json"
    too_many = invoke(
        "--input", str(tmp_path), "--output", str(output), "--max-files", "1"
    )
    assert too_many.returncode == 2
    assert "exceeds --max-files=1" in too_many.stderr
    assert not output.exists()

    too_large = invoke(
        "--input",
        str(first),
        "--output",
        str(output),
        "--max-file-bytes",
        "4",
        "--max-total-bytes",
        "8",
    )
    assert too_large.returncode == 2
    assert "exceeds --max-file-bytes=4" in too_large.stderr
    assert not output.exists()

    total = invoke(
        "--input",
        str(tmp_path),
        "--output",
        str(output),
        "--max-file-bytes",
        "20",
        "--max-total-bytes",
        "20",
    )
    assert total.returncode == 2
    assert "exceed --max-total-bytes=20" in total.stderr
    assert not output.exists()


def test_symlink_logs_and_input_overwrite_are_rejected(tmp_path):
    log = tmp_path / "op_0_0_vec_npuchk.log"
    log.write_text("[ErrorRead1]\n", encoding="utf-8")
    alias = tmp_path / "alias_1_0_vec_npuchk.log"
    try:
        alias.symlink_to(log)
    except OSError:
        pytest.skip("symbolic links are unavailable")

    output = tmp_path / "report.json"
    linked = invoke("--input", str(tmp_path), "--output", str(output))
    assert linked.returncode == 2
    assert "symbolic-link log is not allowed" in linked.stderr
    assert not output.exists()

    alias.unlink()
    original = log.read_bytes()
    overwrite = invoke("--input", str(log), "--output", str(log))
    assert overwrite.returncode == 2
    assert "cannot overwrite an input log" in overwrite.stderr
    assert log.read_bytes() == original


def test_malformed_cli_and_empty_directory_return_contract_error(tmp_path):
    output = tmp_path / "report.json"
    empty = invoke("--input", str(tmp_path), "--output", str(output))
    assert empty.returncode == 2
    assert not output.exists()

    invalid_limit = invoke(
        "--input", str(tmp_path), "--output", str(output), "--max-files", "0"
    )
    assert invalid_limit.returncode == 2
    assert "--max-files must be between" in invalid_limit.stderr
    assert not output.exists()


def test_semantic_gate_rejects_stale_derived_fields(tmp_path):
    log = tmp_path / "op_0_0_vec_npuchk.log"
    log.write_text("[V] [ErrorRead3] raw\n", encoding="utf-8")
    report = run_report(log, tmp_path / "report.json")

    wrong_status = deepcopy(report)
    wrong_status["status"] = "NO_OBSERVATIONS"
    assert any(
        "report status does not match" in error
        for error in npuchk_parser.validate_report(wrong_status)
    )

    stale = deepcopy(report)
    stale["statistics"]["observation_count"] = 0
    stale["observations"][0]["code_status"] = "unknown"
    errors = npuchk_parser.validate_report(stale)
    assert any("code status is stale" in error for error in errors)
    assert "statistics do not match files and observations" in errors


def test_unlisted_cann_version_is_recorded_without_blocking_log_parsing(tmp_path):
    source = tmp_path / "future_npuchk.log"
    source.write_text("[V] [ErrorRead1] observed\n", encoding="utf-8")
    report = run_report(source, tmp_path / "report.json", "--cann", "9.2.0")
    assert report["input"]["declared_cann"] == "9.2.0"
    assert report["observations"][0]["error_code"] == "ErrorRead1"
