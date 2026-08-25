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
import platform
import subprocess
import sys
from pathlib import Path

import pytest


BUILD_DIR = Path(
    os.environ.get(
        "NPU_COMPUTE_TEST_BUILD_DIR",
        "/tmp/asc_tools_npu_compute_integration",
    )
)
BIN_DIR = Path(
    os.environ.get(
        "NPU_COMPUTE_TEST_BIN_DIR",
        str(BUILD_DIR / "npu_compute/bin"),
    )
)
INSTALL_ARCH = os.environ.get("NPU_COMPUTE_TEST_ARCH", platform.machine())


@pytest.fixture(scope="module")
def install_root(tmp_path_factory):
    prefix = tmp_path_factory.mktemp("asc_tools_default_install")
    result = subprocess.run(
        [
            "cmake",
            "--install",
            str(BUILD_DIR),
            "--prefix",
            str(prefix),
            "--component",
            "asc-tools",
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    return prefix


@pytest.fixture(scope="module")
def public_install_root(install_root, tmp_path_factory):
    public_root = tmp_path_factory.mktemp("asc_tools_public_install")
    architecture_root = install_root / f"{INSTALL_ARCH}-linux"
    for directory in ("bin", "include", "lib64"):
        (public_root / directory).symlink_to(
            architecture_root / directory,
            target_is_directory=True,
        )
    return public_root


def test_default_component_installs_the_declared_layout(install_root):
    architecture_root = install_root / f"{INSTALL_ARCH}-linux"
    assert (architecture_root / "bin/npu-compute").is_file()
    for library in (
        "libnpu-compute.so",
        "libacl_pti.so",
        "libacl_tool_injection.so",
    ):
        assert (architecture_root / "lib64" / library).is_file()

    aclpti_headers = architecture_root / "include/aclpti"
    for header in (
        "aclpti.h",
        "aclpti_activity.h",
        "aclpti_callback.h",
        "aclpti_data.h",
        "aclpti_export.h",
        "aclpti_range_profiler.h",
        "aclpti_runtime_api.h",
        "aclpti_types.h",
    ):
        assert (aclpti_headers / header).is_file()
    data_header = (aclpti_headers / "aclpti_data.h").read_text(encoding="utf-8")
    assert "PtiStatus" not in data_header
    assert "PtiDataModule" not in data_header
    assert "class Module" not in data_header
    assert "ReplayPrepareInfo" not in data_header
    assert "npu_compute::pti" not in data_header
    assert not (architecture_root / "include/npu_compute/acl_pti.h").exists()
    assert not (architecture_root / "include/npu_compute/pti_data_module.h").exists()

    assert (install_root / "share/npu-compute/sections").is_dir()
    assert not (architecture_root / "bin/npu_compute_demo_app").exists()
    assert not (architecture_root / "lib64/libpti_data_module.so").exists()
    assert not (architecture_root / "lib64/libpti_data_module_impl.so").exists()
    assert not (architecture_root / "lib64/libprofapi.so").exists()
    assert not (architecture_root / "lib64/libruntime.so").exists()

    aclpti_symbols = subprocess.run(
        [
            "nm",
            "-D",
            "--defined-only",
            "--demangle",
            str(architecture_root / "lib64/libacl_pti.so"),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    assert aclpti_symbols.returncode == 0, aclpti_symbols.stderr
    assert "aclptiRegisterPmuDataCallback" in aclpti_symbols.stdout
    assert "aclptiRegisterDataModuleShutdownCallback" in aclpti_symbols.stdout
    for internal_namespace in (
        "activity",
        "callback",
        "data",
        "profiling",
        "replacement",
    ):
        assert (
            f"npu_compute::aclpti::{internal_namespace}::" not in aclpti_symbols.stdout
        )


def test_public_cli_symlink_uses_the_architecture_injection_library(
    install_root, public_install_root
):
    cli = public_install_root / "bin/npu-compute"
    assert cli.is_file()
    program = (
        "import os, pathlib; "
        "pathlib.Path(os.environ['NPU_COMPUTE_OUTPUT'], 'HardwareInfo.jsonl')"
        ".write_text('{}\\n' * 5, encoding='utf-8'); "
        "print('ACL_API_INJECTION=' + os.environ['ACL_API_INJECTION'])"
    )
    result = subprocess.run(
        [str(cli), "--section", "PipeUtilization", sys.executable, "-c", program],
        cwd=public_install_root,
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    expected = (
        install_root / f"{INSTALL_ARCH}-linux" / "lib64/libnpu-compute.so"
    ).resolve()
    assert f"ACL_API_INJECTION={expected}" in result.stdout.splitlines()


def test_installed_layout_runs_the_minimal_stub_chain(
    install_root, public_install_root
):
    architecture_root = install_root / f"{INSTALL_ARCH}-linux"
    cli = public_install_root / "bin/npu-compute"
    assert cli.is_file()
    environment = os.environ.copy()
    search_paths = [str(architecture_root / "lib64"), str(BIN_DIR)]
    if environment.get("LD_LIBRARY_PATH"):
        search_paths.append(environment["LD_LIBRARY_PATH"])
    environment["LD_LIBRARY_PATH"] = ":".join(search_paths)
    environment.pop("NPU_COMPUTE_STUB_SUBSCRIBE_RESULT", None)

    result = subprocess.run(
        [
            str(cli),
            "--section",
            "PipeUtilization",
            str(BIN_DIR / "npu_compute_demo_app"),
        ],
        cwd=public_install_root,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "[libnpu-compute] subscriber initialized" in result.stderr
    assert "[demo] completed" in result.stderr
