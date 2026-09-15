# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# Licensed under CANN Open Software License Agreement Version 2.0.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(NPU_COMPUTE_BISHENG bisheng HINTS
  "${NPUCOMPUTE_CANN_ROOT}/../tools/bisheng_compiler/bin"
  "${NPUCOMPUTE_CANN_ROOT}/tools/bisheng_compiler/bin"
  "${ASCEND_CANN_PACKAGE_PATH}/tools/bisheng_compiler/bin"
  "$ENV{ASCEND_HOME_PATH}/tools/bisheng_compiler/bin")
if(NOT NPU_COMPUTE_BISHENG)
  message(FATAL_ERROR "Embedded kernel-end probe requires Bisheng; set NPU_COMPUTE_BISHENG")
endif()
set(assets_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
  OUTPUT "${assets_dir}/kernel_end_assets.h" "${assets_dir}/kernel_end.o" "${assets_dir}/ctrl.bin"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${assets_dir}"
  COMMAND "${NPU_COMPUTE_BISHENG}" -x cce --cce-aicore-only --npu-arch=dav-3510
    -O2 -g -c "${CMAKE_CURRENT_SOURCE_DIR}/probe.cpp" -o "${assets_dir}/kernel_end.o"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/embed_assets.py"
    "${assets_dir}/kernel_end.o" "${assets_dir}/ctrl.bin" "${assets_dir}/kernel_end_assets.h"
  DEPENDS probe.cpp embed_assets.py "${NPU_COMPUTE_BISHENG}"
  VERBATIM)
add_custom_target(npu_compute_kernel_end_assets DEPENDS "${assets_dir}/kernel_end_assets.h")
add_library(npu_compute_kernel_end INTERFACE)
add_dependencies(npu_compute_kernel_end npu_compute_kernel_end_assets)
target_include_directories(npu_compute_kernel_end INTERFACE "${assets_dir}")
# Build intermediates only: no sources, objects or control files are installed.
