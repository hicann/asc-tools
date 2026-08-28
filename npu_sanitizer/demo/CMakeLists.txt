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
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../sanitizer_api" sanitizer_api)
set_target_properties(acl_san PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
)

set(ACLSAN_DBI_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dbi")
set(ACLSAN_DBI_RUNTIME_SOURCE_DIRECTORY "${CMAKE_BINARY_DIR}/dbi_runtime_sources")
file(MAKE_DIRECTORY "${ACLSAN_DBI_RUNTIME_SOURCE_DIRECTORY}/probes")
foreach(probe_source mte1 mte2 mte3 fixpipe scalar sync)
  configure_file(
    "${ACLSAN_DBI_SOURCE_DIRECTORY}/src/probes/${probe_source}.cpp"
    "${ACLSAN_DBI_RUNTIME_SOURCE_DIRECTORY}/probes/${probe_source}.cpp"
    COPYONLY
  )
endforeach()
foreach(trace_header trace_record.h trace_buffer_abi.h)
  configure_file(
    "${ACLSAN_DBI_SOURCE_DIRECTORY}/include/${trace_header}"
    "${ACLSAN_DBI_RUNTIME_SOURCE_DIRECTORY}/${trace_header}"
    COPYONLY
  )
endforeach()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" npu_check_common)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../npu_check_cli" npu_check_cli)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../npu_check" npu_check)
add_dependencies(npu_check_cli npu_check)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/add" examples_add)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/synccheck" examples_synccheck)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/add_datacopy_stride_oob" examples_add_datacopy_stride_oob)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/padding_register_state" examples_padding_register_state)
