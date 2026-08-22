# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
PUBLIC_HEADER = REPO_ROOT / "npu_compute/include/npu_compute/npu_compute.h"
PRODUCT_CMAKE = REPO_ROOT / "npu_compute/CMakeLists.txt"
COMPILE_SCRIPT = REPO_ROOT / "npu_compute/compile.sh"
LIBRARY_SOURCE = REPO_ROOT / "npu_compute/src/npu_compute/npu_compute.cpp"
LIBRARY_CMAKE = REPO_ROOT / "npu_compute/src/npu_compute/CMakeLists.txt"
RANGE_PROFILER_SOURCE = (
    REPO_ROOT / "npu_compute/src/acl_pti/profiling/range_profiler.cpp"
)
SECTION_CONFIG_HEADER = REPO_ROOT / "npu_compute/src/npu_compute/section_config.h"
SECTION_CONFIG_SOURCE = REPO_ROOT / "npu_compute/src/npu_compute/section_config.cpp"
VERSION_SCRIPT = REPO_ROOT / "npu_compute/src/npu_compute/libnpu_compute.map"
CLI_CMAKE = REPO_ROOT / "npu_compute/src/compute_launcher/CMakeLists.txt"
CLI_LAUNCHER = REPO_ROOT / "npu_compute/src/compute_launcher/launcher.cpp"
INJECTION_PATH_HEADER = REPO_ROOT / "npu_compute/src/compute_launcher/injection_path.h"
INJECTION_PATH_SOURCE = (
    REPO_ROOT / "npu_compute/src/compute_launcher/injection_path.cpp"
)


def test_injection_library_declares_lifecycle_exports():
    header = PUBLIC_HEADER.read_text(encoding="utf-8")
    source = LIBRARY_SOURCE.read_text(encoding="utf-8")

    assert header.count('extern "C"') == 2
    assert 'extern "C" NPU_COMPUTE_EXPORT int acltoolInitialize();' in header
    assert 'extern "C" NPU_COMPUTE_EXPORT int acltoolShutdown();' in header

    obsolete_exports = (
        "NpuComputeInit",
        "NpuComputeStartProfiling",
        "NpuComputeInjectionLibraryName",
        "NpuComputeInjectionLibraryPath",
    )
    for symbol in obsolete_exports:
        assert symbol not in header
        assert symbol not in source


def test_injection_library_uses_an_export_allowlist():
    cmake = LIBRARY_CMAKE.read_text(encoding="utf-8")
    assert VERSION_SCRIPT.is_file()
    version_script = VERSION_SCRIPT.read_text(encoding="utf-8")

    assert "${CMAKE_CURRENT_SOURCE_DIR}/.." in cmake
    assert "libnpu_compute.map" in cmake
    assert "--version-script" in cmake
    assert "LINK_DEPENDS" in cmake
    assert "acltoolInitialize;" in version_script
    assert "acltoolShutdown;" in version_script
    assert "local:" in version_script
    assert "*;" in version_script


def test_cli_resolves_injection_path_without_linking_injection_library():
    cmake = CLI_CMAKE.read_text(encoding="utf-8")
    launcher = CLI_LAUNCHER.read_text(encoding="utf-8")

    assert INJECTION_PATH_HEADER.is_file()
    assert INJECTION_PATH_SOURCE.is_file()
    assert "injection_path.cpp" in cmake
    assert "PRIVATE npu_compute_headers npu_compute" not in cmake
    assert 'include "injection_path.h"' in launcher
    assert "ResolveInjectionLibraryPath" in launcher
    assert "NpuComputeInjectionLibraryPath" not in launcher


def test_default_cmake_uses_cann_runtime_and_profapi_for_non_test_builds():
    cmake = PRODUCT_CMAKE.read_text(encoding="utf-8")

    assert "option(NPU_COMPUTE_BUILD_TESTS" in cmake
    assert "set(NPU_COMPUTE_USE_STUB_BACKEND ${NPU_COMPUTE_BUILD_TESTS})" in cmake
    backend_block = cmake.split("add_library(acl_runtime_backend INTERFACE)", 1)[
        1
    ].split("add_subdirectory(src/injection_hook)", 1)[0]
    assert "if(NPU_COMPUTE_USE_STUB_BACKEND)" in backend_block
    stub_block = backend_block.split("if(NPU_COMPUTE_USE_STUB_BACKEND)", 1)[1].split(
        "else()", 1
    )[0]
    cann_block = backend_block.split("else()", 1)[1]

    assert "add_subdirectory(stubs/runtime)" in stub_block
    assert "add_subdirectory(stubs/prof_api)" in stub_block
    assert "acl_runtime_stub" in stub_block
    assert "acl_prof_api_stub" in stub_block

    assert "find_library(NPUCOMPUTE_ACL_RT_LIBRARY" in cann_block
    assert "find_library(NPUCOMPUTE_PROFAPI_LIBRARY" in cann_block
    assert "CANN::acl_rt" in cann_block
    assert "CANN::profapi" in cann_block
    assert "acl_runtime_stub" not in cann_block
    assert "acl_prof_api_stub" not in cann_block


def test_compile_script_uses_current_cann_build_options():
    script = COMPILE_SCRIPT.read_text(encoding="utf-8")

    assert 'BUILD_DIR="${SCRIPT_DIR}/build"' in script
    assert (
        'CANN_ROOT="${NPUCOMPUTE_CANN_ROOT:-/usr/local/Ascend/ascend-toolkit/latest}"'
        in script
    )
    assert '-DNPUCOMPUTE_CANN_ROOT="${CANN_ROOT}"' in script
    assert "-U NPU_COMPUTE_BUILD_CANN_BACKEND" in script
    assert "-U NPU_COMPUTE_BUILD_INTEGRATION_STUBS" in script
    assert "-DNPU_COMPUTE_BUILD_TESTS=OFF" in script
    assert "-DNPU_COMPUTE_BUILD_CANN_BACKEND" not in script
    assert "-DNPU_COMPUTE_BUILD_INTEGRATION_STUBS" not in script
    assert "--target npu_compute npu_compute_cli" in script


def test_replay_waits_after_msprof_start_before_launching_kernel():
    source = RANGE_PROFILER_SOURCE.read_text(encoding="utf-8")

    assert "#include <chrono>" in source
    assert "#include <thread>" in source
    assert "std::this_thread::sleep_for(std::chrono::seconds(5));" in source
    assert source.index("MsprofStart(") < source.index(
        "std::this_thread::sleep_for(std::chrono::seconds(5));"
    )
    assert source.index(
        "std::this_thread::sleep_for(std::chrono::seconds(5));"
    ) < source.index("const int launchResult = launchFunction();")


def test_section_parameter_storage_uses_stable_pointer_vector():
    header = SECTION_CONFIG_HEADER.read_text(encoding="utf-8")
    source = SECTION_CONFIG_SOURCE.read_text(encoding="utf-8")
    compact_header = "".join(header.split())
    compact_source = "".join(source.split())

    assert "std::vector<constchar*>section_pointers_;" in compact_header
    assert "aclptiRangeProfilerSetConfigParamsparams_{};" in compact_header
    assert "params_.sections=section_pointers_.data();" in compact_source
    assert "params_.numSections=section_pointers_.size();" in compact_source
    assert "ParamsDeleter" not in header
    assert "params_->numSection" not in source
