cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_ASC_ARCHITECTURES "dav-3510" CACHE STRING "NPU architecture for ASC examples")
find_package(ASC REQUIRED)

project(aclsan_real_runtime_demo LANGUAGES ASC CXX)

set(NPU_COMPUTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../npu_compute" npu_compute)
if(NOT TARGET npu_compute_integration_headers)
  add_library(npu_compute_integration_headers INTERFACE)
  target_link_libraries(npu_compute_integration_headers INTERFACE
    npu_compute_backend_headers)
endif()

set(ACLSAN_DEMO_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/npu_compute/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}")
set(ACLSAN_BUILD_DEVICE_PROBE_RESOURCES ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../sanitizer_api" sanitizer_api)
set_target_properties(acl_san PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" npu_check_common)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../npu_check_cli" npu_check_cli)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../npu_check" npu_check)
add_dependencies(npu_check_cli npu_check)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/add" examples_add)
