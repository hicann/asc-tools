# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import os
import subprocess
import sys
from pathlib import Path

import pytest


BUILD_DIR = Path(
    os.environ.get("NPU_COMPUTE_BUILD_DIR", "/tmp/asc_tools_npu_compute_integration")
)
BIN_DIR = Path(
    os.environ.get("NPU_COMPUTE_TEST_BIN_DIR", str(BUILD_DIR / "npu_compute/bin"))
)
CLI = BIN_DIR / "npu-compute"
SECTIONS = [
    "PipeUtilization",
    "Memory",
    "MemoryL0",
    "MemoryUB",
    "L2Cache",
]
HARDWARE_INFO_PROLOGUE = (
    "import os, pathlib; "
    "pathlib.Path(os.environ['NPU_COMPUTE_OUTPUT'], 'HardwareInfo.jsonl')"
    ".write_text('{}\\n', encoding='utf-8'); "
)


def run_cli(*arguments):
    assert CLI.is_file(), f"npu-compute was not built: {CLI}"
    return subprocess.run(
        [str(CLI), *arguments],
        text=True,
        capture_output=True,
        check=False,
    )


def test_help_lists_only_the_public_command_line_options():
    result = run_cli("--help")

    assert result.returncode == 0
    assert "npu-compute [options] [program] [program-arguments]" in result.stdout
    for option in (
        "-h, --help",
        "--section",
        "--list-sections",
        "--replay-mode",
        "-i, --import",
        "-o, --export",
    ):
        assert option in result.stdout
    for obsolete_option in ("--target", "--session", "--metric", "--dry-run"):
        assert obsolete_option not in result.stdout


def test_list_sections_outputs_only_ids_in_fixed_order():
    result = run_cli("--list-sections")

    assert result.returncode == 0
    assert result.stdout.splitlines() == SECTIONS
    assert result.stderr == ""


@pytest.mark.parametrize("section", SECTIONS)
def test_each_supported_section_is_accepted(section):
    code = HARDWARE_INFO_PROLOGUE + (
        "print('NPU_COMPUTE_SECTIONS=' + os.environ['NPU_COMPUTE_SECTIONS'])"
    )
    result = run_cli("--section", section, sys.executable, "-c", code)

    assert result.returncode == 0
    environment = dict(
        line.split("=", 1) for line in result.stdout.splitlines() if "=" in line
    )
    assert environment["NPU_COMPUTE_SECTIONS"] == section


@pytest.mark.parametrize(
    "removed_section", ("ArithmeticUtilization", "ResourceConflictRatio")
)
def test_sections_without_csv_writer_are_rejected(removed_section):
    result = run_cli("--section", removed_section, "/bin/true")

    assert result.returncode == 2
    assert f"unknown section: {removed_section}" in result.stderr


@pytest.mark.parametrize("abbreviation", ("--l", "--list"))
def test_long_option_abbreviations_are_rejected(abbreviation):
    result = run_cli(abbreviation)

    assert result.returncode == 2
    assert result.stdout == ""
    assert "unknown option" in result.stderr


def test_collection_deduplicates_sections_and_sets_default_replay_mode():
    code = HARDWARE_INFO_PROLOGUE + (
        "print('NPU_COMPUTE_SECTIONS=' + os.environ['NPU_COMPUTE_SECTIONS']); "
        "print('NPU_COMPUTE_REPLAY_MODE=' + os.environ['NPU_COMPUTE_REPLAY_MODE'])"
    )
    result = run_cli(
        "--section",
        "Memory",
        "--section",
        "Memory",
        "--section",
        "L2Cache",
        sys.executable,
        "-c",
        code,
    )

    assert result.returncode == 0
    environment = dict(
        line.split("=", 1) for line in result.stdout.splitlines() if "=" in line
    )
    assert environment["NPU_COMPUTE_SECTIONS"] == "Memory,L2Cache"
    assert environment["NPU_COMPUTE_REPLAY_MODE"] == "kernel"


def test_arguments_after_program_are_passed_to_the_app_verbatim():
    code = HARDWARE_INFO_PROLOGUE + ("import sys; print('\\n'.join(sys.argv[1:]))")
    result = run_cli(
        "--section",
        "Memory",
        sys.executable,
        "-c",
        code,
        "--section",
        "app-owned-value",
    )

    assert result.returncode == 0
    assert result.stdout.splitlines() == ["--section", "app-owned-value"]


@pytest.mark.parametrize(
    "arguments",
    [
        (),
        ("/bin/true",),
        ("--section", "Memory"),
        ("--section", "Unknown", "/bin/true"),
        ("--section", "HardwareInfo", "/bin/true"),
        ("--section", "Memory", "--replay-mode", "application", "/bin/true"),
        (
            "--section",
            "Memory",
            "--replay-mode",
            "kernel",
            "--replay-mode",
            "kernel",
            "/bin/true",
        ),
        ("--section", "Memory", "--", "/bin/true"),
        ("--help", "--section", "Memory"),
        ("--list-sections", "--section", "Memory"),
        ("--export", "result.repo"),
        ("--import", "one.repo", "--import", "two.repo"),
        ("--export", "one.repo", "--export", "two.repo"),
    ],
)
def test_invalid_options_and_combinations_exit_two(arguments):
    result = run_cli(*arguments)

    assert result.returncode == 2
    assert result.stderr.startswith("npu-compute:")


@pytest.mark.parametrize(
    "arguments",
    [
        ("--import", "input.repo"),
        ("--import", "input.repo", "--export", "output.repo"),
        ("--section", "Memory", "--export", "output.repo", "/bin/true"),
    ],
)
def test_repo_paths_are_recognized_but_reported_as_not_available(arguments):
    result = run_cli(*arguments)

    assert result.returncode == 4
    assert "repo import/export is not available" in result.stderr


@pytest.mark.parametrize(
    "obsolete_option",
    ("--target", "--session", "--metric", "--dry-run"),
)
def test_pr_prototype_options_are_rejected(obsolete_option):
    result = run_cli(obsolete_option)

    assert result.returncode == 2
    assert "unknown option" in result.stderr
