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
import posixpath
import re
import subprocess
from collections import Counter
from pathlib import Path, PurePosixPath

import pytest


PACKAGE_ARCH = os.environ.get("NPU_COMPUTE_TEST_ARCH", platform.machine())
ARCH_ROOT = f"{PACKAGE_ARCH}-linux"
REQUIRED_PATHS = frozenset(
    {
        f"{ARCH_ROOT}/bin/npu-compute",
        f"{ARCH_ROOT}/include/aclpti/aclpti.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_activity.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_callback.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_data.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_export.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_range_profiler.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_runtime_api.h",
        f"{ARCH_ROOT}/include/aclpti/aclpti_types.h",
        f"{ARCH_ROOT}/lib64/libacl_pti.so",
        f"{ARCH_ROOT}/lib64/libacl_tool_injection.so",
        f"{ARCH_ROOT}/lib64/libnpu-compute.so",
        "share/npu-compute/sections/README.md",
    }
)
UNIQUE_REQUIRED_PATHS = frozenset(
    {
        f"{ARCH_ROOT}/bin/npu-compute",
        f"{ARCH_ROOT}/lib64/libacl_pti.so",
        f"{ARCH_ROOT}/lib64/libacl_tool_injection.so",
        f"{ARCH_ROOT}/lib64/libnpu-compute.so",
    }
)
FORBIDDEN_FILE_NAMES = frozenset(
    {
        "libacl_pti_callback_stub.so",
        "libnpu_compute_hardware_api_stub.so",
        "libprofapi.so",
        "libpti_data_module.so",
        "libpti_data_module_impl.so",
        "libruntime.so",
        "npu_compute_demo_app",
    }
)
LIST_PATH_PATTERN = re.compile(r"(?:^|\s)(\./\S+)")
RUN_PACKAGE = os.environ.get("NPU_COMPUTE_RUN_PACKAGE")


def list_package_paths(package):
    result = subprocess.run(
        ["bash", str(package), "--list"],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"run package --list failed with exit code {result.returncode}: "
            f"{result.stderr.strip()}"
        )

    paths = []
    for line in result.stdout.splitlines():
        match = LIST_PATH_PATTERN.search(line)
        if match is None:
            continue
        normalized = posixpath.normpath(match.group(1))
        if normalized == "." or normalized.startswith("../"):
            continue
        paths.append(normalized)
    return paths


def validate_paths(paths):
    errors = []
    path_counts = Counter(paths)
    available_paths = set(paths)

    for path in sorted(REQUIRED_PATHS - available_paths):
        errors.append(f"missing required path: {path}")

    for path in sorted(available_paths):
        if PurePosixPath(path).name in FORBIDDEN_FILE_NAMES:
            errors.append(f"forbidden path: {path}")

    for path, count in sorted(path_counts.items()):
        if count > 1 and path in REQUIRED_PATHS:
            errors.append(f"required path appears more than once: {path}")

    unique_names = {
        PurePosixPath(required).name: required for required in UNIQUE_REQUIRED_PATHS
    }
    for path in sorted(available_paths):
        expected_path = unique_names.get(PurePosixPath(path).name)
        if expected_path is not None and path != expected_path:
            errors.append(f"required file name appears at conflicting path: {path}")

    return errors


def create_run_package(tmp_path, paths, return_code=0):
    package = tmp_path / "cann-asc-tools_test_linux-x86_64.run"
    listing = "\n".join(
        f"-r-xr-x--- 0/0 1 2026-01-01 00:00 ././{path}" for path in paths
    )
    package.write_text(
        "#!/bin/bash\n"
        'if [[ "$1" != "--list" ]]; then exit 64; fi\n'
        "cat <<'EOF'\n"
        "Target directory: makeself_staging\n"
        f"{listing}\n"
        "EOF\n"
        f"exit {return_code}\n",
        encoding="utf-8",
    )
    return package


def test_complete_run_package_passes(tmp_path):
    package = create_run_package(tmp_path, REQUIRED_PATHS)

    assert validate_paths(list_package_paths(package)) == []


def test_missing_required_file_fails(tmp_path):
    required_library = f"{ARCH_ROOT}/lib64/libacl_pti.so"
    paths = tuple(path for path in REQUIRED_PATHS if path != required_library)
    package = create_run_package(tmp_path, paths)

    assert f"missing required path: {required_library}" in validate_paths(
        list_package_paths(package)
    )


def test_forbidden_file_fails(tmp_path):
    forbidden_library = f"{ARCH_ROOT}/lib64/libprofapi.so"
    package = create_run_package(tmp_path, (*REQUIRED_PATHS, forbidden_library))

    assert f"forbidden path: {forbidden_library}" in validate_paths(
        list_package_paths(package)
    )


def test_list_command_failure_fails(tmp_path):
    package = create_run_package(tmp_path, REQUIRED_PATHS, return_code=23)

    with pytest.raises(
        RuntimeError, match="run package --list failed with exit code 23"
    ):
        list_package_paths(package)


def test_required_file_at_conflicting_path_fails(tmp_path):
    package = create_run_package(
        tmp_path,
        (*REQUIRED_PATHS, "alternate/lib64/libnpu-compute.so"),
    )

    assert (
        "required file name appears at conflicting path: "
        "alternate/lib64/libnpu-compute.so"
        in validate_paths(list_package_paths(package))
    )


def test_top_level_npu_compute_file_conflicts_with_architecture_layout(tmp_path):
    package = create_run_package(
        tmp_path,
        (*REQUIRED_PATHS, "bin/npu-compute"),
    )

    assert (
        "required file name appears at conflicting path: bin/npu-compute"
        in validate_paths(list_package_paths(package))
    )


@pytest.mark.skipif(
    RUN_PACKAGE is None,
    reason="set NPU_COMPUTE_RUN_PACKAGE to verify a generated run package",
)
def test_generated_run_package_has_expected_npu_compute_contents():
    package = Path(RUN_PACKAGE)

    assert package.is_file()
    assert validate_paths(list_package_paths(package)) == []
