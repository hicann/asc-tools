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

"""asc_tools_probe 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_asc_tools_probe.py -q
"""

from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import Mock, patch

import pytest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "debug" / "asc_tools_probe.py"
sys.path.insert(0, str(TOOL.parent))
import asc_tools_probe as probe  # noqa: E402


@pytest.fixture(autouse=True)
def cmake_process(monkeypatch, tmp_path):
    """Mock only CMake discovery and execution; keep probe decisions real."""
    executable = write_executable(tmp_path / "bin" / "cmake")
    original_which = probe.shutil.which
    original_run = probe.run_quiet
    result = Mock(
        return_value={
            "attempted": True,
            "returncode": 0,
            "timed_out": False,
            "error": None,
        }
    )

    def which(name, *, path=None):
        if name == "cmake":
            return str(executable) if path else None
        return original_which(name, path=path)

    def run(command, **kwargs):
        if command[0] == str(executable):
            return result(command, **kwargs)
        return original_run(command, **kwargs)

    monkeypatch.setattr(probe.shutil, "which", which)
    monkeypatch.setattr(probe, "run_quiet", run)
    return result


def run_tool(
    *arguments: str,
    path: str | None = None,
    ascend_home: Path | None = None,
    toolkit_home: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    environment["PATH"] = os.defpath
    environment.pop("ASCEND_HOME_PATH", None)
    environment.pop("ASCEND_TOOLKIT_HOME", None)
    if ascend_home is not None:
        environment["ASCEND_HOME_PATH"] = str(ascend_home)
    if toolkit_home is not None:
        environment["ASCEND_TOOLKIT_HOME"] = str(toolkit_home)
    if path is not None:
        environment["PATH"] = path
    command = [str(TOOL), *arguments]
    stdout, stderr = StringIO(), StringIO()
    with (
        patch.object(sys, "argv", command),
        patch.dict(os.environ, environment, clear=True),
    ):
        with redirect_stdout(stdout), redirect_stderr(stderr):
            returncode = probe.main()
    return subprocess.CompletedProcess(
        command, returncode, stdout.getvalue(), stderr.getvalue()
    )


def write_executable(path: Path, body: str = "#!/bin/sh\nexit 0\n") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")
    path.chmod(0o755)
    return path


def write_cpu_debug_fixture(
    root: Path, *, product_directory: str = "Ascend910B1", layout: str = "tikicpulib"
) -> None:
    if layout == "cpudebug":
        library_root = root / "tools/cpudebug/lib64"
        include_root = root / "tools/cpudebug/include"
        config = root / "tools/cpudebug/cmake/cpudebug-config.cmake"
    else:
        library_root = root / "tools/tikicpulib/lib"
        include_root = library_root / "include"
        config = library_root / "cmake/tikicpulib-config.cmake"
    include_root.mkdir(parents=True, exist_ok=True)
    (include_root / "tikicpulib.h").write_bytes(b"fixture\n")
    for relative in (
        "libcpudebug_stubreg.so",
        "libcpudebug_cceprint.so",
        "libcpudebug_npuchk.so",
        f"{product_directory}/libcpudebug.so",
    ):
        target = library_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"fixture\n")
    config.parent.mkdir(parents=True, exist_ok=True)
    expected_product = library_root / "Ascend910B1/libcpudebug.so"
    required_paths = [
        library_root / "libcpudebug_stubreg.so",
        library_root / "libcpudebug_cceprint.so",
        library_root / "libcpudebug_npuchk.so",
        expected_product,
    ]
    checks = "\n".join(
        f'if(NOT EXISTS "{path}")\n  message(FATAL_ERROR "missing {path.name}")\nendif()'
        for path in required_paths
    )
    config.write_text(
        "set(PRODUCT_TYPE_LIST_V220_ ascend910B1;Ascend910B1)\n"
        + checks
        + "\nadd_library(probe_ascend910b1 INTERFACE IMPORTED)\n"
        + "add_library(tikicpulib::ascend910B1 ALIAS probe_ascend910b1)\n",
        encoding="utf-8",
    )


def create_complete_root(
    tmp_path: Path, version: str = "9.0.0", *, layout: str = "tikicpulib"
) -> Path:
    root = tmp_path / f"cann-{version}"
    version_file = root / "share/info/asc-tools/version.info"
    version_file.parent.mkdir(parents=True)
    version_file.write_text(f"Version={version}\n", encoding="utf-8")

    write_cpu_debug_fixture(root, layout=layout)

    report = root / "tools/ascendc_tools/ascendc_npuchk_report.py"
    report.parent.mkdir(parents=True)
    report.write_text("# fixture\n", encoding="utf-8")
    write_executable(root / "tools/msobjdump/msobjdump")
    write_executable(root / "tools/show_kernel_debug_data/show_kernel_debug_data")
    return root


def parse_valid_report(result: subprocess.CompletedProcess[str]) -> dict:
    return json.loads(result.stdout)


def capability(report: dict, capability_id: str) -> dict:
    return next(
        item
        for item in report["installations"][0]["capabilities"]
        if item["id"] == capability_id
    )


def test_complete_explicit_root_is_version_bound_and_reports_capabilities(
    tmp_path: Path,
):
    root = create_complete_root(tmp_path)
    result = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
        "--require",
        "npu-check",
        "--require",
        "msobjdump",
        "--require",
        "show-kernel-debug-data",
    )
    assert result.returncode == 0, result.stderr
    report = parse_valid_report(result)

    assert report["status"] == "available"
    assert report["requirements"] == {
        "requested": [
            "cpu-debug",
            "msobjdump",
            "npu-check",
            "show-kernel-debug-data",
        ],
        "satisfied": True,
        "missing": [],
    }
    installation = report["installations"][0]
    assert installation["root"] == str(root.resolve())
    assert installation["sources"] == ["explicit"]
    assert installation["version"]["match"] == "match"
    assert all(item["status"] == "available" for item in installation["capabilities"])
    cpu_debug = capability(report, "cpu-debug")
    assert cpu_debug["target_probe"]["attempted"] is True
    assert cpu_debug["target_probe"]["cmake_target"] == "tikicpulib::ascend910B1"


def test_modern_cpu_layout_works_without_legacy_compatibility_links(tmp_path: Path):
    root = create_complete_root(tmp_path, "9.1.0", layout="cpudebug")
    assert not (root / "tools/tikicpulib").exists()
    result = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.1.0",
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
        "--require",
        "npu-check",
    )
    assert result.returncode == 0, result.stderr
    report = parse_valid_report(result)
    cpu = capability(report, "cpu-debug")
    assert cpu["status"] == "available"
    assert cpu["layout"] == "cpudebug"
    assert cpu["target_probe"]["returncode"] == 0
    npu = capability(report, "npu-check")
    assert npu["status"] == "available"
    assert next(
        item["path"]
        for item in npu["components"]
        if item["name"] == "runtime-instrumentation"
    ) == str(root / "tools/cpudebug/lib64/libcpudebug_npuchk.so")


def test_partial_modern_layout_falls_back_to_complete_legacy_layout(tmp_path: Path):
    root = create_complete_root(tmp_path, "9.1.0")
    write_cpu_debug_fixture(root, layout="cpudebug")
    (root / "tools/cpudebug/lib64/libcpudebug_npuchk.so").unlink()
    result = run_tool(
        "--cann-root", str(root), "--npu-arch", "2201", "--require", "cpu-debug"
    )
    assert result.returncode == 0, result.stderr
    assert capability(parse_valid_report(result), "cpu-debug")["layout"] == "tikicpulib"


def test_partial_layouts_are_not_combined_into_cpu_readiness(tmp_path: Path):
    root = create_complete_root(tmp_path, "9.1.0", layout="cpudebug")
    (root / "tools/cpudebug/lib64/libcpudebug_npuchk.so").unlink()
    legacy_library = root / "tools/tikicpulib/lib/libcpudebug_npuchk.so"
    legacy_library.parent.mkdir(parents=True)
    legacy_library.write_bytes(b"fixture\n")
    result = run_tool(
        "--cann-root",
        str(root),
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
        "--require",
        "npu-check",
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    assert capability(report, "cpu-debug")["layout"] == "cpudebug"
    assert report["requirements"]["missing"] == ["cpu-debug", "npu-check"]


def test_partial_components_are_reported_without_claiming_the_tool_ran(tmp_path: Path):
    root = tmp_path / "partial"
    library_root = root / "tools/tikicpulib/lib"
    for relative in (
        "include/tikicpulib.h",
        "cmake/tikicpulib-config.cmake",
        "libcpudebug_stubreg.so",
        "libcpudebug_cceprint.so",
    ):
        target = library_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"fixture\n")
    write_executable(root / "tools/msobjdump/msobjdump")
    result = run_tool("--cann-root", str(root))
    assert result.returncode == 0, result.stderr
    report = parse_valid_report(result)

    assert report["status"] == "partial"
    assert capability(report, "msobjdump")["status"] == "available"
    assert capability(report, "show-kernel-debug-data")["status"] == "unavailable"
    cpu_debug = capability(report, "cpu-debug")
    assert cpu_debug["status"] == "partial"
    assert (
        next(
            item
            for item in cpu_debug["components"]
            if item["name"] == "product-library"
        )["status"]
        == "missing"
    )


def test_cpu_debug_requires_target_architecture_and_cmake_dependency_set(
    tmp_path: Path,
):
    root = create_complete_root(tmp_path)
    no_arch = run_tool(
        "--cann-root",
        str(root),
        "--require",
        "cpu-debug",
    )
    assert no_arch.returncode == 1, no_arch.stderr
    no_arch_report = parse_valid_report(no_arch)
    assert capability(no_arch_report, "cpu-debug")["status"] == "partial"
    assert "--npu-arch" in capability(no_arch_report, "cpu-debug")["degradation"]

    (root / "tools/tikicpulib/lib/libcpudebug_npuchk.so").unlink()
    missing_dependency = run_tool(
        "--cann-root",
        str(root),
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
    )
    assert missing_dependency.returncode == 1, missing_dependency.stderr
    dependency_report = parse_valid_report(missing_dependency)
    assert capability(dependency_report, "cpu-debug")["status"] == "partial"
    assert dependency_report["requirements"]["missing"] == ["cpu-debug"]


def test_cpu_debug_rejects_a_product_library_for_the_wrong_architecture(
    tmp_path: Path, cmake_process
):
    root = tmp_path / "wrong-product"
    write_cpu_debug_fixture(root, product_directory="Ascend310B1")
    # A real CMake configure rejects this fixture's missing target library.
    cmake_process.return_value = {
        "attempted": True,
        "returncode": 1,
        "timed_out": False,
        "error": None,
    }
    result = run_tool(
        "--cann-root",
        str(root),
        "--npu-arch",
        "dav-2201",
        "--require",
        "cpu-debug",
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    item = capability(report, "cpu-debug")
    assert item["status"] == "unusable"
    assert item["target_probe"]["attempted"] is True
    assert item["target_probe"]["returncode"] != 0


def test_cmake_command_uses_selected_package_and_preserves_failure(
    tmp_path, cmake_process
):
    root = create_complete_root(tmp_path, "9.1.0", layout="cpudebug")
    cmake_process.return_value = {
        "attempted": True,
        "returncode": None,
        "timed_out": True,
        "error": "CMake CPU Debug target probe timed out",
    }
    result = run_tool(
        "--cann-root", str(root), "--npu-arch", "2201", "--require", "cpu-debug"
    )
    assert result.returncode == 1
    item = capability(parse_valid_report(result), "cpu-debug")
    assert item["status"] == "unusable"
    assert item["target_probe"]["timed_out"] is True
    command = cmake_process.call_args.args[0]
    assert f"-Dcpudebug_DIR={root / 'tools/cpudebug/cmake'}" in command
    assert "-DPROBE_PACKAGE=cpudebug" in command
    assert "-DPROBE_TARGET=ascend910B1" in command


def test_cann_910_compatibility_symlinks_resolve_the_cpudebug_lib64_layout(
    tmp_path: Path,
):
    root = tmp_path / "cann-9.1.0"
    cpudebug = root / "tools/cpudebug"
    (cpudebug / "lib64").mkdir(parents=True)
    (root / "tools/tikicpulib").symlink_to("cpudebug", target_is_directory=True)
    (cpudebug / "lib").symlink_to("lib64", target_is_directory=True)
    (cpudebug / "cmake").mkdir()
    (cpudebug / "include").mkdir()
    (cpudebug / "lib64/cmake").symlink_to("../cmake", target_is_directory=True)
    (cpudebug / "lib64/include").symlink_to("../include", target_is_directory=True)
    write_cpu_debug_fixture(root)
    version = root / "share/info/asc-tools/version.info"
    version.parent.mkdir(parents=True)
    version.write_text("Version=9.1.0\n", encoding="utf-8")

    result = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.1.0",
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
    )
    assert result.returncode == 0, result.stderr
    report = parse_valid_report(result)
    assert capability(report, "cpu-debug")["status"] == "available"


def test_npu_check_depends_on_target_cpu_debug_readiness(tmp_path: Path):
    root = tmp_path / "npu-check-only"
    library = root / "tools/tikicpulib/lib/libcpudebug_npuchk.so"
    library.parent.mkdir(parents=True)
    library.write_bytes(b"fixture\n")
    report_script = root / "tools/ascendc_tools/ascendc_npuchk_report.py"
    report_script.parent.mkdir(parents=True)
    report_script.write_text("# fixture\n", encoding="utf-8")

    result = run_tool(
        "--cann-root",
        str(root),
        "--npu-arch",
        "2201",
        "--require",
        "npu-check",
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    assert capability(report, "cpu-debug")["status"] != "available"
    npu_check = capability(report, "npu-check")
    assert npu_check["status"] == "partial"
    assert npu_check["dependencies"] == ["cpu-debug"]


def test_required_capability_returns_one_but_still_emits_valid_report(tmp_path: Path):
    root = tmp_path / "missing"
    result = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.0.0",
        "--require",
        "npu-check",
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    assert report["requirements"] == {
        "requested": ["npu-check"],
        "satisfied": False,
        "missing": ["npu-check"],
    }
    assert report["installations"][0]["version"]["match"] == "unknown"


def test_version_mismatch_cannot_satisfy_a_requirement(tmp_path: Path):
    root = create_complete_root(tmp_path, version="9.1.0")
    result = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.0.0",
        "--require",
        "msobjdump",
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    assert report["status"] == "incompatible"
    assert report["installations"][0]["version"]["match"] == "mismatch"
    assert capability(report, "msobjdump")["status"] == "available"
    assert report["requirements"]["missing"] == ["msobjdump"]


def test_components_from_two_roots_are_never_merged(tmp_path: Path):
    common_root = tmp_path / "common-components"
    product_root = tmp_path / "product-components"
    for root in (common_root, product_root):
        version_file = root / "share/info/asc-tools/version.info"
        version_file.parent.mkdir(parents=True)
        version_file.write_text("Version=9.0.0\n", encoding="utf-8")

    common_library = common_root / "tools/tikicpulib/lib"
    for relative in (
        "include/tikicpulib.h",
        "cmake/tikicpulib-config.cmake",
        "libcpudebug_stubreg.so",
        "libcpudebug_cceprint.so",
    ):
        target = common_library / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"fixture\n")
    product_library = product_root / "tools/tikicpulib/lib/Ascend910B1/libcpudebug.so"
    product_library.parent.mkdir(parents=True)
    product_library.write_bytes(b"fixture\n")

    result = run_tool(
        "--expected-cann",
        "9.0.0",
        "--require",
        "cpu-debug",
        path="",
        ascend_home=common_root,
        toolkit_home=product_root,
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    assert len(report["installations"]) == 2
    assert report["requirements"]["missing"] == ["cpu-debug"]
    assert all(
        next(
            item for item in installation["capabilities"] if item["id"] == "cpu-debug"
        )["status"]
        != "available"
        for installation in report["installations"]
    )


def test_cli_help_timeout_marks_component_unusable(tmp_path: Path):
    root = tmp_path / "slow"
    write_executable(
        root / "tools/msobjdump/msobjdump",
        f"#!{sys.executable}\nimport time\ntime.sleep(2)\n",
    )
    result = run_tool(
        "--cann-root",
        str(root),
        "--timeout",
        "0.05",
        "--require",
        "msobjdump",
    )
    assert result.returncode == 1, result.stderr
    report = parse_valid_report(result)
    item = capability(report, "msobjdump")
    assert item["status"] == "unusable"
    assert item["help_probe"]["timed_out"] is True


def test_report_keeps_capabilities_unique_and_requirements_consistent(tmp_path: Path):
    root = create_complete_root(tmp_path)
    result = run_tool(
        "--cann-root",
        str(root),
        "--npu-arch",
        "2201",
    )
    report = parse_valid_report(result)

    capabilities = report["installations"][0]["capabilities"]
    assert len({item["id"] for item in capabilities}) == len(capabilities)
    assert report["requirements"]["satisfied"] is (
        not report["requirements"]["missing"]
    )


def test_invalid_contract_returns_two_without_a_probe_report(tmp_path: Path):
    invalid_version = run_tool(
        "--cann-root",
        str(tmp_path),
        "--expected-cann",
        "9.0",
    )
    assert invalid_version.returncode == 2
    assert json.loads(invalid_version.stderr)["status"] == "INVALID"
    assert invalid_version.stdout == ""

    invalid_timeout = run_tool("--cann-root", str(tmp_path), "--timeout", "0")
    assert invalid_timeout.returncode == 2
    assert "--timeout" in json.loads(invalid_timeout.stderr)["error"]

    invalid_arch = run_tool("--cann-root", str(tmp_path), "--npu-arch", "9999")
    assert invalid_arch.returncode == 2
    assert "--npu-arch" in json.loads(invalid_arch.stderr)["error"]


def test_output_writes_new_probe_report_atomically(tmp_path: Path):
    root = create_complete_root(tmp_path)
    output = tmp_path / "probe-report.json"
    result = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
        "--output",
        str(output),
    )
    assert result.returncode == 0, result.stderr
    assert json.loads(output.read_text(encoding="utf-8")) == json.loads(result.stdout)
    repeated = run_tool(
        "--cann-root",
        str(root),
        "--expected-cann",
        "9.0.0",
        "--npu-arch",
        "2201",
        "--require",
        "cpu-debug",
        "--output",
        str(output),
    )
    assert repeated.returncode == 2
    assert "already exists" in repeated.stderr
