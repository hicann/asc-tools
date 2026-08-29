add_executable(aclsan_demo_datacopy_stride datacopy_stride.asc)
target_link_libraries(aclsan_demo_datacopy_stride PRIVATE acl_runtime_backend)
target_compile_options(aclsan_demo_datacopy_stride PRIVATE
  $<$<COMPILE_LANGUAGE:ASC>:--npu-arch=${CMAKE_ASC_ARCHITECTURES}>
  -g
)
set_target_properties(aclsan_demo_datacopy_stride PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)
