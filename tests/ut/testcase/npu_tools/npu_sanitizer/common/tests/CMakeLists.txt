# Test-only CANN library providing the directly linked dlog entry points.
# Keep this library in the test build tree; do not install it.
add_library(plog_test_library STATIC plog_test_library.cpp)
target_compile_features(plog_test_library PUBLIC cxx_std_17)
target_include_directories(plog_test_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(plog_test_library PRIVATE Threads::Threads)
set_target_properties(plog_test_library PROPERTIES
  CXX_VISIBILITY_PRESET default
)

add_executable(npu_sanitizer_plog_unit_tests
  aclsan_log_test.cpp
)
target_compile_features(npu_sanitizer_plog_unit_tests PRIVATE cxx_std_17)
target_link_libraries(npu_sanitizer_plog_unit_tests PRIVATE plog_test_library npu_sanitizer_plog)
add_test(NAME npu_sanitizer_plog_unit_tests COMMAND npu_sanitizer_plog_unit_tests)
