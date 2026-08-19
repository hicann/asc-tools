cmake_minimum_required(VERSION 3.21)

project(ascsan_npu_check LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEFINED ASCSAN_RUNTIME_OUTPUT_DIRECTORY)
    set(ASCSAN_LIBRARY_OUTPUT_DIRECTORY "${ASCSAN_RUNTIME_OUTPUT_DIRECTORY}" CACHE PATH
        "Output directory for sanitizer shared libraries" FORCE)
else()
    set(ASCSAN_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build/lib" CACHE PATH
        "Output directory for sanitizer shared libraries")
endif()
set(ASCSAN_SANITIZER_API_LIBRARY_PATH "${ASCSAN_LIBRARY_OUTPUT_DIRECTORY}/libacl_san.so")
set(ASCSAN_PUBLIC_INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(ASCSAN_NPU_CHECK_INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(ASCSAN_SANITIZER_API_INCLUDE_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/../../sanitizer_api/include")

if(TARGET acl_san)
    add_library(ascsan_imported_sanitizer_api INTERFACE)
    target_link_libraries(ascsan_imported_sanitizer_api INTERFACE acl_san)
else()
    if(NOT EXISTS "${ASCSAN_SANITIZER_API_LIBRARY_PATH}")
        message(FATAL_ERROR
            "libacl_san.so must be built before npu_check: ${ASCSAN_SANITIZER_API_LIBRARY_PATH}")
    endif()

    add_library(ascsan_imported_sanitizer_api SHARED IMPORTED GLOBAL)
    set_target_properties(ascsan_imported_sanitizer_api PROPERTIES
        IMPORTED_LOCATION "${ASCSAN_SANITIZER_API_LIBRARY_PATH}"
    )
endif()

add_library(npu_check SHARED
    subscribe.cpp
)
set_target_properties(npu_check PROPERTIES
    BUILD_WITH_INSTALL_RPATH YES
    CXX_VISIBILITY_PRESET hidden
    INSTALL_RPATH "$ORIGIN"
    LIBRARY_OUTPUT_DIRECTORY "${ASCSAN_LIBRARY_OUTPUT_DIRECTORY}"
    VISIBILITY_INLINES_HIDDEN YES
)
target_include_directories(npu_check PUBLIC
    ${ASCSAN_NPU_CHECK_INCLUDE_DIRECTORY}
    ${ASCSAN_SANITIZER_API_INCLUDE_DIRECTORY}
    ${ASCSAN_PUBLIC_INCLUDE_DIRECTORY}
)
target_compile_options(npu_check PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(npu_check PRIVATE ascsan_imported_sanitizer_api)
