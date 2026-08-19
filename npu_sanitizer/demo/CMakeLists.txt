cmake_minimum_required(VERSION 3.16.0)
project(aclsan_host_stub_demo LANGUAGES CXX)

set(NPU_COMPUTE_BUILD_INTEGRATION_STUBS ON CACHE BOOL "" FORCE)
set(NPU_COMPUTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../npu_compute" npu_compute)

set(ACLSAN_DEMO_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/npu_compute/bin")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../sanitizer_api" sanitizer_api)
set_target_properties(acl_san PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
)

set(ASCSAN_RUNTIME_OUTPUT_DIRECTORY
  "${ACLSAN_DEMO_OUTPUT_DIRECTORY}" CACHE PATH "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/npu_check" npu_check)
add_dependencies(npu_check acl_san)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/npu_check_exec" npu_check_exec)
add_dependencies(npucheck npu_check)

add_executable(aclsan_demo_app demo_app.cpp)
target_compile_features(aclsan_demo_app PRIVATE cxx_std_17)
target_link_libraries(aclsan_demo_app PRIVATE acl_runtime_stub)
set_target_properties(aclsan_demo_app PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)
