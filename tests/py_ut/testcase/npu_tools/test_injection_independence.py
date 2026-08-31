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
from pathlib import Path
import subprocess


REPO_ROOT = Path(__file__).resolve().parents[4]
NPU_TOOLS_ROOT = REPO_ROOT / "npu_tools"
INJECTION_ROOT = NPU_TOOLS_ROOT / "injection"
TEXT_SUFFIXES = {".cmake", ".cpp", ".h", ".md", ".sh", ".txt"}
FORBIDDEN_PRODUCT_NAMES = (
    "npu_compute",
    "NPU_COMPUTE",
    "npu_sanitizer",
    "NPU_SANITIZER",
)


def test_compile_script_requires_preloaded_cann_environment():
    script = NPU_TOOLS_ROOT / "npu_compute/compile.sh"
    script_text = script.read_text(encoding="utf-8")
    environment = os.environ.copy()
    environment.pop("NPUCOMPUTE_CANN_ROOT", None)
    environment.pop("ASCEND_HOME_PATH", None)

    result = subprocess.run(
        ["bash", str(script)],
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode != 0
    assert "NPUCOMPUTE_CANN_ROOT or ASCEND_HOME_PATH must be set" in result.stderr
    assert "/usr/local/Ascend" not in script_text


def _source_text(root: Path) -> str:
    return "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted(root.rglob("*"))
        if path.is_file()
        and (path.suffix in TEXT_SUFFIXES or path.name == "CMakeLists.txt")
    )


def test_injection_is_an_independent_sibling_component():
    assert (NPU_TOOLS_ROOT / "injection").is_dir()
    assert (NPU_TOOLS_ROOT / "npu_compute").is_dir()
    assert (NPU_TOOLS_ROOT / "npu_sanitizer").is_dir()
    assert not (REPO_ROOT / "npu_compute").exists()
    assert not (REPO_ROOT / "npu_sanitizer").exists()

    public_header = INJECTION_ROOT / "include/injection/injection_hook.h"
    export_header = INJECTION_ROOT / "include/injection/export.h"
    assert public_header.is_file()
    assert export_header.is_file()
    assert "ACL_TOOL_INJECTION_EXPORT" in export_header.read_text(encoding="utf-8")

    injection_text = _source_text(INJECTION_ROOT)
    for product_name in FORBIDDEN_PRODUCT_NAMES:
        assert product_name not in injection_text


def test_consumers_use_the_independent_injection_interface():
    compute_text = _source_text(NPU_TOOLS_ROOT / "npu_compute")
    sanitizer_text = _source_text(NPU_TOOLS_ROOT / "npu_sanitizer")

    assert '"injection/injection_hook.h"' in compute_text
    assert '"injection/injection_hook.h"' in sanitizer_text
    assert '"npu_compute/injection_hook.h"' not in compute_text
    assert '"npu_compute/injection_hook.h"' not in sanitizer_text
    assert "npu_compute_integration_headers" not in sanitizer_text


def test_top_level_build_uses_npu_tools_orchestration():
    root_cmake = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    tools_cmake = (NPU_TOOLS_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

    assert "add_subdirectory(npu_tools)" in root_cmake
    assert "add_subdirectory(npu_compute)" not in root_cmake
    assert "add_subdirectory(npu_sanitizer/" not in root_cmake
    assert "ASC_TOOLS_BUILD_INJECTION" not in root_cmake
    assert "ASC_TOOLS_BUILD_INJECTION" not in tools_cmake
    assert "if(ASC_TOOLS_BUILD_NPU_COMPUTE OR BUILD_NPU_SANITIZER)" in root_cmake
    assert "if(ASC_TOOLS_BUILD_NPU_COMPUTE OR BUILD_NPU_SANITIZER)" in tools_cmake
    assert tools_cmake.index("add_subdirectory(injection)") < tools_cmake.index(
        "add_subdirectory(npu_compute)"
    )
