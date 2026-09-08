# Test-only CANN library: the product sink uses its unchanged dlopen/dlsym path.
# Keep this library in the test build tree; do not install it.
add_library(plog_test_library SHARED plog_test_library.cpp)
target_compile_features(plog_test_library PUBLIC cxx_std_17)
target_include_directories(plog_test_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(plog_test_library PRIVATE Threads::Threads)
set_target_properties(plog_test_library PROPERTIES
  OUTPUT_NAME unified_dlog
  CXX_VISIBILITY_PRESET default
)

add_executable(npu_sanitizer_plog_unit_tests
  plog_sink_test.cpp
)
target_compile_features(npu_sanitizer_plog_unit_tests PRIVATE cxx_std_17)
target_link_libraries(npu_sanitizer_plog_unit_tests PRIVATE plog_test_library npu_sanitizer_plog)
add_test(NAME npu_sanitizer_plog_unit_tests COMMAND npu_sanitizer_plog_unit_tests)
