add_executable(aclsan_demo_add_datacopy_stride_oob add_datacopy_stride_oob.asc)
target_link_libraries(aclsan_demo_add_datacopy_stride_oob PRIVATE acl_runtime_backend)
target_compile_options(aclsan_demo_add_datacopy_stride_oob PRIVATE
  $<$<COMPILE_LANGUAGE:ASC>:--npu-arch=${CMAKE_ASC_ARCHITECTURES}>
  -g
)
set_target_properties(aclsan_demo_add_datacopy_stride_oob PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)
