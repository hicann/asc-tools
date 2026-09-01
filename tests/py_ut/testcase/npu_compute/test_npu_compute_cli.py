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
BIN_DIR = Path(os.environ.get("NPU_COMPUTE_TEST_BIN_DIR", str(BUILD_DIR / "bin")))
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
    ".write_text('{}\\n' * 5, encoding='utf-8'); "
)


def run_cli(*arguments, cwd=None):
    assert CLI.is_file(), f"npu-compute was not built: {CLI}"
    return subprocess.run(
        [str(CLI), *arguments],
        cwd=cwd,
        text=True,
        capture_output=True,
        check=False,
    )


def run_cli_with_combined_output(*arguments, cwd=None):
    assert CLI.is_file(), f"npu-compute was not built: {CLI}"
    return subprocess.run(
        [str(CLI), *arguments],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
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


@pytest.mark.parametrize(
    "arguments",
    [
        ("-h",),
        ("--help",),
        ("-h", "--help"),
        ("--section", "Memory", "--help", "--export", "result.npu-rep"),
    ],
)
def test_help_with_valid_tool_options_exits_zero(arguments, tmp_path):
    result = run_cli(*arguments, cwd=tmp_path)

    assert result.returncode == 0
    assert result.stdout.count("Usage:") == 1
    assert result.stderr == ""
    assert list(tmp_path.iterdir()) == []


@pytest.mark.parametrize(
    ("arguments", "expected_errors"),
    [
        (
            ("--bad-option", "--help"),
            ["unknown option: --bad-option"],
        ),
        (
            ("--section", "--help"),
            ["--section requires a value"],
        ),
        (
            ("--bad-one", "--bad-two", "--help"),
            ["unknown option: --bad-one", "unknown option: --bad-two"],
        ),
        (
            ("--bad-option", "--list-sections", "--help"),
            ["unknown option: --bad-option"],
        ),
        (
            (
                "--section",
                "Invalid",
                "--replay-mode",
                "invalid",
                "--help",
            ),
            [
                "unknown section: Invalid",
                "--replay-mode currently only accepts kernel",
            ],
        ),
    ],
)
def test_help_reports_all_option_errors(arguments, expected_errors, tmp_path):
    result = run_cli(*arguments, cwd=tmp_path)

    assert result.returncode == 2
    assert result.stdout.count("Usage:") == 1
    assert result.stderr.splitlines() == [
        f"npu-compute: {message}" for message in expected_errors
    ]
    assert list(tmp_path.iterdir()) == []


def test_help_prints_all_errors_before_usage_and_does_not_run_program(tmp_path):
    marker = tmp_path / "target-ran"
    result = run_cli_with_combined_output(
        "--bad-one",
        "--bad-two",
        "--help",
        "/bin/sh",
        "-c",
        f"touch {marker}",
        cwd=tmp_path,
    )

    assert result.returncode == 2
    assert result.stdout.count("Usage:") == 1
    assert result.stdout.index("unknown option: --bad-one") < result.stdout.index(
        "unknown option: --bad-two"
    )
    assert result.stdout.index("unknown option: --bad-two") < result.stdout.index(
        "Usage:"
    )
    assert not marker.exists()
    assert list(tmp_path.iterdir()) == []


@pytest.mark.parametrize("program", ("hh", "jojhhh"))
def test_missing_section_identifies_the_parsed_program(program, tmp_path):
    result = run_cli(program, "-h", "/path/to/run.sh", cwd=tmp_path)

    assert result.returncode == 2
    assert result.stdout == ""
    assert result.stderr == (
        f"npu-compute: missing required --section option before program '{program}'\n"
    )
    assert list(tmp_path.iterdir()) == []


def test_list_sections_outputs_only_ids_in_fixed_order():
    result = run_cli("--list-sections")

    assert result.returncode == 0
    assert result.stdout.splitlines() == SECTIONS
    assert result.stderr == ""


@pytest.mark.parametrize(
    "arguments",
    [
        ("--list-sections", "--help"),
        ("--help", "--list-sections"),
    ],
)
def test_help_takes_precedence_over_list_sections(arguments, tmp_path):
    result = run_cli(*arguments, cwd=tmp_path)

    assert result.returncode == 0
    assert result.stdout.count("Usage:") == 1
    assert result.stderr == ""
    assert not any(line in SECTIONS for line in result.stdout.splitlines())
    assert list(tmp_path.iterdir()) == []


@pytest.mark.parametrize("section", SECTIONS)
def test_each_supported_section_is_accepted(section, tmp_path):
    code = HARDWARE_INFO_PROLOGUE + (
        "print('NPU_COMPUTE_SECTIONS=' + os.environ['NPU_COMPUTE_SECTIONS'])"
    )
    result = run_cli("--section", section, sys.executable, "-c", code, cwd=tmp_path)

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


def test_collection_deduplicates_sections_and_sets_default_replay_mode(tmp_path):
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
        cwd=tmp_path,
    )

    assert result.returncode == 0
    environment = dict(
        line.split("=", 1) for line in result.stdout.splitlines() if "=" in line
    )
    assert environment["NPU_COMPUTE_SECTIONS"] == "Memory,L2Cache"
    assert environment["NPU_COMPUTE_REPLAY_MODE"] == "kernel"


def test_arguments_after_program_are_passed_to_the_app_verbatim(tmp_path):
    code = HARDWARE_INFO_PROLOGUE + ("import sys; print('\\n'.join(sys.argv[1:]))")
    result = run_cli(
        "--section",
        "Memory",
        sys.executable,
        "-c",
        code,
        "--section",
        "app-owned-value",
        cwd=tmp_path,
    )

    assert result.returncode == 0
    assert result.stdout.splitlines() == ["--section", "app-owned-value"]


@pytest.mark.parametrize("app_argument", ("-h", "--help"))
def test_help_after_program_is_passed_to_the_app(app_argument, tmp_path):
    code = HARDWARE_INFO_PROLOGUE + "import sys; print(sys.argv[1])"
    result = run_cli(
        "--section",
        "Memory",
        sys.executable,
        "-c",
        code,
        app_argument,
        cwd=tmp_path,
    )

    assert result.returncode == 0
    assert result.stdout.strip() == app_argument


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
        ("--help=value",),
        ("-hh",),
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
        ("--import", "missing.npu-rep"),
        ("--import", "missing.npu-rep", "--export", "restored"),
    ],
)
def test_missing_import_report_returns_report_error(arguments, tmp_path):
    result = run_cli(*arguments, cwd=tmp_path)

    assert result.returncode == 4
    assert "inspect imported rep failed: missing.npu-rep" in result.stderr


def test_collection_rejects_export_path_without_report_suffix(tmp_path):
    result = run_cli(
        "--section",
        "Memory",
        "--export",
        "output.repo",
        "/bin/true",
        cwd=tmp_path,
    )

    assert result.returncode == 4
    assert (
        "report export path must be an existing directory or end with .npu-rep"
        in result.stderr
    )


@pytest.mark.parametrize(
    "obsolete_option",
    ("--target", "--session", "--metric", "--dry-run"),
)
def test_pr_prototype_options_are_rejected(obsolete_option):
    result = run_cli(obsolete_option)

    assert result.returncode == 2
    assert "unknown option" in result.stderr
