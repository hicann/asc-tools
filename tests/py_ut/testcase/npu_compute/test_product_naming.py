# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
LEGACY_NAMES = (
    "cann" + "_compute",
    "cann" + "-compute",
    "lib" + "cann" + "-compute",
    "CANN" + "_COMPUTE_",
    "Cann" + "Compute",
)
TEXT_SUFFIXES = {
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".md",
    ".py",
    ".txt",
}
TARGET_DECLARATION = re.compile(r"add_(?:library|executable)\(\s*([^\s)]+)")


def test_npu_compute_product_naming_is_consistent():
    product_root = REPO_ROOT / "npu_tools/npu_compute"
    assert product_root.is_dir()
    assert not (REPO_ROOT / ("cann" + "_compute")).exists()

    files = [REPO_ROOT / "CMakeLists.txt"]
    files.extend(
        path
        for root in (product_root, REPO_ROOT / "tests")
        for path in root.rglob("*")
        if path.is_file()
        and (path.name == "CMakeLists.txt" or path.suffix in TEXT_SUFFIXES)
    )

    violations = []
    for path in files:
        content = path.read_text(encoding="utf-8")
        for legacy_name in LEGACY_NAMES:
            if legacy_name in content:
                violations.append(f"{path.relative_to(REPO_ROOT)}: {legacy_name}")

    assert violations == []


def test_npu_compute_build_and_log_names_are_consistent():
    product_cmake = (REPO_ROOT / "npu_tools/npu_compute" / "CMakeLists.txt").read_text(
        encoding="utf-8"
    )
    library_cmake = (
        REPO_ROOT / "npu_tools/npu_compute" / "src" / "npu_compute" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    cli_cmake = (
        REPO_ROOT
        / "npu_tools/npu_compute"
        / "src"
        / "compute_launcher"
        / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    cli_main = (
        REPO_ROOT / "npu_tools/npu_compute" / "src" / "compute_launcher" / "main.cpp"
    ).read_text(encoding="utf-8")
    library_source = (
        REPO_ROOT / "npu_tools/npu_compute" / "src" / "npu_compute" / "npu_compute.cpp"
    ).read_text(encoding="utf-8")

    assert "project(npu_compute LANGUAGES CXX)" in product_cmake
    assert "add_library(npu_compute SHARED" in library_cmake
    assert "OUTPUT_NAME npu-compute" in library_cmake
    assert "add_executable(npu_compute_cli" in cli_cmake
    assert "OUTPUT_NAME npu-compute" in cli_cmake
    assert '"npu-compute: %s\\n"' in cli_main
    assert '"[libnpu-compute]' in library_source


def test_root_build_always_includes_npu_compute_and_sanitizer():
    top_level_cmake = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    npu_tools_cmake = (REPO_ROOT / "npu_tools" / "CMakeLists.txt").read_text(
        encoding="utf-8"
    )
    sanitizer_cmake = (
        REPO_ROOT / "npu_tools/npu_sanitizer" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    build_script = (REPO_ROOT / "build.sh").read_text(encoding="utf-8")

    for feature_switch in (
        "BUILD_NPU_SANITIZER",
        "ASC_TOOLS_BUILD_NPU_COMPUTE",
        "NPU_COMPUTE_BUILD_INTEGRATION_STUBS",
    ):
        assert feature_switch not in top_level_cmake
        assert feature_switch not in build_script

    assert (
        'set(NPU_SANITIZER_INSTALL_BINDIR "${CMAKE_SYSTEM_PROCESSOR}-linux/bin")'
        in sanitizer_cmake
    )
    assert (
        'set(NPU_SANITIZER_INSTALL_LIBDIR "${CMAKE_SYSTEM_PROCESSOR}-linux/lib64")'
        in sanitizer_cmake
    )
    assert (
        'set(NPU_SANITIZER_INSTALL_INCLUDEDIR "${CMAKE_SYSTEM_PROCESSOR}-linux/include")'
        in sanitizer_cmake
    )
    assert "install(TARGETS npu_check_cli" in sanitizer_cmake
    assert "install(TARGETS npu_check acl_san" in sanitizer_cmake
    assert "install(FILES npu_check/include/npu_check.h" in sanitizer_cmake

    for submodule in ("npu_check_cli", "npu_check", "sanitizer_api"):
        submodule_cmake = (
            REPO_ROOT / "npu_tools/npu_sanitizer" / submodule / "CMakeLists.txt"
        ).read_text(encoding="utf-8")
        assert "install(" not in submodule_cmake

    assert "add_subdirectory(npu_tools)" in top_level_cmake
    for subdirectory in ("injection", "npu_compute", "npu_sanitizer"):
        assert f"add_subdirectory({subdirectory})" in npu_tools_cmake

    sanitizer_subdirectories = (
        "common",
        "dbi",
        "sanitizer_api",
        "npu_check_cli",
        "npu_check",
        "dbi/test",
    )
    subdirectory_positions = [
        sanitizer_cmake.index(f"add_subdirectory({subdirectory})")
        for subdirectory in sanitizer_subdirectories
    ]
    assert subdirectory_positions == sorted(subdirectory_positions)


def test_cmake_test_switches_are_owned_by_their_consumer_modules():
    top_level_cmake = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    npu_compute_cmake = (
        REPO_ROOT / "npu_tools/npu_compute" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    dbi_test_cmake = (
        REPO_ROOT / "npu_tools/npu_sanitizer" / "dbi" / "test" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")

    npu_compute_switch = "NPU_COMPUTE_BUILD_TESTS"
    assert f"option({npu_compute_switch}" in top_level_cmake
    assert f"option({npu_compute_switch}" in npu_compute_cmake

    dbi_options = (
        "NPU_CHECK_ENABLE_REAL_DBI_FLOW",
        "NPU_CHECK_ENABLE_REAL_DBI_TOOLCHAIN",
        "NPU_CHECK_RUN_REAL_DBI_HARDWARE",
    )
    dbi_cache_variables = (
        "NPU_CHECK_REAL_DBI_ARCH",
        "NPU_CHECK_REAL_DBI_KERNEL",
        "NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT",
        "NPU_CHECK_REAL_DBI_DEVICE_ID",
    )
    for option in dbi_options:
        assert f"option({option}" in top_level_cmake
        assert f"option({option}" in dbi_test_cmake
    for variable in dbi_cache_variables:
        assert f"set({variable}" in top_level_cmake
        assert f"set({variable}" in dbi_test_cmake


def test_cmake_targets_use_component_prefixes_and_merge_data_module():
    product_root = REPO_ROOT / "npu_tools/npu_compute"
    product_cmake = (product_root / "CMakeLists.txt").read_text(encoding="utf-8")
    assert "NPU_COMPUTE_BUILD_DEMO" not in product_cmake
    assert "add_subdirectory(demo)" not in product_cmake

    cmake_files = list(product_root.rglob("CMakeLists.txt"))
    cmake_content = "\n".join(path.read_text(encoding="utf-8") for path in cmake_files)
    targets = TARGET_DECLARATION.findall(cmake_content)
    local_targets = [target for target in targets if "::" not in target]

    assert local_targets
    assert all(
        target.startswith(("acl_", "acl_pti_", "npu_compute"))
        or target in {"acl_pti", "npu_compute"}
        for target in local_targets
    )
    assert "asc_cc_" not in cmake_content

    acl_pti_cmake = (product_root / "src" / "acl_pti" / "CMakeLists.txt").read_text(
        encoding="utf-8"
    )
    assert "add_library(acl_pti SHARED" in acl_pti_cmake
    assert "data/api.cpp" in acl_pti_cmake
    assert "data/module.cpp" in acl_pti_cmake
    assert "data/raw_data_decoder.cpp" in acl_pti_cmake
    assert "add_library(acl_pti_data_module_impl" not in cmake_content
    assert "OUTPUT_NAME pti_data_module_impl" not in cmake_content


def test_runtime_replacements_use_specific_flat_names():
    acl_pti_source = REPO_ROOT / "npu_tools/npu_compute" / "src" / "acl_pti"
    replacement_source = acl_pti_source / "replacement"
    header = replacement_source / "runtime_api_replacements.h"
    source = replacement_source / "runtime_api_replacements.cpp"
    replay_runtime_header = acl_pti_source / "profiling" / "replay_runtime.h"

    assert not (acl_pti_source / "runtime_replacement").exists()
    assert header.is_file()
    assert source.is_file()
    assert replay_runtime_header.is_file()

    header_content = header.read_text(encoding="utf-8")
    assert "namespace npu_compute::aclpti::replacement" in header_content
    assert "class RuntimeApiReplacements" not in header_content
    assert "bool RegisterRuntimeApiReplacements();" in header_content
    assert "ReplayMemory*" not in header_content
    assert "RangeProfiler*" not in header_content

    replay_runtime_content = replay_runtime_header.read_text(encoding="utf-8")
    assert "class ReplayRuntime" in replay_runtime_content
    assert "ReplayMemory replayMemory_;" in replay_runtime_content
    assert "RangeProfiler rangeProfiler_;" in replay_runtime_content
    assert "ReplayRuntime& GetReplayRuntime();" in replay_runtime_content

    assert not (acl_pti_source / "manager.h").exists()
    assert not (acl_pti_source / "manager.cpp").exists()
    assert (acl_pti_source / "initialization.h").is_file()
    assert (acl_pti_source / "initialization.cpp").is_file()
