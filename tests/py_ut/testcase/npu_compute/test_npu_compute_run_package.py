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
import shutil
import subprocess
from collections import Counter
from pathlib import Path, PurePosixPath

import pytest


PACKAGE_ARCH = os.environ.get("NPU_COMPUTE_TEST_ARCH", platform.machine())
ARCH_ROOT = f"{PACKAGE_ARCH}-linux"
SANITIZER_ROOT = "tools/npu_tools"
NPU_COMPUTE_ROOT = f"{ARCH_ROOT}/tools/npu_tools"
NPU_CHECK_PATHS = frozenset(
    {
        f"{ARCH_ROOT}/bin/npu-check",
        f"{SANITIZER_ROOT}/bin/npu-check",
    }
)
REQUIRED_PATHS = (
    frozenset(
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
            f"{NPU_COMPUTE_ROOT}/lib64/libacl_pti.so",
            f"{NPU_COMPUTE_ROOT}/lib64/libacl_tool_injection.so",
            f"{NPU_COMPUTE_ROOT}/lib64/libnpu-compute.so",
            f"{SANITIZER_ROOT}/lib64/libacl_san.so",
            f"{SANITIZER_ROOT}/lib64/libnpu_check.so",
            "libexec/aclsan/gen_ctrlbin",
            "share/aclsan/dbi/probes/fixpipe.cpp",
            "share/aclsan/dbi/probes/mte1.cpp",
            "share/aclsan/dbi/probes/mte2.cpp",
            "share/aclsan/dbi/probes/mte3.cpp",
            "share/aclsan/dbi/probes/scalar.cpp",
            "share/aclsan/dbi/probes/sync.cpp",
            "share/aclsan/dbi/trace_buffer_abi.h",
            "share/aclsan/dbi/trace_record.h",
        }
    )
    | NPU_CHECK_PATHS
)
UNIQUE_REQUIRED_PATHS = frozenset(
    {
        f"{ARCH_ROOT}/bin/npu-compute",
        f"{NPU_COMPUTE_ROOT}/lib64/libacl_pti.so",
        f"{NPU_COMPUTE_ROOT}/lib64/libacl_tool_injection.so",
        f"{NPU_COMPUTE_ROOT}/lib64/libnpu-compute.so",
        f"{SANITIZER_ROOT}/lib64/libacl_san.so",
        f"{SANITIZER_ROOT}/lib64/libnpu_check.so",
        "libexec/aclsan/gen_ctrlbin",
    }
)
EXPECTED_PATHS_BY_REQUIRED_FILE_NAME = {
    PurePosixPath(required).name: frozenset({required})
    for required in UNIQUE_REQUIRED_PATHS
}
EXPECTED_PATHS_BY_REQUIRED_FILE_NAME["npu-check"] = NPU_CHECK_PATHS
FORBIDDEN_FILE_NAMES = frozenset(
    {
        "libacl_pti_callback_stub.so",
        "libnpu_compute_hardware_api_stub.so",
        "libprofapi.so",
        "libpti_data_module.so",
        "libpti_data_module_impl.so",
        "libruntime.so",
        "npu_compute_demo_app",
        "npu_check",
    }
)
FORBIDDEN_PATH_PREFIXES = (f"{SANITIZER_ROOT}/include/",)
LIST_PATH_PATTERN = re.compile(r"(?:^|\s)(\./\S+)")
RUN_PACKAGE = os.environ.get("ASC_TOOLS_RUN_PACKAGE")


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
        if path.startswith(FORBIDDEN_PATH_PREFIXES):
            errors.append(f"forbidden path: {path}")
        if PurePosixPath(path).name in FORBIDDEN_FILE_NAMES:
            errors.append(f"forbidden path: {path}")

    for path, count in sorted(path_counts.items()):
        if count > 1 and path in REQUIRED_PATHS:
            errors.append(f"required path appears more than once: {path}")

    for path in sorted(available_paths):
        expected_paths = EXPECTED_PATHS_BY_REQUIRED_FILE_NAME.get(
            PurePosixPath(path).name
        )
        if expected_paths is not None and path not in expected_paths:
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
    required_library = f"{NPU_COMPUTE_ROOT}/lib64/libacl_pti.so"
    paths = tuple(path for path in REQUIRED_PATHS if path != required_library)
    package = create_run_package(tmp_path, paths)

    assert f"missing required path: {required_library}" in validate_paths(
        list_package_paths(package)
    )


def test_missing_npu_check_library_fails(tmp_path):
    required_library = f"{SANITIZER_ROOT}/lib64/libnpu_check.so"
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


def test_sanitizer_header_fails(tmp_path):
    forbidden_header = f"{SANITIZER_ROOT}/include/aclsan/aclsan_api.h"
    package = create_run_package(tmp_path, (*REQUIRED_PATHS, forbidden_header))

    assert f"forbidden path: {forbidden_header}" in validate_paths(
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


def test_third_npu_check_file_conflicts_with_wrapper_and_executable(tmp_path):
    package = create_run_package(
        tmp_path,
        (*REQUIRED_PATHS, "alternate/bin/npu-check"),
    )

    assert (
        "required file name appears at conflicting path: alternate/bin/npu-check"
        in validate_paths(list_package_paths(package))
    )


def test_npu_check_wrapper_is_installed_to_arch_bin():
    sanitizer_cmake = (
        Path(__file__).parents[4] / "npu_sanitizer" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")

    wrapper_install = re.compile(
        r"install\(PROGRAMS\s+"
        r'"\$\{CMAKE_CURRENT_SOURCE_DIR\}/npu_check_cli/npu_check\.sh"\s+'
        r'DESTINATION "\$\{NPU_SANITIZER_INSTALL_BINDIR\}".*?'
        r"\bRENAME npu-check\b.*?"
        r"\bCOMPONENT asc-tools\s*\)",
        re.DOTALL,
    )
    assert wrapper_install.search(sanitizer_cmake)


def test_npu_check_library_keeps_origin_rpath_for_packaged_dependency():
    repo_root = Path(__file__).parents[4]
    top_level_cmake = (repo_root / "CMakeLists.txt").read_text(encoding="utf-8")
    npu_check_cmake = (
        repo_root / "npu_sanitizer" / "npu_check" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")

    assert "set(CMAKE_SKIP_RPATH TRUE)" in top_level_cmake
    assert '"LINKER:-rpath,$ORIGIN"' in npu_check_cmake


def test_npu_check_wrapper_executes_real_binary(tmp_path):
    repo_root = Path(__file__).parents[4]
    wrapper_source = repo_root / "npu_sanitizer" / "npu_check_cli" / "npu_check.sh"
    install_root = tmp_path / "cann"
    arch_bin = install_root / ARCH_ROOT / "bin"
    wrapper = arch_bin / "npu-check"
    real_binary = install_root / SANITIZER_ROOT / "bin" / "npu-check"
    arch_bin.mkdir(parents=True)
    real_binary.parent.mkdir(parents=True)
    (install_root / "bin").symlink_to(f"{ARCH_ROOT}/bin", target_is_directory=True)

    shutil.copy2(wrapper_source, wrapper)
    wrapper.chmod(0o750)
    real_binary.write_text(
        "#!/bin/sh\nprintf 'pid=%s\\n' \"$$\"\nprintf 'arg=<%s>\\n' \"$@\"\nexit 23\n",
        encoding="utf-8",
    )
    real_binary.chmod(0o750)

    process = subprocess.Popen(
        [
            str(install_root / "bin" / "npu-check"),
            "argument with spaces",
            "--flag=value",
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = process.communicate()

    assert process.returncode == 23
    assert stderr == ""
    assert stdout.splitlines() == [
        f"pid={process.pid}",
        "arg=<argument with spaces>",
        "arg=<--flag=value>",
    ]


@pytest.mark.skipif(
    RUN_PACKAGE is None,
    reason="set ASC_TOOLS_RUN_PACKAGE to verify a generated run package",
)
def test_generated_run_package_has_expected_contents():
    package = Path(RUN_PACKAGE)

    assert package.is_file()
    assert validate_paths(list_package_paths(package)) == []
