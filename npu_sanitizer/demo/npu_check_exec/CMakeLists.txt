cmake_minimum_required(VERSION 3.21)

project(ascsan_npu_check_exec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ASCSAN_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build/bin" CACHE PATH
    "Output directory for sanitizer executables")
set(ASCSAN_SANITIZER_API_INCLUDE_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/../../sanitizer_api/include")

add_executable(npucheck
    main.cpp
)

set_target_properties(npucheck PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    OUTPUT_NAME "npucheck"
    RUNTIME_OUTPUT_DIRECTORY "${ASCSAN_RUNTIME_OUTPUT_DIRECTORY}"
    VISIBILITY_INLINES_HIDDEN YES
)
target_compile_options(npucheck PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(npucheck PRIVATE ${ASCSAN_SANITIZER_API_INCLUDE_DIRECTORY})
