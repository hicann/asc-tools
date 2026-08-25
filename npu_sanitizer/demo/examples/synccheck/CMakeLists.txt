set(SYNCCHECK_EXAMPLES
  no_sync
  single_pair
  single_unconsumed
  duplicate_set
  multi_block_isolation
  wait_without_set
  mutex_pair
  mutex_unreleased
  mutex_unlock_without_lock
  mutex_duplicate_lock
  mutex_multi_block_isolation
)

foreach(example IN LISTS SYNCCHECK_EXAMPLES)
  set(target "aclsan_demo_synccheck_${example}")
  add_executable("${target}" "${example}.asc")
  target_link_libraries("${target}" PRIVATE acl_runtime_backend)
  target_compile_options("${target}" PRIVATE
    $<$<COMPILE_LANGUAGE:ASC>:--npu-arch=${CMAKE_ASC_ARCHITECTURES}>
    -g
  )
  set_target_properties("${target}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ACLSAN_DEMO_OUTPUT_DIRECTORY}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
  )
endforeach()
