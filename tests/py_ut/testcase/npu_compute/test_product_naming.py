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
TARGET_DECLARATION = re.compile(r"add_(?:library|executable)\(([A-Za-z0-9_]+)")


def test_npu_compute_product_naming_is_consistent():
    product_root = REPO_ROOT / "npu_compute"
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
    top_level_cmake = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    product_cmake = (REPO_ROOT / "npu_compute" / "CMakeLists.txt").read_text(
        encoding="utf-8"
    )
    library_cmake = (
        REPO_ROOT / "npu_compute" / "src" / "npu_compute" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    cli_cmake = (
        REPO_ROOT / "npu_compute" / "src" / "compute_launcher" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    cli_main = (
        REPO_ROOT / "npu_compute" / "src" / "compute_launcher" / "main.cpp"
    ).read_text(encoding="utf-8")
    library_source = (
        REPO_ROOT / "npu_compute" / "src" / "npu_compute" / "npu_compute.cpp"
    ).read_text(encoding="utf-8")

    assert "option(ASC_TOOLS_BUILD_NPU_COMPUTE " in top_level_cmake
    assert "project(npu_compute LANGUAGES CXX)" in product_cmake
    assert "add_library(npu_compute SHARED" in library_cmake
    assert "OUTPUT_NAME npu-compute" in library_cmake
    assert "add_executable(npu_compute_cli" in cli_cmake
    assert "OUTPUT_NAME npu-compute" in cli_cmake
    assert '"npu-compute: %s\\n"' in cli_main
    assert '"[libnpu-compute]' in library_source


def test_cmake_targets_use_component_prefixes_and_merge_data_module():
    product_root = REPO_ROOT / "npu_compute"
    cmake_files = list(product_root.rglob("CMakeLists.txt"))
    cmake_content = "\n".join(path.read_text(encoding="utf-8") for path in cmake_files)
    targets = TARGET_DECLARATION.findall(cmake_content)

    assert targets
    assert all(
        target.startswith(("acl_", "acl_pti_", "npu_compute"))
        or target in {"acl_pti", "npu_compute"}
        for target in targets
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
